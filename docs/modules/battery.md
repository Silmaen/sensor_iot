# Battery Monitoring Module

## Overview

The battery module measures battery voltage via an ADC and computes the state of charge (SoC). It is
enabled by the `-DHAS_BATTERY` build flag in `platformio.ini`.

**Metrics published** (on `thermo/{id}/sensors`):

| Metric        | Unit | Description              |
|---------------|------|--------------------------|
| `battery_pct` | %    | State of charge (0--100) |
| `battery_v`   | V    | Measured battery voltage |

**Low battery alert:** when SoC drops to 15% or below, the device publishes a status message on
`thermo/{id}/status`:

```json
{
  "level": "warning",
  "message": "low_battery"
}
```

The threshold is defined by `BATTERY_LOW_THRESHOLD` in `include/config.h`.

## Hardware Configurations

### ESP8266 (Wemos D1 Mini) -- 2S LiPo

The ESP8266 has a single 10-bit ADC (0--3.3 V range) on pin `A0`. An **external voltage divider**
scales the 2S pack voltage down to the ADC input range.

| Parameter       | Value                            |
|-----------------|----------------------------------|
| Battery         | 2S 18650 LiPo (6.0--8.4 V)       |
| ADC resolution  | 10-bit (0--1023)                 |
| ADC ref voltage | 3.3 V                            |
| Divider R1      | 150 k-ohm (top, to V_bat)        |
| Divider R2      | 100 k-ohm (bottom, to GND)       |
| Divider ratio   | (R1+R2)/R2 = 2.5                 |
| V_adc formula   | V_bat x R2/(R1+R2) = V_bat x 0.4 |
| V_adc at 8.4 V  | 3.36 V (ADC ~1023)               |
| V_adc at 6.0 V  | 2.40 V (ADC ~744)                |
| ADC pin         | `A0` (`PIN_BATTERY_ADC`)         |

![ESP8266 battery wiring diagram](../img/battery-wiring.svg)

![Battery monitoring schematic](../img/battery-schematic.svg)

### MKR WiFi 1010 -- 1S LiPo

The MKR board has a **built-in voltage divider** connected to its LiPo connector and a 12-bit ADC.

| Parameter       | Value                        |
|-----------------|------------------------------|
| Battery         | 1S LiPo (3.0--4.2 V)         |
| ADC resolution  | 12-bit (0--4095)             |
| ADC ref voltage | 3.3 V                        |
| Divider ratio   | ~2.0 (approximate, built-in) |
| ADC pin         | `ADC_BATTERY`                |

The SAMD21 driver calls `analogReadResolution(12)` before each read to ensure 12-bit precision.

## Voltage-to-SoC Mapping

The firmware uses a **linear interpolation** between `BATTERY_VOLTAGE_EMPTY` (0%) and
`BATTERY_VOLTAGE_FULL` (100%), clamped at both ends.

```
SoC = (V_bat - V_empty) / (V_full - V_empty) x 100
```

### 2S LiPo (ESP8266)

| Voltage (V) | SoC (%) | Notes             |
|-------------|--------:|-------------------|
| 8.40        |     100 | Fully charged     |
| 8.16        |      90 |                   |
| 7.92        |      80 |                   |
| 7.68        |      70 |                   |
| 7.44        |      60 |                   |
| 7.20        |      50 | Nominal           |
| 6.96        |      40 |                   |
| 6.72        |      30 |                   |
| 6.48        |      20 |                   |
| 6.36        |      15 | Low battery alert |
| 6.24        |      10 |                   |
| 6.00        |       0 | Empty / cutoff    |

### 1S LiPo (MKR WiFi 1010)

| Voltage (V) | SoC (%) | Notes             |
|-------------|--------:|-------------------|
| 4.20        |     100 | Fully charged     |
| 4.08        |      90 |                   |
| 3.96        |      80 |                   |
| 3.84        |      70 |                   |
| 3.72        |      60 |                   |
| 3.60        |      50 | Nominal           |
| 3.48        |      40 |                   |
| 3.36        |      30 |                   |
| 3.24        |      20 |                   |
| 3.18        |      15 | Low battery alert |
| 3.12        |      10 |                   |
| 3.00        |       0 | Empty / cutoff    |

> **Note:** Real LiPo discharge curves are non-linear. The linear mapping is a deliberate
> simplification. For higher accuracy, a lookup table or polynomial fit could replace the linear
> formula in `voltage_to_soc()`.

## Calibration Notes

1. **Divider tolerance:** 5% resistors shift the divider ratio. Measure the actual ratio with a
   multimeter and adjust `BATTERY_DIVIDER_RATIO` in `battery.h` if needed.
2. **ADC offset:** The ESP8266 ADC has a documented non-linearity near 0 V and 3.3 V. Readings in
   the 0.5--3.0 V range are most accurate.
3. **MKR built-in divider:** The default ratio of 2.0 is approximate. Calibrate by comparing
   `battery_v` with a multimeter reading and adjusting `BATTERY_DIVIDER_RATIO`.
4. **ADC noise:** Consider averaging multiple ADC samples if readings fluctuate. The current
   implementation reads a single sample per cycle.

## PlatformIO Configuration

Enable battery monitoring by adding `-DHAS_BATTERY` to the `build_flags` of the target environment
in `platformio.ini`:

```ini
build_flags =
    -DHAS_BATTERY
; ... other flags
```

## Firmware Files

| File                                             | Purpose                                              |
|--------------------------------------------------|------------------------------------------------------|
| `lib/thermo_core/src/battery.h`                  | ADC conversion + SoC calculation (portable)          |
| `lib/thermo_core/src/battery.cpp`                | Implementation of `adc_to_voltage`, `voltage_to_soc` |
| `lib/thermo_core/src/modules/battery_module.h`   | Module interface (register + contribute)             |
| `lib/thermo_core/src/modules/battery_module.cpp` | Registers `battery_pct` and `battery_v` metrics      |
| `src/hw/battery_adc.cpp`                         | ESP8266 ADC driver (`analogRead(A0)`)                |
| `src/hw/samd_battery_adc.cpp`                    | MKR WiFi 1010 ADC driver (12-bit)                    |
| `include/config.h`                               | Pin definitions and `BATTERY_LOW_THRESHOLD`          |
