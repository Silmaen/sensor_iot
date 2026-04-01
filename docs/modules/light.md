# Light Sensor Module

## Overview

The light module reads an **analog photoresistor** (Seeedstudio Grove-compatible light sensor) and publishes a relative
light level as a percentage. It is enabled by the `-DHAS_LIGHT` build flag in `platformio.ini`.

**Metrics published** (on `thermo/{id}/sensors`):

| Metric  | Unit | Description                                   |
|---------|------|-----------------------------------------------|
| `light` | %    | Relative light level (0 = dark, 100 = bright) |

## Sensor Specifications

| Parameter       | Value                                        |
|-----------------|----------------------------------------------|
| Sensor type     | Photoresistor (LDR) in voltage divider       |
| Output          | Analog voltage, 0–VCC proportional to light  |
| Supply voltage  | 3.3–5V                                       |
| Interface       | Single analog pin                            |
| Response time   | 20–30 ms                                     |
| Spectral peak   | ~540 nm (green, matches human eye)           |

> **Note:** This sensor is **not calibrated** to lux. The published percentage represents a relative
> reading: 0% = very dark, 100% = maximum light the sensor can detect. For calibrated lux
> measurement, use the MKR ENV Shield (`HAS_MKR_ENV`).

## Wiring

### ESP8266 (D1 Mini)

| D1 Mini Pin | GPIO | Sensor Pin | Notes                       |
|-------------|------|------------|-----------------------------|
| A0          | ADC0 | SIG        | Analog output               |
| 3V3         | —    | VCC        | 3.3V supply                 |
| GND         | —    | GND        | Ground                      |

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

```ini
[env:sensor_esp32_light]
extends = common_esp32
build_flags =
    ${common_esp32.build_flags}
    -DDEVICE_ID='"sensor_light_1"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_LIGHT
    -DHAS_SERIAL_DEBUG
```

## Firmware Files

| File                                            | Role                                    |
|-------------------------------------------------|-----------------------------------------|
| `lib/thermo_core/src/modules/light_module.h`    | Module interface + ADC conversion       |
| `lib/thermo_core/src/modules/light_module.cpp`  | Module logic (register, contribute)     |
| `src/hw/light_adc.h` _(to create)_              | Hardware driver header (platform ADC)   |
| `src/hw/light_adc.cpp` _(to create)_            | Hardware driver (analogRead + max)      |

### Module API

```c++
void light_module_register(ModuleRegistry& reg);
void light_module_contribute(PayloadBuilder& pb, uint8_t light_pct);
uint8_t adc_to_light_pct(uint16_t raw, uint16_t adc_max);
```

## MQTT Payload

When active alongside other modules, the device publishes on `thermo/{device_id}/sensors`:

```json
{"temperature":22.5,"humidity":48.3,"light":65}
```

## Troubleshooting

| Symptom            | Cause                        | Fix                                            |
|--------------------|------------------------------|------------------------------------------------|
| Always reads 0%    | Wrong ADC pin or not wired   | Check wiring and pin definition in config.h    |
| Always reads 100%  | VCC connected to signal pin  | Verify SIG goes to ADC, not VCC                |
| Erratic readings   | Long wire or no decoupling   | Add 100nF cap near sensor, keep wires short    |
| Conflicts with bat | A0 shared on ESP8266         | Use ESP32 or add analog multiplexer            |
