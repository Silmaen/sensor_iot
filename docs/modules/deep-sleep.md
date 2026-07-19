# Deep Sleep Module

## Overview

The deep sleep module puts the ESP8266 (and ESP32-C3) into its lowest power state between sensor readings. Instead of
staying connected to WiFi and polling on a timer, the device performs a **one-shot** cycle: wake, connect, read,
publish, sleep. This dramatically extends battery life for nodes powered by 18650 cells.

Enabled by the `-DHAS_DEEP_SLEEP` build flag. **Requires `-DHAS_BATTERY`** (battery-powered nodes only).

| Parameter              | Value                                                      |
|------------------------|------------------------------------------------------------|
| Build flag             | `-DHAS_DEEP_SLEEP`                                         |
| Dependency             | `HAS_BATTERY`                                              |
| Platforms              | ESP8266 (D1 Mini), ESP32-C3 (Super Mini)                   |
| Default sleep interval | 300s (5 minutes)                                           |
| Configurable range     | 1-86400s (via MQTT command)                                |
| Deep sleep current     | ~20uA (ESP8266 RTC); board quiescent dominates (see below) |
| Firmware files         | `lib/thermo_platform/src/esp8266/esp_sleep.{h,cpp}`        |

## Hardware Requirement: GPIO16 to RST

The ESP8266 deep sleep wake-up mechanism requires a physical wire connecting **GPIO16 (D0)** to the **RST** pin on the
Wemos D1 Mini. When the internal RTC timer expires, GPIO16 pulls low, which resets the chip and starts a fresh boot
cycle.

![Deep Sleep Wiring](../img/deep-sleep-wiring.svg)

> **Warning**: With D0 wired to RST, the serial upload may fail. Disconnect the D0-RST wire before flashing firmware,
> then reconnect it.

### Pin Connection

| From        | To  | Notes                                    |
|-------------|-----|------------------------------------------|
| D0 (GPIO16) | RST | Single jumper wire, required for wake-up |

## Execution Flow (One-Shot Mode)

Without deep sleep (normal mode), the device stays awake and publishes periodically:

![Continuous mode](../img/deep-sleep-continuous.svg)

With deep sleep, each cycle is a complete boot-to-sleep sequence:

![Deep sleep cycle](../img/deep-sleep-cycle.svg)

Each wake cycle takes approximately 2-5 seconds depending on WiFi/MQTT connection time. The device is asleep for the
remaining duration of the interval.

## RTC Memory

The ESP8266 has a small region of RTC memory that survives deep sleep (but not power-off). The module uses this to
persist the publish interval across sleep cycles, so the device remembers a `set_interval` command without needing to
query the server every wake.

### RTC Data Structure

```c++
struct RtcData {
    uint32_t magic;           // 0xDEADBEEF = valid data
    uint32_t sleep_interval_s;
};
```

- **Magic value** (`0xDEADBEEF`): validates that RTC memory contains real data (not garbage after power-on).
- **RTC slot 64**: user memory area starts at byte 128 (slot 64). The module uses slot 64.
- On first boot (or after power loss), the magic check fails and the default interval (300s) is used.

### Interface

```c++
class ISleep {
public:
    virtual void deep_sleep(uint32_t seconds) = 0;
    virtual bool read_rtc_interval(uint32_t& interval_s) = 0;
    virtual void write_rtc_interval(uint32_t interval_s) = 0;
};
```

The concrete implementation `EspSleep` (in `lib/thermo_platform/src/esp8266/esp_sleep.cpp`) uses the ESP8266 SDK functions
`system_rtc_mem_read()` and `system_rtc_mem_write()`.

### ESP32-C3 additions

On the ESP32-C3 the same RTC region (`RTC_DATA_ATTR` variables — survive deep sleep, not power loss) persists three
things across wake cycles, each guarded by its own magic value:

