# ESP8266 + SHT30 Shield + 7-Segment Display

D1 Mini with stacked SHT30 Shield and 3-digit 7-segment display. USB powered, always
on, intended as an indoor display station. See [components inventory](../components.md)
and [architecture](../architecture.md).

## Overview

| Parameter      | Value                                   |
|----------------|-----------------------------------------|
| MCU            | Wemos D1 Mini v3 (ESP8266)              |
| Sensor         | SHT30 Shield v2.1.0 (stacked, I2C 0x44) |
| Display        | 3-digit 7-segment, shift registers      |
| Power          | USB 5V (always connected)               |
| Sleep mode     | None (continuous)                       |
| PlatformIO env | `thermo_display_sht30`                  |

## Modules Used

- [SHT30](../modules/sht30.md) -- temperature, humidity
- [Display](../modules/display.md) -- 3-digit 7-segment with shift registers + button

## Wiring

![Display Config](../img/config-display.svg)

### GPIO Assignment

| Pin         | Function                        |
|-------------|---------------------------------|
| D1 (GPIO5)  | I2C SCL (SHT30 Shield, stacked) |
| D2 (GPIO4)  | I2C SDA (SHT30 Shield, stacked) |
| D7 (GPIO13) | Shift register DATA             |
| D5 (GPIO14) | Shift register CLOCK            |
| D8 (GPIO15) | Shift register LATCH            |
| D6 (GPIO12) | Push button (active LOW)        |

## Display Modes

The push button on D6 cycles through display modes:

| Mode        | Content                                    |
|-------------|--------------------------------------------|
| Temperature | Current temperature in C (e.g. `22.5`)     |
| Humidity    | Current relative humidity in % (e.g. `45`) |

Only 2 display modes (vs 3 with BME280) since the SHT30 has no pressure sensor.

## PlatformIO Environment

```ini
[env:thermo_display_sht30]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DDEVICE_ID='"thermo_display_sht30"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_SHT30
    -DHAS_DISPLAY
    -DHAS_SERIAL_DEBUG
```

## Build & Upload

```bash
pio run -e thermo_display_sht30
pio run -e thermo_display_sht30 -t upload
pio device monitor
```

## Notes

- The existing `thermo_display` env uses BME280. This config replaces the BME280 with
  the SHT30 Shield for a more compact build (sensor stacks, no wiring).
- The `DisplayMode` enum is automatically adapted: only Temperature and Humidity modes
  (no Pressure) when `HAS_SHT30` is used without `HAS_BME280`.
- WiFi always active, continuous MQTT publishing.
- No battery module -- USB powered permanently.
