# mod_ringback - FreeSWITCH 回铃音识别模块

基于 [顶顶通空号识别](https://www.ddrj.com/asr/index.html) 和 [mod_da2](https://www.ddrj.com/asr/mod_da2.html) 的原理，实现的**本地化**回铃音检测 FreeSWITCH 模块。无需依赖外部服务，完全在 FreeSWITCH 内部处理。

## 识别原理

通过分析拨打电话接通之前的**早期媒体 (early media)** 声音，识别以下信号音：

| 信号类型 | 频率 | 时序特征 | 说明 |
|---------|------|----------|------|
| 忙音 (busy) | 450Hz | 响 350ms，停 350ms | 用户忙、占线 |
| 回铃音 (ringback) | 450Hz | 响 1000ms，停 4000ms | 正常振铃 |
| 拥塞音 (congestion) | 450Hz | 响 700ms，停 700ms | 网络忙 |

### 技术实现

1. **频率分析**：使用 Goertzel 算法检测 450Hz 信号（中国/北美电信标准）
2. **能量检测**：区分静音与有音段
3. **时序分析**：根据响/停时长模式区分忙音、回铃音、拥塞音

## 编译

### 方式一：在 FreeSWITCH 源码树中编译

```bash
# 1. 克隆 FreeSWITCH 源码
git clone https://github.com/signalwire/freeswitch.git
cd freeswitch

# 2. 复制本模块到 FreeSWITCH
cp -r /path/to/mod_ringback/freeswitch/mod/applications/mod_ringback src/mod/applications/

# 3. 在 modules.conf 或 modules.conf.in 中添加
echo "applications/mod_ringback" >> build/modules.conf.in

# 4. 编译
./bootstrap.sh && ./configure && make mod_ringback-install
```

### 方式二：独立编译（需 FreeSWITCH 头文件）

```bash
# 指定 FreeSWITCH 源码路径（推荐）
make FS_SRC=/path/to/freeswitch

# 或使用已安装的 FreeSWITCH
make FS_PREFIX=/usr/local/freeswitch

# 或使用 build.sh 脚本
./build.sh /path/to/freeswitch
```

### 方式三：Docker 环境编译

```bash
# 克隆 FreeSWITCH 获取头文件后编译
git clone --depth 1 https://github.com/signalwire/freeswitch.git /tmp/freeswitch
make FS_SRC=/tmp/freeswitch
# 产物: mod_ringback.so
```

## 自动化测试

### 1. 算法单元测试（无需 FreeSWITCH）

测试时序模式匹配和能量检测逻辑：

```bash
make test
# 或
cd test && make test
```

### 2. CI 流水线

项目包含 GitHub Actions 工作流，推送代码后自动执行：

- **单元测试**：算法逻辑测试
- **独立编译**：验证 mod_ringback.so 可成功编译
- **源码树编译**：在 FreeSWITCH 树中编译（可选）

### 3. 集成测试（手动）

需要运行中的 FreeSWITCH 和可用网关：

```bash
# fs_cli 中发起测试呼叫
originate {execute_on_media=start_ringback}sofia/gateway/your_gw/测试号码 &park

# 挂断后检查通道变量
uuid_getvar <uuid> ringback_result
```

### 安装

```bash
# 复制模块
cp mod_ringback.so /usr/local/freeswitch/mod/

# 复制配置（可选）
cp conf/ringback.conf.xml /usr/local/freeswitch/conf/autoload_configs/

# 在 modules.conf.xml 中加载
# <load module="mod_ringback"/>
```

## 使用方法

### 1. 在拨号串中启用

```xml
<!-- 收到早期媒体时启动检测 -->
originate {execute_on_media=start_ringback}sofia/gateway/xxx/13800138000 &park
```

### 2. 收到 183 时启动（推荐）

```xml
originate {ignore_early_media=consume,execute_on_pre_answer=start_ringback}sofia/gateway/xxx/13800138000 &park
```

### 3. Dialplan 配置

```xml
<extension name="ringback_detect">
  <condition field="destination_number" expression="^(\d+)$">
    <action application="export" data="nolocal:execute_on_media=start_ringback"/>
    <action application="bridge" data="sofia/gateway/xxx/$1"/>
  </condition>
</extension>
```

### 4. 通过 API 启动

```bash
# fs_cli 中
uuid_start_ringback <channel-uuid>
```

## 通道变量

| 变量名 | 说明 |
|--------|------|
| ringback_active | 检测已启动时为 "true" |
| ringback_result | 检测结果: busy, ringback, unknown |
| ringback_tone | 信号类型: busy, ringback, unknown |
| ringback_finish_cause | 停止原因: busy, ringback, timeout |

## 可配置参数（通道变量）

| 变量 | 说明 | 默认 |
|------|------|------|
| ringback_maxdetecttime | 最大检测时间(秒) | 60 |
| ringback_autohangup | 检测到忙音时自动挂断 | true |

## 与 mod_da2 的对比

| 特性 | mod_ringback | mod_da2 |
|------|--------------|---------|
| 部署方式 | 本地，无外部依赖 | 需连接 da2 云服务 |
| 识别能力 | 忙音、回铃音、拥塞音 | 含空号、关机等语音提示 |
| 样本库 | 无，仅信号音 | 需样本库匹配 |
| 适用场景 | 基础信号音检测 | 完整空号识别 |

mod_ringback 专注于**信号音**（忙音、回铃音）的本地识别，适合需要快速判断占线状态的场景。如需识别空号、关机等语音提示，建议使用 mod_da2 或类似商业方案。

## 参考

- [顶顶通空号识别介绍](https://www.ddrj.com/asr/index.html)
- [mod_da2 使用说明](https://www.ddrj.com/asr/mod_da2.html)

## 许可证

见 [LICENSE](LICENSE) 文件。
