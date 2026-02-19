/*
 * mod_ringback 算法单元测试
 * 测试时序模式匹配和能量检测逻辑，无需 FreeSWITCH 依赖
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define SAMPLE_RATE 8000
#define TARGET_FREQ 450.0
#define GOERTZEL_N 205

/* 时序规则 (与 mod_ringback.c 一致) */
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
#define ENERGY_THRESHOLD 500

static int tests_run = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { tests_failed++; printf("FAIL: %s\n", msg); } \
    else { printf("OK: %s\n", msg); } \
} while(0)

static int match_busy_pattern(uint32_t on_ms, uint32_t off_ms)
{
    return (on_ms >= BUSY_ON_MIN && on_ms <= BUSY_ON_MAX &&
            off_ms >= BUSY_OFF_MIN && off_ms <= BUSY_OFF_MAX);
}

static int match_ringback_pattern(uint32_t on_ms, uint32_t off_ms)
{
    return (on_ms >= RINGBACK_ON_MIN && on_ms <= RINGBACK_ON_MAX &&
            off_ms >= RINGBACK_OFF_MIN && off_ms <= RINGBACK_OFF_MAX);
}

static int match_congestion_pattern(uint32_t on_ms, uint32_t off_ms)
{
    return (on_ms >= CONGESTION_ON_MIN && on_ms <= CONGESTION_ON_MAX &&
            off_ms >= CONGESTION_OFF_MIN && off_ms <= CONGESTION_OFF_MAX);
}

static double calc_frame_energy(int16_t *samples, int count)
{
    double sum = 0;
    for (int i = 0; i < count; i++) {
        sum += (double)samples[i] * samples[i];
    }
    return sqrt(sum / count);
}

/* 生成 450Hz 正弦波样本 */
static void generate_450hz_tone(int16_t *buf, int samples, int amplitude)
{
    for (int i = 0; i < samples; i++) {
        double t = (double)i / SAMPLE_RATE;
        buf[i] = (int16_t)(amplitude * sin(2 * M_PI * TARGET_FREQ * t));
    }
}

/* 生成静音 */
static void generate_silence(int16_t *buf, int samples)
{
    memset(buf, 0, samples * sizeof(int16_t));
}

int main(void)
{
    printf("=== mod_ringback 算法单元测试 ===\n\n");

    /* 1. 忙音模式匹配 */
    ASSERT(match_busy_pattern(350, 350) == 1, "忙音 350/350 应匹配");
    ASSERT(match_busy_pattern(300, 300) == 1, "忙音 300/300 应匹配");
    ASSERT(match_busy_pattern(400, 400) == 1, "忙音 400/400 应匹配");
    ASSERT(match_busy_pattern(200, 350) == 0, "忙音 200/350 不应匹配(太短)");
    ASSERT(match_busy_pattern(500, 350) == 0, "忙音 500/350 不应匹配(太长)");

    /* 2. 回铃音模式匹配 */
    ASSERT(match_ringback_pattern(1000, 4000) == 1, "回铃音 1000/4000 应匹配");
    ASSERT(match_ringback_pattern(900, 3500) == 1, "回铃音 900/3500 应匹配");
    ASSERT(match_ringback_pattern(350, 350) == 0, "忙音不应匹配回铃音");

    /* 3. 拥塞音模式匹配 */
    ASSERT(match_congestion_pattern(700, 700) == 1, "拥塞音 700/700 应匹配");
    ASSERT(match_congestion_pattern(650, 600) == 1, "拥塞音 650/600 应匹配");

    /* 4. 能量检测 - 450Hz 信号应有足够能量 */
    {
        int16_t buf[320];  /* 40ms @ 8kHz */
        generate_450hz_tone(buf, 320, 8000);
        double energy = calc_frame_energy(buf, 320);
        ASSERT(energy > ENERGY_THRESHOLD, "450Hz 信号能量应超过阈值");
    }

    /* 5. 能量检测 - 静音应低于阈值 */
    {
        int16_t buf[320];
        generate_silence(buf, 320);
        double energy = calc_frame_energy(buf, 320);
        ASSERT(energy < ENERGY_THRESHOLD, "静音能量应低于阈值");
    }

    printf("\n=== 结果: %d/%d 通过 ===\n", tests_run - tests_failed, tests_run);
    return tests_failed ? 1 : 0;
}