- the **publish interval** (as above);
- the **battery calibration ratio** (`bat_divider`), so a calibrated node keeps its ratio without re-commissioning;
- the **WiFi BSSID + channel cache**. After a full, signal-sorted scan picks the strongest access point, its BSSID is
  cached so the next wake-ups reconnect **directly** (~0.2 s) instead of re-scanning (~2.5 s) — the scan's extra radio-on
  time would otherwise cost ~35 % of autonomy. A full rescan runs on cold boot, every 12th connect, or whenever a cached
  connect fails, so roaming is still re-evaluated. Implemented in `esp32_network.cpp::connect_wifi()`; see
  [ESP32-C3 config → WiFi & Connectivity](../configs/esp32c3-bme280-bh1750.md#wifi--connectivity).

## Power Savings

Per-cycle MCU current (ESP8266):

| State              | Current Draw | Duration     | Notes                            |
|--------------------|--------------|--------------|----------------------------------|
| Deep sleep         | ~20uA        | 295-298s     | ESP8266 RTC timer only           |
| WiFi connect       | ~70mA        | 1-3s         | DHCP, association                |
| MQTT + sensor read | ~80mA        | 0.5-1s       | Publish + command wait           |
| WiFi TX peak       | ~350mA       | Milliseconds | Brief spikes during transmission |

On a real 2S battery node these MCU figures are **not** what dominates autonomy. On **HW rev 1** the board-level
quiescent draw sets the floor: the **MC78M05BTG** 5V linear regulator (~3 mA Iq) and an **always-on** voltage divider
(~247 µA) draw continuously, even while the MCU sleeps. That is why deep-sleep autonomy is modest despite the ~20 µA
sleep current.

### Autonomy (2S, HW rev 1)

- **BME280 fleet**: **~21 days theoretical**, **~19–20 days measured** (field telemetry, 2026-07) at 300 s intervals.
- **SHT30 / ESP32-C3** configs: no unit deployed yet, so **theoretical only (~21 days)** — no field data.

The full current budget and per-config breakdown live in **[2S power supply](power-2s.md)** — refer to it rather than
duplicating the numbers here.

### Planned: HW rev 2

**HW rev 2** (planned, not yet built) replaces the regulator with an **HT7350** (~4 µA Iq) and **switches the voltage
divider with a MOSFET** so it is only powered during an ADC read — eliminating the ~247 µA permanent drain. Target
autonomy is ~76 days (~4×). The firmware gates the divider-switch pin behind `#if HW_REV >= 2`.

## MQTT Integration

### Interval Configuration

The server can change the sleep/publish interval via the `command` topic:

```json
{
  "action": "set_interval",
  "value": 600
}
```

On receiving this command, the device:

1. Updates the publish interval in RAM.
2. Writes the new interval to RTC memory (persists across sleep cycles).
3. Enters deep sleep with the new interval.

### Retained Commands

Because the device is asleep most of the time, it cannot receive MQTT commands in real-time. The server should publish
commands with the **retain** flag. On each wake cycle, the device:

1. Connects to MQTT and subscribes to `{type}/{id}/command`.
2. Waits `MQTT_COMMAND_WAIT_MS` (2000ms) for retained messages.
3. Processes any received commands before publishing sensor data.

### Capabilities

The `request_capabilities` command is handled during the command wait window. The device responds with its capabilities
and then continues the normal publish-and-sleep cycle.

## PlatformIO Configuration

### Build Flags

```ini
build_flags =
    -DHAS_DEEP_SLEEP
    -DHAS_BATTERY    ; required dependency
```

### Example Environments

From `platformio.ini`. Production images carry **no** `-DDEVICE_ID` — the `device_id` is provisioned once over serial
(see [OTA module](ota.md#first-provisioning)). ESP8266 `sensor_8266_bmp80`:

```ini
[env:sensor_8266_bmp80]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHW_CODE='"E8BMEBAT"'
    -DHW_REV=1
    -DHAS_BME280
    -DHAS_BATTERY
    -DHAS_DEEP_SLEEP
    -DHAS_CALIBRATION
    -DHAS_OTA
```

ESP32-C3 `sensor_c3_bme280_bh1750` (BME280 + BH1750 lux):

```ini
[env:sensor_c3_bme280_bh1750]
extends = common_esp32c3
build_flags =
    ${common_esp32c3.build_flags}
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHW_CODE='"C3BMELUX"'
    -DHW_REV=1
    -DHAS_BME280
    -DHAS_BH1750
    -DHAS_BATTERY
    -DHAS_DEEP_SLEEP
    -DHAS_CALIBRATION
    -DHAS_OTA
```

Build / upload:

```bash
pio run -e sensor_8266_bmp80              # build ESP8266 battery node
pio run -e sensor_8266_bmp80 -t upload    # flash (disconnect the D0-RST wire first)
pio run -e sensor_c3_bme280_bh1750        # build ESP32-C3 node
```

## Firmware Files

| File                                          | Role                                        |
|-----------------------------------------------|---------------------------------------------|
| `lib/thermo_platform/src/esp8266/esp_sleep.*` | `EspSleep`: ESP8266 deep sleep + RTC memory |
| `lib/thermo_platform/src/esp32/esp32_sleep.*` | ESP32-C3 deep sleep (`RTC_DATA_ATTR`)       |
| `lib/thermo_core/src/interfaces/i_sleep.h`    | `ISleep` interface + `RtcData` struct       |
| `lib/thermo_core/src/config.h`                | `DEFAULT_SLEEP_INTERVAL_S`, `RTC_MAGIC`     |
| `src/main.cpp`                                | `#ifdef HAS_DEEP_SLEEP` integration blocks  |

## Configuration Constants

| Constant                   | Value      | File            | Description                |
|----------------------------|------------|-----------------|----------------------------|
| `DEFAULT_SLEEP_INTERVAL_S` | 300        | `config.h`      | Default sleep duration (s) |
| `RTC_MAGIC`                | 0xDEADBEEF | `config.h`      | RTC memory validity marker |
| `RTC_SLOT`                 | 64         | `esp_sleep.cpp` | RTC memory slot number     |
| `MQTT_COMMAND_WAIT_MS`     | 2000       | `config.h`      | Retained command wait (ms) |

## See Also

- [Battery](battery.md) — required dependency for deep sleep mode
- [2S power supply](power-2s.md) — hardware power chain for battery nodes
- [MQTT protocol](../mqtt-protocol.md) — one-shot mode command handling
- [Architecture](../architecture.md) — continuous vs one-shot execution modes
- Configs using this module: [ESP+BMP280+Battery](../configs/esp-bmp280-battery.md),
  [ESP+SHT30+Battery](../configs/esp-sht30-battery.md)
