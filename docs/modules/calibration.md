# Sensor Calibration Module

## Overview

The calibration module applies **per-device offsets** to temperature, humidity, and pressure readings. This corrects
systematic bias between different sensor ICs (e.g. BME280 vs HTS221) or individual unit variation.

It is enabled by the `-DHAS_CALIBRATION` build flag and works with **any** sensor module (`HAS_BME280`, `HAS_SHT30`,
`HAS_MKR_ENV`).

**No metrics published** — this module only adjusts readings from other sensor modules.

**Commands** (on `thermo/{id}/command`):

| Action       | Payload                                                 | Effect                          |
|--------------|---------------------------------------------------------|---------------------------------|
| `set_offset` | `{"action":"set_offset","metric":"temp","value":-0.5}`  | Set calibration offset          |

## Why Calibrate?

Different sensors have different factory tolerances:

| Sensor            | Temperature accuracy | Humidity accuracy | Pressure accuracy |
|-------------------|----------------------|-------------------|-------------------|
| BME280 (Bosch)    | ±1.0°C               | ±3% RH            | ±1.0 hPa          |
| HTS221 (ST)       | ±0.5°C               | ±3.5% RH          | —                 |
| SHT30 (Sensirion) | ±0.2°C               | ±2% RH            | —                 |
| LPS22HB (ST)      | —                    | —                 | ±0.025 hPa        |

Even two BME280 side by side can differ by 0.5–1°C. Add self-heating from the MCU (especially the
MKR WiFi 1010 with its WiFiNINA module), and offsets of 1–2°C are common.

### Common offset sources

1. **Sensor IC tolerance** — factory calibration varies unit to unit
2. **Self-heating** — MCU and WiFi radio warm the PCB, biasing the reading upward
3. **Enclosure effects** — a closed case traps heat and moisture differently
4. **Altitude** — pressure offset for a known reference station

## Build-Time Offsets (platformio.ini)

Known offsets can be baked into the firmware via `-D` flags, so the device starts with the right
calibration from the first boot — no MQTT command needed.

```ini
[env:thermo_kitchen]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DDEVICE_ID='"thermo_kitchen"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_BME280
    -DHAS_CALIBRATION
    -DCALIBRATION_TEMP_OFFSET=-1.5f
    -DCALIBRATION_HUMI_OFFSET=3.0f
```

| Flag                       | Default | Description               |
|----------------------------|---------|---------------------------|
| `CALIBRATION_TEMP_OFFSET`  | `0.0f`  | Temperature offset (°C)   |
| `CALIBRATION_HUMI_OFFSET`  | `0.0f`  | Humidity offset (% RH)    |
| `CALIBRATION_PRESS_OFFSET` | `0.0f`  | Pressure offset (hPa)     |

These defaults are used at boot and after a `calibration_module_reset()`. They can still be
overridden at runtime via the `set_offset` MQTT command. If offset persistence is configured
(RTC, EEPROM), the persisted value takes priority over the build-time default — call
`calibration_module_set_offsets()` in `setup()` to restore saved values.

**Priority** (highest wins):

1. `calibration_module_set_offsets()` — restored from persistent storage at boot
2. `set_offset` MQTT command — runtime adjustment
3. `-DCALIBRATION_*_OFFSET` — build-time default (used when no persistence or fresh device)

## Command: `set_offset`

```json
{"action":"set_offset","metric":"temp","value":-0.5}
```

| Field    | Type   | Required | Values                              |
|----------|--------|:--------:|-------------------------------------|
| `metric` | string |   Yes    | `"temp"`, `"humi"`, or `"press"`    |
| `value`  | number |   Yes    | Offset to add (-50.0 to +50.0)      |

The offset is **added** to the raw sensor reading:

```text
published_value = raw_reading + offset
```

To lower a reading that's too high, use a **negative** offset. Examples:

| Problem                       | Command                                                 |
|-------------------------------|---------------------------------------------------------|
| Reads 1.5°C too high          | `{"action":"set_offset","metric":"temp","value":-1.5}`  |
| Reads 5% RH too low           | `{"action":"set_offset","metric":"humi","value":5.0}`   |
| Pressure 0.3 hPa above ref    | `{"action":"set_offset","metric":"press","value":-0.3}` |
| Reset temperature offset to 0 | `{"action":"set_offset","metric":"temp","value":0}`     |

### Calibration procedure

1. Place the device next to a **reference instrument** (calibrated thermometer, weather station, etc.)
2. Wait 15–30 minutes for thermal equilibrium
3. Note the difference: `offset = reference - device_reading`
4. Send the `set_offset` command via the server UI or MQTT

## Firmware Integration

The `calibration_apply()` function is called in `main.cpp` between the sensor read and the module
contribute step:

```c++
SensorData data = sensor.read();
#ifdef HAS_CALIBRATION
calibration_apply(data);  // offsets applied in-place
#endif
bme280_module_contribute(pb, data);
```

### Offset persistence

Offsets can be persisted across reboots via a platform callback:

- **Deep sleep (ESP8266)**: store in RTC memory alongside the battery ratio
- **Duty-cycle (MKR)**: store in flash/EEPROM
- **No persistence**: offsets are lost on reboot — the server can re-send them on reconnection

```c++
// In setup():
calibration_module_set_persist_callback(on_offsets_changed);
calibration_module_set_offsets(saved_temp, saved_humi, saved_press);
```

## PlatformIO Configuration

### Build Flag

```ini
build_flags = -DHAS_CALIBRATION
```

### Example Environment

```ini
[env:sensor_calibrated]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DDEVICE_ID='"thermo_cal_1"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_BME280
    -DHAS_CALIBRATION
    -DCALIBRATION_TEMP_OFFSET=-1.2f
    -DHAS_SERIAL_DEBUG
```

## Firmware Files

| File                                                 | Role                                         |
|------------------------------------------------------|----------------------------------------------|
| `lib/thermo_core/src/modules/calibration_module.h`   | Module interface (register, apply, get/set)  |
| `lib/thermo_core/src/modules/calibration_module.cpp` | Command handler, offset storage, apply logic |

### Module API

```c++
void calibration_module_register(ModuleRegistry& reg);
void calibration_apply(SensorData& data);

float calibration_get_temp_offset();
float calibration_get_humi_offset();
float calibration_get_press_offset();

void calibration_module_set_offsets(float temp, float humi, float press);
void calibration_module_reset();

using CalibrationPersistCallback = void (*)(float temp, float humi, float press);
void calibration_module_set_persist_callback(CalibrationPersistCallback cb);
```

## Safety

- Offsets are clamped to ±50 — extreme values are rejected.
- Humidity is clamped to 0–100% after offset application.
- Setting an offset to 0 effectively disables calibration for that metric.
- On reboot without persistence, offsets default to 0 (raw readings).
