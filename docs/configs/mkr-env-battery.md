# MKR + ENV Shield + 1S Battery

MKR WiFi 1010 with stacked ENV Shield and single 18650 cell. Compact indoor sensor.
See [components inventory](../components.md) and [architecture](../architecture.md).

## Overview

| Parameter      | Value                                                      |
|----------------|------------------------------------------------------------|
| MCU            | Arduino MKR WiFi 1010                                      |
| Sensors        | HTS221 + LPS22HB + TEMT6000 + VEML6075 (on MKR ENV Shield) |
| Power          | 1S 18650 via JST connector (BQ24195L built-in charger)     |
| Sleep mode     | Standby between publications (RTCZero)                     |
| Autonomy       | ~5 d theoretical (duty-cycle, 5 min) · ~18 d measured      |
| PlatformIO env | `sensor_mkr_env` (debug: `sensor_mkr_debug`)               |
| HW code / rev  | `MKENVBAT` / rev 1                                         |
| Device ID      | Provisioned at runtime (fleet: `thermo_mkr`)               |

## Modules Used

- [MKR ENV](../modules/mkr-env.md) -- temperature, humidity, pressure, ambient light, UV index via ENV Shield
- [Battery](../modules/battery.md) -- voltage monitoring via `ADC_BATTERY` (divider ratio 2.0)

## Wiring

![MKR Config](../img/config-mkr.svg)

No external wiring required beyond stacking the ENV Shield and plugging in the 18650
cell via JST. The ENV Shield communicates over I2C (HTS221, LPS22HB, VEML6075) and
analog pin A2 (TEMT6000) using the default Wire pins.

### GPIO Assignment

| Pin           | Function                                       |
|---------------|------------------------------------------------|
| `SDA` / `SCL` | I2C to HTS221, LPS22HB, VEML6075 on ENV Shield |
| `A2`          | TEMT6000 analog light sensor                   |
| `ADC_BATTERY` | Battery voltage (on-board divider, ratio 2.0)  |

## Sensors on the ENV Shield

| Sensor   | Function              | Interface   | I2C Address |
|----------|-----------------------|-------------|-------------|
| HTS221   | Temperature, humidity | I2C         | 0x5F        |
| LPS22HB  | Barometric pressure   | I2C         | 0x5C        |
| TEMT6000 | Ambient light         | Analog (A2) | --          |
| VEML6075 | UV index (UVA/UVB)    | I2C         | 0x10        |

## Power Budget (Duty-Cycle Mode)

The firmware uses a **duty-cycle** strategy: WiFi is powered down between publications
and brought up only to transmit. This drastically reduces average consumption.

| Phase             | Duration | Current | Notes                         |
|-------------------|----------|---------|-------------------------------|
| WiFi off (idle)   | ~297s    | ~18 mA  | SAMD21 + ENV Shield, no radio |
| WiFi on (publish) | ~3s      | ~80 mA  | Connect, publish, disconnect  |

Average current at 5-min interval:

```ini
I_avg = (297 × 18 + 3 × 80) / 300 ≈ 18.6 mA
```

### Autonomy Estimates

| Cell                   | Capacity | Autonomy (5 min) | Autonomy (10 min) |
|------------------------|----------|------------------|-------------------|
| Samsung ICR18650-22HU  | 2150 mAh | ~4.8 days        | ~5.5 days         |
| Generic 18650 3000 mAh | 3000 mAh | ~6.7 days        | ~7.5 days         |

> **Measured vs theoretical:** field data for `thermo_mkr` (daily `bat_percent`
> aggregates over ~100 days, 3 full cycles) gives **~18 days** per charge (16–27 days
> per cycle, ~5.6 %/day). That is **3–4× the estimate above**. The gap means the
> "~18 mA idle" assumption in the power budget is wrong: the SAMD21 actually enters
> **standby (RTCZero)** between publications, so the real average current is ~5 mA
> (2150 mAh / 18 days), not 18.6 mA. The estimates below are a worst-case (no standby)
> bound; treat ~18 days as the realistic figure.

With `HAS_SERIAL_DEBUG` enabled, the default interval is 10s (for testing), which
keeps WiFi active most of the time (~75 mA average, ~28h with 2150 mAh).
Disable debug for production to use the 5-min interval.

## PlatformIO Environment

The production env carries no serial debug and no `-DDEVICE_ID` — identity is provisioned at
runtime. For bring-up, use `sensor_mkr_debug` (same hardware, `-DHAS_SERIAL_DEBUG`, waits for the
monitor at boot with a 10 s interval).

```ini
[env:sensor_mkr_env]
extends = common_samd
board = mkrwifi1010
build_flags =
    ${common_samd.build_flags}
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHW_CODE='"MKENVBAT"'
    -DHW_REV=1
    -DHAS_MKR_ENV
    -DHAS_BATTERY
    -DHAS_CALIBRATION
```

There is **no `-DHAS_OTA`**: OTA is ESP-only, so the MKR is reflashed over USB.

## Build & Upload

```bash
pio run -e sensor_mkr_env
pio run -e sensor_mkr_env -t upload
pio run -e sensor_mkr_debug -t upload   # serial-debug variant for bring-up
```

After the first flash, provision identity over serial (`provision thermo_mkr`). The MKR's USB is
native, so it does **not** reset when the port opens — press RESET or use `sensor_mkr_debug` to
catch the boot banner. See the [calibration module](../modules/calibration.md).

## Notes

- **Compact form factor** -- just stack the ENV Shield on the MKR, plug in a cell,
  done.
- The ENV Shield uses HTS221 + LPS22HB + TEMT6000 + VEML6075, **not** a BME280.
  Use `-DHAS_MKR_ENV`, not `-DHAS_BME280`.
- Battery monitoring uses the MKR's built-in ADC voltage divider (calibrated `bat_divider`,
  runtime), different from the ESP8266 external divider.
- The SAMD21 uses `WiFiNINA` (not the ESP8266 WiFi stack).
