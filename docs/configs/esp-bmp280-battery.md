# ESP8266 + BMP/BME280 + 2S Battery + Deep Sleep

D1 Mini with external BME/BMP280 breakout, 2S battery pack, and deep sleep for maximum
autonomy. See [components inventory](../components.md) for available hardware and
[architecture](../architecture.md) for the module system.

## Overview

| Parameter      | Value                                                  |
|----------------|--------------------------------------------------------|
| MCU            | Wemos D1 Mini v3 (ESP8266)                             |
| Sensor         | BME/BMP280 breakout board (I2C)                        |
| Power          | 2S 18650 -> BMS 2S -> MC78M05BTG LDO (5V)              |
| Sleep mode     | Deep sleep between readings                            |
| Sleep interval | 300s (configurable via MQTT)                           |
| Autonomy       | ~21 d theoretical (HW rev 1) · ~20 d measured          |
| PlatformIO env | `sensor_8266_bmp80`                                    |
| HW code / rev  | `E8BMEBAT` / rev 1                                     |
| Device ID      | Provisioned at runtime (fleet: `thermo_1`, `thermo_2`) |

## Modules Used

- [BME280](../modules/bme280.md) -- temperature, humidity, pressure
- [Battery](../modules/battery.md) -- voltage monitoring via external divider on A0
- [Power 2S](../modules/power-2s.md) -- 2S pack, BMS, buck converter
- [Deep Sleep](../modules/deep-sleep.md) -- ESP8266 deep sleep with D0-RST bridge

## Wiring

![Battery Config](../img/config-battery.svg)

### GPIO Assignment

| Pin         | Function                                      |
|-------------|-----------------------------------------------|
| D1 (GPIO5)  | I2C SCL to BME/BMP280                         |
| D2 (GPIO4)  | I2C SDA to BME/BMP280                         |
| A0          | Battery voltage via divider (R1=22k / R2=12k) |
| D0 (GPIO16) | Wired to RST for deep sleep wake              |

### Power Path

2S 18650 pack -> BMS 2S (protection) -> MC78M05BTG LDO (5V output, HW rev 1) -> D1 Mini
5V pin. HW rev 2 (planned) swaps this for an HT7350 LDO plus a MOSFET-switched divider.

See [power-2s](../modules/power-2s.md) for details on buck converter selection and BMS
wiring.

### Battery Monitoring

Voltage divider: R1=22k (top), R2=12k (bottom) from pack voltage to A0. Low-value
resistors keep source impedance (7.76k) within ESP8266 ADC spec. This maps the 2S
range (6.0-8.4V) to the ESP8266 ADC range (0-3.0V).

See [battery](../modules/battery.md) for divider calculations and SoC curve.

## Power Budget

| Subsystem                | Active  | Deep sleep |
|--------------------------|---------|------------|
| ESP8266 + WiFi           | ~70 mA  | ~20 uA     |
| BME/BMP280               | < 1 mA  | < 1 uA     |
| Buck converter quiescent | ~5 mA   | ~5 mA      |
| Voltage divider          | ~247 uA | ~247 uA    |

On **HW rev 1** (current fleet: `thermo_1`, `thermo_2`) the MC78M05BTG regulator (~3 mA Iq)
and the always-on voltage divider (~247 µA) dominate the sleep budget, so deep sleep only
saves so much: the theoretical autonomy is **~21 days** on a 2200 mAh 2S pack (5 min cycle,
4.5 s active — see [power-2s](../modules/power-2s.md) for the full breakdown).

**Field-measured autonomy** (daily `bat_percent` aggregates over ~100 days, 4 full
discharge cycles per unit, extrapolated to a 100 % → 0 % cycle):

| Device     | Measured autonomy | Range (per cycle) | Discharge slope |
|------------|-------------------|-------------------|-----------------|
| `thermo_1` | ~20 days          | 16–22 days        | ~5.1 %/day      |
| `thermo_2` | ~19 days          | 17–24 days        | ~5.4 %/day      |

The measured autonomy matches the HW rev 1 theoretical estimate within ~5–10 %, confirming
the power model and that the reclaimed 18650 cells hold ~2100 mAh (grade A/B). The planned
**HW rev 2** (HT7350 regulator + MOSFET-switched divider) targets ~76 days — see the
[power-2s autonomy comparison](../modules/power-2s.md#recommendations-for-2s-battery-nodes).

## PlatformIO Environment

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

One image serves the whole fleet (`thermo_1`, `thermo_2`, …): `device_id` and calibration are
provisioned at runtime, not compiled in. After the first flash, assign identity over serial
(`provision thermo_1`). See the [OTA module](../modules/ota.md) and
[calibration](../modules/calibration.md).

## Build & Upload

```bash
pio run -e sensor_8266_bmp80
pio run -e sensor_8266_bmp80 -t upload
pio device monitor   # only works before first sleep cycle
```

## Notes

- **D0-RST bridge** must be wired for deep sleep wake. Remove the bridge when uploading
  firmware (RST held low prevents boot).
- Serial debug is disabled (`HAS_SERIAL_DEBUG` not set) -- this is a production config.
- The sleep interval (default 300s) is adjustable via the `set_interval` MQTT command.
- BMP280 variants lack humidity -- the firmware handles this gracefully (reports
  temperature and pressure only).
