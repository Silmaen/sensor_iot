# ESP8266 + SHT30 Shield + 2S Battery + Deep Sleep

D1 Mini with stacked SHT30 Shield, 2S battery pack, and deep sleep. More compact than
the [BME280 version](esp-bmp280-battery.md) since the sensor stacks directly, but no pressure
measurement. See [components inventory](../components.md) and [architecture](../architecture.md).

## Overview

| Parameter      | Value                                                 |
|----------------|-------------------------------------------------------|
| MCU            | Wemos D1 Mini v3 (ESP8266)                            |
| Sensor         | SHT30 Shield v2.1.0 (stacked, I2C 0x44)               |
| Power          | 2S 18650 -> BMS 2S -> MP1584 buck (5V)                |
| Sleep mode     | Deep sleep between readings                           |
| Sleep interval | 300s (configurable via MQTT)                          |
| Autonomy       | ~21 d theoretical (HW rev 1) — no unit deployed yet   |
| PlatformIO env | `sensor_8266_sht30`                                   |
| HW code / rev  | `E8SHTBAT` / rev 1                                    |
| Device ID      | Provisioned at runtime (`provision <id>` over serial) |

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

2S 18650 pack -> BMS 2S (protection) -> MC78M05BTG LDO (5V output, HW rev 1, 100 uF cap on
output) -> D1 Mini 5V pin.

See [power-2s](../modules/power-2s.md) for buck converter details.

### Battery Monitoring

Same divider as the BME280 config: R1=22k (top), R2=12k (bottom) from pack voltage
to A0. Low-value resistors for ADC source impedance compatibility. See
[battery](../modules/battery.md) for details.

## Power Budget

Same 2S power hardware as the [BME280 config](esp-bmp280-battery.md): **HW rev 1** uses the
MC78M05BTG 5V LDO (~3 mA Iq) and an always-on voltage divider (~247 µA), which dominate the
deep-sleep budget.

| Subsystem          | Active  | Deep sleep |
|--------------------|---------|------------|
| ESP8266 + WiFi     | ~70 mA  | ~20 uA     |
| SHT30              | < 1 mA  | < 1 uA     |
| MC78M05BTG (rev 1) | ~3 mA   | ~3 mA      |
| Voltage divider    | ~247 uA | ~247 uA    |

Theoretical autonomy is **~21 days** on a 2200 mAh 2S pack (5 min cycle). No SHT30 unit is
deployed yet, so there is no field-measured figure — the BME280 fleet on identical power
hardware measures ~19–20 days, which is the best proxy. The planned **HW rev 2** (HT7350 +
MOSFET-switched divider) targets ~76 days. See the
[power-2s autonomy comparison](../modules/power-2s.md#recommendations-for-2s-battery-nodes)
for the full breakdown.

## PlatformIO Environment

```ini
[env:sensor_8266_sht30]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHW_CODE='"E8SHTBAT"'
    -DHW_REV=1
    -DHAS_SHT30
    -DHAS_BATTERY
    -DHAS_DEEP_SLEEP
    -DHAS_CALIBRATION
    -DHAS_OTA
```

`device_id` is not compiled in — provision it once over serial (`provision <id>`) after the
first flash. See the [OTA module](../modules/ota.md) and [calibration](../modules/calibration.md).

## Build & Upload

```bash
pio run -e sensor_8266_sht30
pio run -e sensor_8266_sht30 -t upload
pio device monitor   # only works before first sleep cycle
```

## Notes

- **No pressure measurement** -- the SHT30 only reports temperature and humidity. Use
  the BME280 config if pressure is needed.
- **More compact** than the BME280 version: the SHT30 Shield stacks directly on the D1
  Mini, no wiring needed for the sensor.
- 100 uF electrolytic capacitor on the regulator output stabilizes voltage during WiFi
  transmit spikes.
- D0-RST bridge must be removed for firmware upload.
