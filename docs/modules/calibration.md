# Sensor Calibration Module

## Overview

The calibration module applies **per-device offsets** to temperature, humidity, and pressure readings. This corrects
systematic bias between different sensor ICs (e.g. BME280 vs HTS221) or individual unit variation. It also carries the
per-unit battery **voltage-divider ratio** (`bat_divider`) used by the battery module.

It is enabled by the `-DHAS_CALIBRATION` build flag and works with **any** sensor module (`HAS_BME280`, `HAS_SHT30`,
`HAS_MKR_ENV`).

**No metrics published** — this module only adjusts readings from other sensor modules.

> **Stage B — calibration is runtime, not compiled in.** Offsets and `bat_divider` are **not** build flags. They live
> in the device's [runtime config store](../ota-calibration-protocol.md#3-store-rémanent-calibration--éventuel-device_id)
> (LittleFS on ESP8266, NVS on ESP32-C3, FlashStorage on SAMD), are **mirrored on the server** per `device_id`, and are
> **re-pushed** by the server after an OTA or a factory reset. The old `-DCALIBRATION_TEMP/HUMI/PRESS_OFFSET` and
> `-DBATTERY_DIVIDER_RATIO` build flags are gone. See [OTA module](ota.md) and the
> [OTA/calibration protocol](../ota-calibration-protocol.md).

**Commands** (on `thermo/{id}/command`):

| Action                | Payload                                                          | Effect                                          |
|-----------------------|------------------------------------------------------------------|-------------------------------------------------|
| `set_offset`          | `{"action":"set_offset","metric":"temp","value":-0.5}`           | Set a sensor offset in the runtime config store |
| `set_calibration`     | `{"action":"set_calibration","key":"bat_divider","value":2.771}` | Set a generic calibration value (divider ratio) |
| `request_calibration` | `{"action":"request_calibration"}`                               | Device publishes its current calibration        |

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

## Runtime calibration (config store)

A production firmware image is **device-agnostic**: a single binary per `(HW_CODE, HW_REV)` serves every unit. It ships
with **no** compiled calibration. Instead, each unit's offsets and `bat_divider` live in the runtime config store:

- **Boot** — the module loads the stored values (defaults: offsets `0.0`, `bat_divider` = the nominal value for the
  `HW_REV`) via the platform persist/restore callbacks.
