# Light Sensor Module

## Overview

The light module reads an **analog photoresistor** (Seeedstudio Grove-compatible light sensor) and publishes a relative
light level as a percentage. It is enabled by the `-DHAS_LIGHT` build flag in `platformio.ini`.

**Metrics published** (on `thermo/{id}/sensors`):

| Metric  | Unit | Description                                   |
|---------|------|-----------------------------------------------|
| `light` | %    | Relative light level (0 = dark, 100 = bright) |

## Sensor Specifications

| Parameter      | Value                                       |
|----------------|---------------------------------------------|
| Sensor type    | Photoresistor (LDR) in voltage divider      |
| Output         | Analog voltage, 0–VCC proportional to light |
| Supply voltage | 3.3–5V                                      |
| Interface      | Single analog pin                           |
| Response time  | 20–30 ms                                    |
| Spectral peak  | ~540 nm (green, matches human eye)          |

> **Note:** This sensor is **not calibrated** to lux. The published percentage represents a relative
> reading: 0% = very dark, 100% = maximum light the sensor can detect. For calibrated lux
> measurement, use the MKR ENV Shield (`HAS_MKR_ENV`).

## Wiring

### ESP8266 (D1 Mini)

| D1 Mini Pin | GPIO | Sensor Pin | Notes         |
|-------------|------|------------|---------------|
| A0          | ADC0 | SIG        | Analog output |
| 3V3         | —    | VCC        | 3.3V supply   |
| GND         | —    | GND        | Ground        |

> **Important:** The ESP8266 has a single ADC pin (A0). If `HAS_BATTERY` is also enabled,
> A0 is already used for battery voltage. In that case, the light sensor requires an external
> analog multiplexer or a different MCU (ESP32 with multiple ADC channels).

### ESP32 / ESP32 CAM

| ESP32 Pin | GPIO   | Sensor Pin | Notes                         |
|-----------|--------|------------|-------------------------------|
| GPIO34    | ADC1_6 | SIG        | Analog input (input-only pin) |
| 3V3       | —      | VCC        | 3.3V supply                   |
| GND       | —      | GND        | Ground                        |

The ESP32 has multiple ADC channels — the light sensor can coexist with battery monitoring.

## ADC Conversion

The firmware converts the raw ADC reading to a percentage using linear scaling:

```text
light_pct = raw * 100 / adc_max
```

| Platform | ADC bits | adc_max | Example: raw=512 |
|----------|----------|---------|------------------|
| ESP8266  | 10       | 1023    | 50%              |
| ESP32    | 12       | 4095    | 12%              |

The conversion is done by `adc_to_light_pct()` in the module library, called from `main.cpp`
after reading the ADC.

## PlatformIO Configuration

### Build Flag

```ini
build_flags = -DHAS_LIGHT
```

### Example Environment

> **Note:** No shipping environment currently enables `-DHAS_LIGHT` — this analog module is not part
> of any deployed configuration. The BH1750 (`HAS_BH1750`, I2C) is the calibrated-lux alternative used
> on the ESP32-C3 nodes. The block below is illustrative only.

A Stage-B-correct env would look like this — one image per `(HW_CODE, HW_REV)`, **no** `-DDEVICE_ID`
(identity is provisioned at runtime, see below):

```ini
[env:sensor_c3_light]
extends = common_esp32c3
build_flags =
    ${common_esp32c3.build_flags}
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHW_CODE='"C3LIGHT0"'
    -DHW_REV=1
    -DHAS_LIGHT
    -DHAS_OTA
```

> **Identity (Stage B):** production images carry no compiled `device_id`. On first boot the device
> enters serial provisioning — assign its identity once with `provision <id>` (see
> [OTA](ota.md#first-provisioning)). `-DDEVICE_ID` survives only on dev builds (`-DHAS_SERIAL_DEBUG`)
> as a store seed.

## Firmware Files

| File                                                  | Role                                  |
|-------------------------------------------------------|---------------------------------------|
| `lib/thermo_core/src/modules/light_module.h`          | Module interface + ADC conversion     |
| `lib/thermo_core/src/modules/light_module.cpp`        | Module logic (register, contribute)   |
| `lib/thermo_platform/src/light_adc.h` _(to create)_   | Hardware driver header (platform ADC) |
| `lib/thermo_platform/src/light_adc.cpp` _(to create)_ | Hardware driver (analogRead + max)    |

### Module API

```c++
void light_module_register(ModuleRegistry& reg);
void light_module_contribute(PayloadBuilder& pb, uint8_t light_pct);
uint8_t adc_to_light_pct(uint16_t raw, uint16_t adc_max);
```

## MQTT Payload

When active alongside other modules, the device publishes on `thermo/{device_id}/sensors`:

```json
{"temp":22.5,"humi":48.3,"light":65}
```

## Troubleshooting

| Symptom            | Cause                       | Fix                                         |
|--------------------|-----------------------------|---------------------------------------------|
| Always reads 0%    | Wrong ADC pin or not wired  | Check wiring and pin definition in config.h |
| Always reads 100%  | VCC connected to signal pin | Verify SIG goes to ADC, not VCC             |
| Erratic readings   | Long wire or no decoupling  | Add 100nF cap near sensor, keep wires short |
| Conflicts with bat | A0 shared on ESP8266        | Use ESP32 or add analog multiplexer         |

## See Also

- [MKR ENV](mkr-env.md) — alternative with calibrated lux sensor (TEMT6000)
- [MQTT protocol](../mqtt-protocol.md) — metric names and payload format
- [Components inventory](../components.md) — Seeedstudio light sensor specs
- [Architecture](../architecture.md) — module system and feature flags
