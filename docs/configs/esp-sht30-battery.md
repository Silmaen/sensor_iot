# ESP8266 + SHT30 Shield + 2S Battery + Deep Sleep

D1 Mini with stacked SHT30 Shield, 2S battery pack, and deep sleep. More compact than
the [BME280 version](esp-bmp280-battery.md) since the sensor stacks directly, but no pressure
measurement. See [components inventory](../components.md) and [architecture](../architecture.md).

## Overview

| Parameter      | Value                                                  |
|----------------|--------------------------------------------------------|
| MCU            | Wemos D1 Mini v3 (ESP8266)                             |
| Sensor         | SHT30 Shield v2.1.0 (stacked, I2C 0x44)                |
| Power          | 2S 18650 -> BMS 2S -> MP1584 buck (5V)                 |
| Sleep mode     | Deep sleep between readings                            |
| Sleep interval | 300s (configurable via MQTT)                           |
| Autonomy       | ~119 days (5 min interval, 3000 mAh x2, MP1584 at 90%) |
| PlatformIO env | `thermo_sht30`                                         |
| Device ID      | `thermo_sht30`                                         |

## Modules Used

- [SHT30](../modules/sht30.md) -- temperature, humidity (no pressure)
- [Battery](../modules/battery.md) -- voltage monitoring via external divider on A0
- [Power 2S](../modules/power-2s.md) -- 2S pack, BMS, buck converter
- [Deep Sleep](../modules/deep-sleep.md) -- ESP8266 deep sleep with D0-RST bridge

## Wiring

![Battery Config](../img/config-battery.svg)

### GPIO Assignment

| Pin         | Function                                      |
|-------------|-----------------------------------------------|
| D1 (GPIO5)  | I2C SCL (SHT30 Shield, stacked)               |
| D2 (GPIO4)  | I2C SDA (SHT30 Shield, stacked)               |
| A0          | Battery voltage via divider (R1=22k / R2=12k) |
| D0 (GPIO16) | Wired to RST for deep sleep wake              |

### Power Path

2S 18650 pack -> BMS 2S (protection) -> MP1584 buck (5V output, 100 uF cap on output)
-> D1 Mini 5V pin.

See [power-2s](../modules/power-2s.md) for buck converter details.

### Battery Monitoring

Same divider as the BME280 config: R1=22k (top), R2=12k (bottom) from pack voltage
to A0. Low-value resistors for ADC source impedance compatibility. See
[battery](../modules/battery.md) for details.

## Power Budget

| Subsystem        | Active  | Deep sleep |
|------------------|---------|------------|
| ESP8266 + WiFi   | ~70 mA  | ~20 uA     |
| SHT30            | < 1 mA  | < 1 uA     |
| MP1584 quiescent | ~0.1 mA | ~0.1 mA    |
| Voltage divider  | ~247 uA | ~247 uA    |

The MP1584's lower quiescent current (~0.1 mA vs ~5 mA for H78M05BT) gives better
autonomy: ~119 days vs ~89 days. See [deep-sleep](../modules/deep-sleep.md) for the
full calculation.

## PlatformIO Environment

```ini
[env:thermo_sht30]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DDEVICE_ID='"thermo_sht30"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_SHT30
    -DHAS_BATTERY
    -DHAS_DEEP_SLEEP
```

## Build & Upload

```bash
pio run -e thermo_sht30
pio run -e thermo_sht30 -t upload
pio device monitor   # only works before first sleep cycle
```

## Notes

- **No pressure measurement** -- the SHT30 only reports temperature and humidity. Use
  the BME280 config if pressure is needed.
- **More compact** than the BME280 version: the SHT30 Shield stacks directly on the D1
  Mini, no wiring needed for the sensor.
- 100 uF electrolytic capacitor on the MP1584 output stabilizes voltage during WiFi
  transmit spikes.
- D0-RST bridge must be removed for firmware upload.
