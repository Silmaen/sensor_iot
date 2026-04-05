# Battery Monitoring Module

## Overview

The battery module measures battery voltage via an ADC and computes the state of charge (SoC). It is
enabled by the `-DHAS_BATTERY` build flag in `platformio.ini`.

**Metrics published** (on `thermo/{id}/sensors`):

| Metric        | Unit | Description              |
|---------------|------|--------------------------|
| `battery_pct` | %    | State of charge (0--100) |
| `battery_v`   | V    | Measured battery voltage |

**Battery alerts:** Two levels of alert are published on `thermo/{id}/status`:

| Level     | Message            | Condition                           |
|-----------|--------------------|-------------------------------------|
| `warning` | `low_battery`      | SoC <= `BATTERY_WARN_THRESHOLD`     |
| `error`   | `critical_battery` | SoC <= `BATTERY_CRITICAL_THRESHOLD` |

```json
{"level":"warning","message":"low_battery"}
{"level":"error","message":"critical_battery"}
```

Thresholds are platform-specific (defined in `include/config.h`):

| Platform           | Warning | Critical | Rationale                             |
|--------------------|---------|----------|---------------------------------------|
| ESP8266 (2S LiPo)  | 15%     | 5%       | Wide voltage range (6.0--8.4V)        |
| MKR WiFi (1S LiPo) | 20%     | 5%       | Narrow range (3.0--4.2V), PMIC cutoff |

## Hardware Configurations

### ESP8266 (Wemos D1 Mini) -- 2S LiPo

The ESP8266 has a single 10-bit ADC (0--3.3 V range) on pin `A0`. An **external voltage divider**
scales the 2S pack voltage down to the ADC input range.

| Parameter        | Value                              |
|------------------|------------------------------------|
| Battery          | 2S 18650 LiPo (6.0--8.4 V)         |
| ADC resolution   | 10-bit (0--1023)                   |
| ADC ref voltage  | 3.3 V                              |
| Divider R1       | 22 k-ohm (top, to V_bat)           |
| Divider R2       | 12 k-ohm (bottom, to GND)          |
| Divider ratio    | (R1+R2)/R2 = 2.833                 |
| Source impedance | R1//R2 = 7.76 k-ohm (ADC-friendly) |
| V_adc formula    | V_bat x R2/(R1+R2) = V_bat x 0.353 |
| V_adc at 8.4 V   | 2.96 V (ADC ~919)                  |
| V_adc at 6.0 V   | 2.12 V (ADC ~657)                  |
| ADC pin          | `A0` (`PIN_BATTERY_ADC`)           |

#### MOSFET Divider Switch (recommended)

The voltage divider draws **~247 µA continuously**, including during deep sleep. An N-channel MOSFET
on the low side of R2 cuts this drain to zero when not reading.

| Parameter              | Value                                                |
|------------------------|------------------------------------------------------|
| Switch pin             | `D3` (GPIO0) — `PIN_BATTERY_SWITCH` in `config.h`   |
| MOSFET                 | 2N7000, BS170, or any logic-level N-MOSFET           |
| Vgs(th) requirement    | < 2V (must fully turn on at 3.3V gate drive)         |
| Settling time          | 500 µs (RC time constant of divider + ADC input cap) |
| Quiescent savings      | ~247 µA eliminated during deep sleep                 |

**Circuit:** R1 (22k) → mid-point (to A0) → R2 (12k) → Q1 drain. Q1 source → GND. Q1 gate → D3.

When `D3 = HIGH`, Q1 conducts, the divider is active, and A0 reads the battery voltage.
When `D3 = LOW` (or during deep sleep), Q1 is off, no current flows through the divider.

**Firmware:** Enable by uncommenting `PIN_BATTERY_SWITCH` in `config.h`. The ADC driver
automatically toggles the pin before/after each read.

**Compatible transistors** (any of these work):

| Type     | Part       | Vgs(th) | Rds(on)  | Package  | Notes                        |
|----------|------------|---------|----------|----------|------------------------------|
| N-MOSFET | **2N7000** | 1.0-3.0V | 1.8 ohm  | TO-92    | Most common, easy to solder  |
| N-MOSFET | **BS170**  | 0.8-3.0V | 1.2 ohm  | TO-92    | Lower Rds(on) than 2N7000   |
| N-MOSFET | **IRLML6344** | 0.8V | 0.029 ohm | SOT-23 | SMD, excellent for low power |
| NPN BJT  | **2N2222** | Vbe~0.7V | —        | TO-92    | Needs 10k base resistor      |
| NPN BJT  | **BC547**  | Vbe~0.7V | —        | TO-92    | Needs 10k base resistor      |

> **N-MOSFET vs NPN:** MOSFETs are preferred — they draw zero gate current and switch cleanly
> from a GPIO. NPN transistors work too but need a base resistor (~10k) and draw ~330 µA from the
> GPIO when on. For a switch that's only on for 1 ms per cycle, either is fine.

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

```text
SoC = (V_bat - V_empty) / (V_full - V_empty) x 100
```

### 2S LiPo (ESP8266)

| Voltage (V) | SoC (%) | Notes                    |
|-------------|--------:|--------------------------|
| 8.40        |     100 | Fully charged            |
| 8.16        |      90 |                          |
| 7.92        |      80 |                          |
| 7.68        |      70 |                          |
| 7.44        |      60 |                          |
| 7.20        |      50 | Nominal                  |
| 6.96        |      40 |                          |
| 6.72        |      30 |                          |
| 6.48        |      20 |                          |
| 6.36        |      15 | Warning (low_battery)    |
| 6.24        |      10 |                          |
| 6.12        |       5 | Error (critical_battery) |
| 6.00        |       0 | Empty / cutoff           |

### 1S LiPo (MKR WiFi 1010)

| Voltage (V) | SoC (%) | Notes                    |
|-------------|--------:|--------------------------|
| 4.20        |     100 | Fully charged            |
| 4.08        |      90 |                          |
| 3.96        |      80 |                          |
| 3.84        |      70 |                          |
| 3.72        |      60 |                          |
| 3.60        |      50 | Nominal                  |
| 3.48        |      40 |                          |
| 3.36        |      30 |                          |
| 3.24        |      20 | Warning (low_battery)    |
| 3.12        |      10 |                          |
| 3.06        |       5 | Error (critical_battery) |
| 3.00        |       0 | Empty / cutoff           |

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

## See Also

- [2S power supply](power-2s.md) — hardware power chain for battery-powered nodes
- [Deep sleep](deep-sleep.md) — one-shot mode for battery life extension
- [Calibration](calibration.md) — per-device sensor offset correction
- [Battery cells guide](../battery-cells.md) — testing and grading reclaimed 18650 cells
- [MQTT protocol](../mqtt-protocol.md) — battery metrics and alert format
- [Components inventory](../components.md) — battery hardware (BMS, cells, buck module)
- Configs using this module: [ESP+BMP280](../configs/esp-bmp280-battery.md),
  [ESP+SHT30](../configs/esp-sht30-battery.md), [MKR+ENV](../configs/mkr-env-battery.md)
