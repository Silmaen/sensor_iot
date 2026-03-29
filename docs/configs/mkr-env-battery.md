# MKR + ENV Shield + 1S Battery

MKR WiFi 1010 with stacked ENV Shield and single 18650 cell. Compact indoor sensor.

## Overview

| Parameter      | Value                                                  |
|----------------|--------------------------------------------------------|
| MCU            | Arduino MKR WiFi 1010                                  |
| Sensor         | BME280 (on MKR ENV Shield, I2C default Wire)           |
| Power          | 1S 18650 via JST connector (BQ24195L built-in charger) |
| Sleep mode     | None (continuous)                                      |
| Autonomy       | ~35h continuous (3000 mAh cell)                        |
| PlatformIO env | `thermo_mkr`                                           |
| Device ID      | `thermo_mkr`                                           |

## Modules Used

- [BME280](../modules/bme280.md) -- temperature, humidity, pressure via ENV Shield
- [Battery](../modules/battery.md) -- voltage monitoring via `ADC_BATTERY` (divider ratio 2.0)

## Wiring

![MKR Config](../img/config-mkr.svg)

No external wiring required beyond stacking the ENV Shield and plugging in the 18650
cell via JST. The ENV Shield communicates over I2C using the default Wire pins.

### GPIO Assignment

| Pin           | Function                                      |
|---------------|-----------------------------------------------|
| `SDA` / `SCL` | I2C to BME280 on ENV Shield                   |
| `ADC_BATTERY` | Battery voltage (on-board divider, ratio 2.0) |

## Power Budget

| Subsystem           | Active     |
|---------------------|------------|
| SAMD21 + WiFiNINA   | ~80 mA     |
| BME280 (ENV Shield) | < 1 mA     |
| **Total**           | **~80 mA** |

Estimated autonomy: 3000 mAh / 80 mA = ~37h (conservative: ~35h with WiFi spikes).

No deep sleep -- SAMD21 deep sleep is not yet implemented in this project.

## PlatformIO Environment

```ini
[env:thermo_mkr]
extends = common_samd
board = mkrwifi1010
build_flags =
    ${common_samd.build_flags}
    -DDEVICE_ID='"thermo_mkr"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_BME280
    -DHAS_BATTERY
    -DHAS_SERIAL_DEBUG
```

## Build & Upload

```bash
pio run -e thermo_mkr
pio run -e thermo_mkr -t upload
pio device monitor
```

## Notes

- **Compact form factor** -- just stack the ENV Shield on the MKR, plug in a cell,
  done.
- Battery monitoring uses the MKR's built-in ADC voltage divider (ratio 2.0), different
  from the ESP8266 external divider.
- `HAS_SERIAL_DEBUG` is enabled -- disable it for production to save power.
- The SAMD21 uses `WiFiNINA` (not the ESP8266 WiFi stack).