- **Adjust** — the server (or an admin) sends `set_offset` / `set_calibration`; the new value is written to the store.
- **Mirror** — the server keeps a copy of every unit's calibration, keyed by `device_id`.
- **Re-push** — after an OTA (store normally preserved) or a factory reset / chip swap (store wiped), the device
  reports `cal:0` in its [capabilities](../mqtt-protocol.md); the server re-pushes the mirrored values so the store is
  restored. See [protocol §4.3](../ota-calibration-protocol.md#43-flux-de-re-push-après-ota--reset).

Because the store lives outside the application partition, an OTA (which rewrites only that partition) preserves the
device's calibration — there is nothing to re-flash.

## Command: `set_offset`

```json
{"action":"set_offset","metric":"temp","value":-0.5}
```

| Field    | Type   | Required | Values                           |
|----------|--------|----------|----------------------------------|
| `metric` | string | Yes      | `"temp"`, `"humi"`, or `"press"` |
| `value`  | number | Yes      | Offset to add (-50.0 to +50.0)   |

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
4. Send the `set_offset` command via the server UI or MQTT — the value is stored and mirrored automatically.

## Command: `set_calibration`

Sets a generic, non-offset calibration value. The only key today is `bat_divider`, the battery voltage-divider ratio
(previously the `-DBATTERY_DIVIDER_RATIO` build flag) used by the [battery module](battery.md) to convert the ADC
reading to a cell voltage.

```json
{"action":"set_calibration","key":"bat_divider","value":2.771}
```

| Field   | Type   | Required | Values          | Description                       |
|---------|--------|----------|-----------------|-----------------------------------|
| `key`   | string | Yes      | `"bat_divider"` | Calibration key (extensible)      |
| `value` | number | Yes      | divider ratio   | Value written to the config store |

The device rejects an unsupported key (the platform value callback returns `false`).

## Command: `request_calibration`

The server sends this command to query the current calibration. The device responds by publishing its offsets and
`bat_divider` on the dedicated `calibration` topic (not `ack` — see [protocol §4.2](../ota-calibration-protocol.md#42-rapport-de-calibration-device--serveur)):

```json
{"action":"request_calibration"}
```

**Response** (on `thermo/{id}/calibration`):

```json
{"cal_temp":-1.50,"cal_humi":3.00,"cal_press":-0.30,"bat_divider":2.771}
```

This lets the server **capture** the calibration of an already-tuned unit (bootstrapping the mirror) and audit
calibration across the fleet.

### Integration pattern

The module calls a response callback when the command is received. Platform code provides
this callback to format and publish the response:

```c++
// In setup():
calibration_module_set_response_callback(publish_calibration);

// Callback:
static void publish_calibration() {
    char buf[96];
    calibration_format_response(buf, sizeof(buf));
    network.publish(topics.calibration, buf);
}
```

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

### Store persistence

Offsets and `bat_divider` are persisted through platform callbacks backed by the runtime config store
(`IConfigStore`). The store survives both power loss and an OTA:

| Platform     | Config store backend |
|--------------|----------------------|
| ESP8266      | LittleFS (JSON file) |
| ESP32-C3     | NVS (`Preferences`)  |
| SAMD21 (MKR) | `FlashStorage`       |

At boot, platform code reads the stored values and restores them into the module. When the server pushes a new value,
the persist / value callback writes it back to the store:

```c++
// In setup():
calibration_module_set_persist_callback(on_offsets_changed);   // set_offset  -> store
calibration_module_set_value_callback(on_calibration_value);   // set_calibration -> store
calibration_module_set_offsets(saved_temp, saved_humi, saved_press);
calibration_module_set_bat_divider(saved_bat_divider);
```

If the store is empty (fresh chip, factory reset), the module starts from defaults and reports `cal:0` so the server
re-pushes the mirrored calibration.

## PlatformIO Configuration

### Build Flag

```ini
build_flags = -DHAS_CALIBRATION
```

Note: there are **no** `-DCALIBRATION_*_OFFSET` or `-DBATTERY_DIVIDER_RATIO` flags — those values are runtime store
entries, not build flags.

### Example Environment

Every current battery/sensor env already enables `HAS_CALIBRATION`, e.g. `sensor_8266_bmp80`:

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

// Battery voltage-divider ratio (set via set_calibration, mirrored in the report)
void calibration_module_set_bat_divider(float ratio);
float calibration_get_bat_divider();

// Format the calibration-topic payload (see protocol §4.2)
int calibration_format_response(char* buf, size_t buf_size);

using CalibrationPersistCallback = void (*)(float temp, float humi, float press);
void calibration_module_set_persist_callback(CalibrationPersistCallback cb);

using CalibrationValueCallback = bool (*)(const char* key, float value);
void calibration_module_set_value_callback(CalibrationValueCallback cb);
```

## Safety

- Offsets are clamped to ±50 — extreme values are rejected.
- Humidity is clamped to 0–100% after offset application.
- Setting an offset to 0 effectively disables calibration for that metric.
- On a fresh or wiped store, offsets default to `0.0` and `bat_divider` to the nominal value for the `HW_REV`; the
  device reports `cal:0` until the server re-pushes the mirrored calibration.

## See Also

- [BME280](bme280.md), [SHT30](sht30.md), [MKR ENV](mkr-env.md) — sensor modules this calibration applies to
- [OTA module](ota.md) — device-agnostic images; calibration survives an update
- [OTA/calibration protocol](../ota-calibration-protocol.md) — runtime store, mirror and re-push contract
- [MQTT protocol](../mqtt-protocol.md) — `set_offset` / `set_calibration` command format
- [Architecture](../architecture.md) — module system and feature flags
```
