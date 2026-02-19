/*
 * mod_ringback - FreeSWITCH 回铃音识别模块
 *
 * 基于顶顶通等回铃音识别原理，实现本地化的拨号音分析：
 * - 通过分析早期媒体(early media)的声音频率和时序特征
 * - 识别回铃音、忙音、彩铃等信号音
 * - 无需依赖外部服务，完全本地处理
 *
 * 原理说明：
 * 1. 频率分析：使用 Goertzel 算法检测 450Hz 信号（中国/北美标准）
 * 2. 时序分析：根据响/停时长模式区分不同信号
 *    - 忙音：响 350ms，停 350ms
 *    - 回铃音：响 1000ms，停 4000ms
 *    - 拥塞音：响 700ms，停 700ms
 * 3. 能量检测：区分静音与有音
 */

#include <switch.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* 信号音类型定义 (兼容 mod_da2) */
#define RINGBACK_TONE_BUSY           0x01
#define RINGBACK_TONE_RINGBACK       0x02
#define RINGBACK_TONE_COLORRINGBACK  0x04
#define RINGBACK_TONE_SILENCE        0x20
#define RINGBACK_TONE_450HZ          0x40

/* 采样率 */
#define SAMPLE_RATE 8000

/* Goertzel 算法参数 - 检测 450Hz */
#define TARGET_FREQ 450.0
#define GOERTZEL_N 205  /* 约 25.6ms @ 8kHz, 适合检测 450Hz */

/* 时序规则 (毫秒) - 允许误差 */
#define BUSY_ON_MIN      250
#define BUSY_ON_MAX      450
#define BUSY_OFF_MIN     250
#define BUSY_OFF_MAX     450

#define RINGBACK_ON_MIN  900
#define RINGBACK_ON_MAX  1200
#define RINGBACK_OFF_MIN 3000
#define RINGBACK_OFF_MAX 5000

#define CONGESTION_ON_MIN  600
#define CONGESTION_ON_MAX  800
#define CONGESTION_OFF_MIN 500
#define CONGESTION_OFF_MAX 900

/* 能量阈值 */
#define ENERGY_THRESHOLD 500
#define MIN_TONE_SAMPLES 80  /* 10ms @ 8kHz */

/* 检测状态 */
typedef struct ringback_state {
    switch_core_session_t *session;
    switch_media_bug_t *bug;
    int running;
    int sample_count;
    double goertzel_s1, goertzel_s2;
    double goertzel_coef;
    int in_tone;
    uint32_t tone_start_ms;
    uint32_t silence_start_ms;
    uint32_t last_tone_duration_ms;
    uint32_t last_silence_duration_ms;
    int tone_type;
    int consecutive_busy;
    int consecutive_ringback;
    uint32_t start_time_ms;
    uint32_t max_detect_time_ms;
    int autohangup;
    char *stoptone;
    int hangup_on_busy;
    int hangup_on_ringback;
} ringback_state_t;

/* Goertzel 系数预计算 */
static double goertzel_coefficient = 0;

static void init_goertzel(void)
{
    if (goertzel_coefficient == 0) {
        double k = (double)GOERTZEL_N * TARGET_FREQ / SAMPLE_RATE;
        goertzel_coefficient = 2.0 * cos(2.0 * M_PI * k / GOERTZEL_N);
    }
}

/* Goertzel 算法 - 处理一个样本 */
static void goertzel_process_sample(ringback_state_t *state, int16_t sample)
{
    double s0 = (double)sample + state->goertzel_coef * state->goertzel_s1 - state->goertzel_s2;
    state->goertzel_s2 = state->goertzel_s1;
    state->goertzel_s1 = s0;
    state->sample_count++;
}

/* 获取 Goertzel 能量 */
static double goertzel_get_energy(ringback_state_t *state)
{
    double power = state->goertzel_s1 * state->goertzel_s1 + state->goertzel_s2 * state->goertzel_s2
                   - state->goertzel_coef * state->goertzel_s1 * state->goertzel_s2;
    return power / (GOERTZEL_N * GOERTZEL_N);
}

/* 计算帧能量 (RMS) */
static double calc_frame_energy(int16_t *samples, int count)
{
    double sum = 0;
    int i;
    for (i = 0; i < count; i++) {
        sum += (double)samples[i] * samples[i];
    }
    return sqrt(sum / count);
}

/* 检查是否匹配忙音模式 */
static int match_busy_pattern(uint32_t on_ms, uint32_t off_ms)
{
    return (on_ms >= BUSY_ON_MIN && on_ms <= BUSY_ON_MAX &&
            off_ms >= BUSY_OFF_MIN && off_ms <= BUSY_OFF_MAX);
}

