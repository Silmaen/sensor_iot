# ESP8266 + BMP/BME280 + 2S Battery + Deep Sleep

D1 Mini with external BME/BMP280 breakout, 2S battery pack, and deep sleep for maximum
autonomy.

## Overview

| Parameter      | Value                                     |
|----------------|-------------------------------------------|
| MCU            | Wemos D1 Mini v3 (ESP8266)                |
| Sensor         | BME/BMP280 breakout board (I2C)           |
| Power          | 2S 18650 -> BMS 2S -> buck converter (5V) |
| Sleep mode     | Deep sleep between readings               |
| Sleep interval | 300s (configurable via MQTT)              |
| Autonomy       | ~89 days (5 min interval, 3000 mAh x2)    |
| PlatformIO env | `thermo_1`                                |
| Device ID      | `thermo_1`                                |

## Modules Used

- [BME280](../modules/bme280.md) -- temperature, humidity, pressure
- [Battery](../modules/battery.md) -- voltage monitoring via external divider on A0
- [Power 2S](../modules/power-2s.md) -- 2S pack, BMS, buck converter
- [Deep Sleep](../modules/deep-sleep.md) -- ESP8266 deep sleep with D0-RST bridge

## Wiring

![Battery Config](../img/config-battery.svg)

### GPIO Assignment

| Pin         | Function                                        |
|-------------|-------------------------------------------------|
| D1 (GPIO5)  | I2C SCL to BME/BMP280                           |
| D2 (GPIO4)  | I2C SDA to BME/BMP280                           |
| A0          | Battery voltage via divider (R1=22k / R2=12k)   |
| D0 (GPIO16) | Wired to RST for deep sleep wake                |

### Power Path

2S 18650 pack -> BMS 2S (protection) -> H78M05BT or MP1584 buck (5V output) -> D1 Mini
5V pin.

See [power-2s](../modules/power-2s.md) for details on buck converter selection and BMS
wiring.

### Battery Monitoring

Voltage divider: R1=22k (top), R2=12k (bottom) from pack voltage to A0. Low-value
resistors keep source impedance (7.76k) within ESP8266 ADC spec. This maps the 2S
range (6.0-8.4V) to the ESP8266 ADC range (0-3.0V).

See [battery](../modules/battery.md) for divider calculations and SoC curve.

## Power Budget

| Subsystem                | Active | Deep sleep |
|--------------------------|--------|------------|
| ESP8266 + WiFi           | ~70 mA | ~20 uA     |
| BME/BMP280               | < 1 mA | < 1 uA     |
| Buck converter quiescent | ~5 mA  | ~5 mA      |
| Voltage divider          | ~247 uA | ~247 uA   |

See [deep-sleep](../modules/deep-sleep.md) for the full autonomy calculation.

## PlatformIO Environment

```ini
[env:thermo_1]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DDEVICE_ID='"thermo_1"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_BME280
    -DHAS_BATTERY
    -DHAS_DEEP_SLEEP
```

## Build & Upload

```bash
pio run -e thermo_1
pio run -e thermo_1 -t upload
pio device monitor   # only works before first sleep cycle
```

## Notes

- **D0-RST bridge** must be wired for deep sleep wake. Remove the bridge when uploading
  firmware (RST held low prevents boot).
- Serial debug is disabled (`HAS_SERIAL_DEBUG` not set) -- this is a production config.
- The sleep interval (default 300s) is adjustable via the `set_interval` MQTT command.
- BMP280 variants lack humidity -- the firmware handles this gracefully (reports
  temperature and pressure only).
