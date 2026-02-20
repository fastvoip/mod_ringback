# mod_ringback - FreeSWITCH Ringback Tone Detection Module

> [中文](README.md)

A **local** ringback tone detection module for FreeSWITCH, based on the principles of [Dingdingtong ASR](https://www.ddrj.com/asr/index.html) and [mod_da2](https://www.ddrj.com/asr/mod_da2.html). No external services required; all processing runs inside FreeSWITCH.

---

## Build

### Method 1: Build within FreeSWITCH Source Tree

```bash
# 1. Clone FreeSWITCH source
git clone https://github.com/signalwire/freeswitch.git
cd freeswitch

# 2. Copy this module into FreeSWITCH
cp -r /path/to/mod_ringback/freeswitch/mod/applications/mod_ringback src/mod/applications/

# 3. Add to modules.conf or modules.conf.in
echo "applications/mod_ringback" >> build/modules.conf.in

# 4. Build and install
./bootstrap.sh && ./configure && make mod_ringback-install
```

### Method 2: Standalone Build (requires FreeSWITCH headers)

```bash
# Specify FreeSWITCH source path (recommended)
make FS_SRC=/path/to/freeswitch

# Or use installed FreeSWITCH
make FS_PREFIX=/usr/local/freeswitch

# Or use build script
./build.sh /path/to/freeswitch
```

### Method 3: Docker / No Local FreeSWITCH

```bash
git clone --depth 1 https://github.com/signalwire/freeswitch.git /tmp/freeswitch
make FS_SRC=/tmp/freeswitch
# Output: mod_ringback.so
```

### Install

```bash
# Copy module to FreeSWITCH
cp mod_ringback.so /usr/local/freeswitch/mod/

# Copy config (optional)
cp conf/ringback.conf.xml /usr/local/freeswitch/conf/autoload_configs/

# Load in conf/autoload_configs/modules.conf.xml
# <load module="mod_ringback"/>
```

---

## Usage

### 1. Enable in Dial String (start when early media received)

```bash
originate {execute_on_media=start_ringback}sofia/gateway/your_gateway/13800138000 &park
```

### 2. Start on 183 (recommended for lines with 183)

```bash
originate {ignore_early_media=consume,execute_on_pre_answer=start_ringback}sofia/gateway/your_gateway/13800138000 &park
```

### 3. Dialplan Configuration

```xml
<extension name="ringback_detect">
  <condition field="destination_number" expression="^(\d+)$">
    <action application="export" data="nolocal:execute_on_media=start_ringback"/>
    <action application="bridge" data="sofia/gateway/your_gateway/$1"/>
  </condition>
</extension>
```

### 4. Start via API (by channel UUID)

```bash
# In fs_cli
uuid_start_ringback <channel-uuid>
```

### 5. Custom Parameters (channel variables)

```bash
# Max detect 30 seconds, do not auto-hangup on busy
originate {execute_on_media=start_ringback,ringback_maxdetecttime=30,ringback_autohangup=false}sofia/gateway/xxx/number &park
```

### Channel Variables

| Variable | Description |
|----------|-------------|
| ringback_active | "true" when detection is active |
| ringback_result | Result: busy, ringback, unknown |
| ringback_tone | Tone type: busy, ringback, unknown |
| ringback_finish_cause | Stop reason: busy, ringback, timeout |

### Configurable Parameters (channel variables)

| Variable | Description | Default |
|----------|-------------|---------|
| ringback_maxdetecttime | Max detection time (seconds) | 60 |
| ringback_autohangup | Auto-hangup on busy | true |

---

## Detection Principle

Analyzes **early media** audio before call answer to detect:

| Tone Type | Frequency | Pattern | Description |
|-----------|-----------|---------|-------------|
| Busy | 450Hz | 350ms on, 350ms off | User busy |
| Ringback | 450Hz | 1000ms on, 4000ms off | Normal ringing |
| Congestion | 450Hz | 700ms on, 700ms off | Network busy |

### Implementation

1. **Frequency analysis**: Goertzel algorithm for 450Hz (China/North America standard)
2. **Energy detection**: Distinguish silence vs. tone
3. **Pattern analysis**: Match on/off duration to identify tone type

---

## Automated Testing

```bash
# Unit tests (no FreeSWITCH required)
make test
```

---

## Comparison with mod_da2

| Feature | mod_ringback | mod_da2 |
|---------|--------------|---------|
| Deployment | Local, no external deps | Requires da2 cloud service |
| Detection | Busy, ringback, congestion | + Power off, invalid number, etc. |
| Sample DB | None, tone only | Sample library required |
| Use case | Basic tone detection | Full number status detection |

---

## References

- [Dingdingtong ASR](https://www.ddrj.com/asr/index.html)
- [mod_da2 Documentation](https://www.ddrj.com/asr/mod_da2.html)

## License

See [LICENSE](LICENSE).