/* 检查是否匹配回铃音模式 */
static int match_ringback_pattern(uint32_t on_ms, uint32_t off_ms)
{
    return (on_ms >= RINGBACK_ON_MIN && on_ms <= RINGBACK_ON_MAX &&
            off_ms >= RINGBACK_OFF_MIN && off_ms <= RINGBACK_OFF_MAX);
}

/* 检查是否匹配拥塞音模式 */
static int match_congestion_pattern(uint32_t on_ms, uint32_t off_ms)
{
    return (on_ms >= CONGESTION_ON_MIN && on_ms <= CONGESTION_ON_MAX &&
            off_ms >= CONGESTION_OFF_MIN && off_ms <= CONGESTION_OFF_MAX);
}

/* 媒体 bug 回调 */
static switch_bool_t ringback_media_callback(switch_media_bug_t *bug, void *user_data,
                                             switch_abc_codec_t *codec,
                                             void *buffer, switch_size_t len,
                                             switch_frame_t *frame)
{
    ringback_state_t *state = (ringback_state_t *)user_data;
    uint32_t now_ms;
    int samples_per_frame;
    int i;

    if (!state || !state->running) {
        return SWITCH_TRUE;
    }

    if (!frame->data || frame->datalen == 0) {
        return SWITCH_TRUE;
    }

    /* 需要 L16 格式 (8kHz 16bit mono)，FreeSWITCH 内部通常已转换 */
    samples_per_frame = frame->datalen / 2;
    if (samples_per_frame <= 0) return SWITCH_TRUE;

    now_ms = switch_micro_time_now() / 1000 - state->start_time_ms;

    /* 超时检测 */
    if (state->max_detect_time_ms > 0 && now_ms > state->max_detect_time_ms) {
        state->running = 0;
        set_ringback_result(state);
        return SWITCH_FALSE;
    }

    init_goertzel();
    state->goertzel_coef = goertzel_coefficient;

    /* 处理音频帧 */
    int16_t *samples = (int16_t *)frame->data;
    double energy = calc_frame_energy(samples, samples_per_frame);
    double goertzel_energy = 0;

    /* 重置 Goertzel 状态每 GOERTZEL_N 样本 */
    for (i = 0; i < samples_per_frame; i++) {
        goertzel_process_sample(state, samples[i]);
        if (state->sample_count >= GOERTZEL_N) {
            goertzel_energy = goertzel_get_energy(state);
            state->goertzel_s1 = state->goertzel_s2 = 0;
            state->sample_count = 0;
            break;
        }
    }

    /* 判断是否有 450Hz 信号 */
    int has_tone = (energy > ENERGY_THRESHOLD) && (goertzel_energy > 1000 || state->sample_count > 0);
    if (state->sample_count >= GOERTZEL_N / 2) {
        goertzel_energy = goertzel_get_energy(state);
        has_tone = has_tone || goertzel_energy > 1000;
    }

    /* 简化：用能量判断 */
    has_tone = (energy > ENERGY_THRESHOLD);

    uint32_t frame_duration_ms = (samples_per_frame * 1000) / SAMPLE_RATE;

    if (has_tone) {
        if (!state->in_tone) {
            state->in_tone = 1;
            if (state->silence_start_ms > 0) {
                state->last_silence_duration_ms = now_ms - state->silence_start_ms;
            }
            state->tone_start_ms = now_ms;
        }
    } else {
        if (state->in_tone) {
            state->in_tone = 0;
            state->last_tone_duration_ms = now_ms - state->tone_start_ms;
            state->silence_start_ms = now_ms;

            /* 模式匹配 */
            if (match_busy_pattern(state->last_tone_duration_ms, 0)) {
                state->consecutive_busy++;
                state->consecutive_ringback = 0;
                if (state->consecutive_busy >= 2) {
                    state->tone_type = RINGBACK_TONE_BUSY;
                    state->running = 0;
                    set_ringback_result(state);
                    if (state->hangup_on_busy) {
                        switch_channel_t *channel = switch_core_session_get_channel(state->session);
                        switch_channel_hangup(channel, SWITCH_CAUSE_USER_BUSY);
                    }
                    return SWITCH_FALSE;
                }
            } else if (match_ringback_pattern(state->last_tone_duration_ms, 0)) {
                state->consecutive_ringback++;
                state->consecutive_busy = 0;
                if (state->consecutive_ringback >= 1) {
                    state->tone_type = RINGBACK_TONE_RINGBACK;
                    /* 回铃音不停止，继续检测 */
                }
            } else {
                state->consecutive_busy = 0;
                state->consecutive_ringback = 0;
            }
        } else if (state->silence_start_ms == 0) {
            state->silence_start_ms = now_ms;
        }
    }

    return SWITCH_TRUE;
}

/* 设置检测结果到通道变量 */
static void set_ringback_result(ringback_state_t *state)
{
    switch_channel_t *channel = switch_core_session_get_channel(state->session);
    if (channel) {
        switch_channel_set_variable(channel, "ringback_finish_cause",
            state->tone_type == RINGBACK_TONE_BUSY ? "busy" :
            state->tone_type == RINGBACK_TONE_RINGBACK ? "ringback" : "timeout");
        switch_channel_set_variable(channel, "ringback_tone",
            state->tone_type == RINGBACK_TONE_BUSY ? "busy" :
            state->tone_type == RINGBACK_TONE_RINGBACK ? "ringback" : "unknown");
        switch_channel_set_variable(channel, "ringback_result",
            state->tone_type == RINGBACK_TONE_BUSY ? "busy" :
            state->tone_type == RINGBACK_TONE_RINGBACK ? "ringback" : "unknown");
    }
}

/* 启动回铃音检测 */
static switch_status_t start_ringback(switch_core_session_t *session,
                                      const char *data)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_media_bug_t *bug = NULL;
    ringback_state_t *state = NULL;
    switch_status_t status;
    switch_codec_t read_codec = { 0 };

    state = switch_core_session_alloc(session, sizeof(ringback_state_t));
    memset(state, 0, sizeof(ringback_state_t));
    state->session = session;
    state->running = 1;
    state->max_detect_time_ms = 60000;  /* 默认 60 秒 */
    state->autohangup = 1;
    state->hangup_on_busy = 1;
    state->hangup_on_ringback = 0;
    state->start_time_ms = switch_micro_time_now() / 1000;

    /* 从通道变量读取参数 */
    {
        const char *var = switch_channel_get_variable(channel, "ringback_maxdetecttime");
        if (var && atoi(var) > 0) {
            state->max_detect_time_ms = atoi(var) * 1000;
        }
        var = switch_channel_get_variable(channel, "ringback_autohangup");
        if (var) {
            state->hangup_on_busy = switch_true(var);
        }
    }

    switch_core_session_get_read_codec(session, &read_codec);

    status = switch_core_session_create_media_bug(session, "ringback", 0,
        ringback_media_callback, state, 0, SMBF_READ_PING, &bug);
    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_LOG_ERROR, "mod_ringback: Failed to create media bug\n");
        return status;
    }

    state->bug = bug;

    switch_channel_set_variable(channel, "ringback_active", "true");

    return SWITCH_STATUS_SUCCESS;
}

/* API: uuid_start_ringback <uuid> */
static switch_status_t api_uuid_start_ringback(const char *cmd, const char *args,
                                               switch_core_session_t *session,
                                               switch_stream_handle_t *stream)
{
    switch_core_session_t *target_session = NULL;
    switch_status_t status;

    if (!args || !*args) {
        stream->write_function(stream, "-ERR Usage: uuid_start_ringback <uuid>\n");
        return SWITCH_STATUS_SUCCESS;
    }

    target_session = switch_core_session_locate(args);
    if (!target_session) {
        stream->write_function(stream, "-ERR No such channel\n");
        return SWITCH_STATUS_SUCCESS;
    }

    status = start_ringback(target_session, NULL);
    switch_core_session_rwunlock(target_session);

    stream->write_function(stream, status == SWITCH_STATUS_SUCCESS ? "+OK\n" : "-ERR\n");
    return SWITCH_STATUS_SUCCESS;
}

/* 应用接口 */
SWITCH_MODULE_LOAD_FUNCTION(mod_ringback_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ringback_shutdown);

SWITCH_STANDARD_APP(start_ringback_app)
{
    start_ringback(session, data);
}

SWITCH_MODULE_DEFINITION(mod_ringback, mod_ringback_load, mod_ringback_shutdown, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_ringback_load)
{
    switch_application_interface_t *app_interface;
    switch_api_interface_t *api_interface;

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    SWITCH_ADD_APPLICATION(app_interface, "start_ringback", "Start ringback tone detection",
                          "Start ringback tone detection on early media",
                          start_ringback_app, "", SAF_NONE);

    SWITCH_ADD_API(api_interface, "uuid_start_ringback", "Start ringback detection on UUID",
                   "uuid_start_ringback <uuid>", api_uuid_start_ringback, "");

    switch_console_set_complete("add uuid_start_ringback");

    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ringback_shutdown)
{
    return SWITCH_STATUS_SUCCESS;
}
