# Battery Monitoring Module

## Overview

The battery module measures battery voltage via an ADC and computes the state of charge (SoC). It is
enabled by the `-DHAS_BATTERY` build flag in `platformio.ini`.

**Metrics published** (on `thermo/{id}/sensors`):

| Metric | Unit | Description              |
|--------|------|--------------------------|
| `bat`  | %    | State of charge (0--100) |
| `batv` | V    | Measured battery voltage |

> The server stores these under the canonical names `bat_percent` / `bat_voltage`; the device
> publishes the short `bat` / `batv` keys.

**Battery alerts:** Two levels of alert are published on `thermo/{id}/status`:

| Level     | Message            | Condition                           |
|-----------|--------------------|-------------------------------------|
| `warning` | `low_battery`      | SoC <= `BATTERY_WARN_THRESHOLD`     |
| `error`   | `critical_battery` | SoC <= `BATTERY_CRITICAL_THRESHOLD` |

```json
{"level":"warning","message":"low_battery"}
{"level":"error","message":"critical_battery"}
```

Thresholds are platform-specific (defined in `lib/thermo_core/src/config.h`):

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

#### MOSFET Divider Switch (ESP8266: HW rev 2, planned)

The voltage divider draws **~247 µA continuously**, including during deep sleep — on HW rev 1 this
is one of the two dominant sleep drains (with the MC78M05BTG regulator) and caps autonomy at ~20 days.
An N-channel MOSFET on the low side of R2 cuts this drain to zero when not reading. On the ESP8266 this
is a **HW rev 2** change (the `PIN_BATTERY_SWITCH` define is currently commented out in `config.h`); on
the **ESP32-C3 it is already present** (`PIN_BATTERY_SWITCH = GPIO5`, always built).

| Parameter           | Value                                                          |
|---------------------|----------------------------------------------------------------|
| Switch pin          | ESP8266 `D3` (GPIO0) / ESP32-C3 `GPIO5` — `PIN_BATTERY_SWITCH` |
| MOSFET              | 2N7000, BS170, or any logic-level N-MOSFET                     |
| Vgs(th) requirement | < 2V (must fully turn on at 3.3V gate drive)                   |
| Settling time       | 500 µs (RC time constant of divider + ADC input cap)           |
| Quiescent savings   | ~247 µA eliminated during deep sleep                           |

**Circuit:** R1 (22k) → mid-point (to A0) → R2 (12k) → Q1 drain. Q1 source → GND. Q1 gate → switch pin.

When the switch pin is `HIGH`, Q1 conducts, the divider is active, and the ADC reads the battery
voltage. When it is `LOW` (or during deep sleep), Q1 is off and no current flows through the divider.

**Firmware:** on the ESP8266, enable by uncommenting `PIN_BATTERY_SWITCH` in `config.h` once the HW
rev 2 MOSFET is fitted. The ADC driver automatically toggles the pin before/after each read whenever
the define is present.

**Compatible transistors** (any of these work):

| Type     | Part          | Vgs(th)  | Rds(on)   | Package | Notes                        |
|----------|---------------|----------|-----------|---------|------------------------------|
| N-MOSFET | **2N7000**    | 1.0-3.0V | 1.8 ohm   | TO-92   | Most common, easy to solder  |
| N-MOSFET | **BS170**     | 0.8-3.0V | 1.2 ohm   | TO-92   | Lower Rds(on) than 2N7000    |
| N-MOSFET | **IRLML6344** | 0.8V     | 0.029 ohm | SOT-23  | SMD, excellent for low power |
| NPN BJT  | **2N2222**    | Vbe~0.7V | —         | TO-92   | Needs 10k base resistor      |
| NPN BJT  | **BC547**     | Vbe~0.7V | —         | TO-92   | Needs 10k base resistor      |

> **N-MOSFET vs NPN:** MOSFETs are preferred — they draw zero gate current and switch cleanly
> from a GPIO. NPN transistors work too but need a base resistor (~10k) and draw ~330 µA from the
> GPIO when on. For a switch that's only on for 1 ms per cycle, either is fine.

![ESP8266 battery wiring diagram](../img/battery-wiring.svg)

![Battery monitoring schematic](../img/battery-schematic.svg)

### MKR WiFi 1010 -- 1S LiPo

The MKR board has a **built-in voltage divider** connected to its LiPo connector and a 12-bit ADC.

| Parameter       | Value                                                      |
|-----------------|------------------------------------------------------------|
| Battery         | 1S LiPo (3.0--4.2 V)                                       |
| ADC resolution  | 12-bit (0--4095)                                           |
| ADC ref voltage | 3.3 V                                                      |
| Divider ratio   | ~2.0 built-in (calibrated per unit, runtime `bat_divider`) |
| ADC pin         | `ADC_BATTERY`                                              |

The SAMD21 driver calls `analogReadResolution(12)` before each read to ensure 12-bit precision.

## Voltage-to-SoC Mapping

The firmware maps voltage to SoC through a **non-linear Li-ion/LiPo discharge curve** rather than a
straight line. A linear mapping is badly wrong for these cells: it never reaches 100% (a real pack
tops out below its nominal full voltage under load) and it *overestimates* charge near the end,
making the device die while still reporting 20-25%.

The curve is expressed **per cell** and shared by all platforms (`voltage_to_soc()` divides the pack
voltage by `BATTERY_CELLS`). It was derived from real 17-18 day discharge traces of the 2S field
devices — assuming a roughly constant load, `SoC ≈ 100% − fraction of runtime elapsed`. It captures
the flat mid plateau and the steep end-of-life cliff. The raw curve result is then rescaled so that
the configured `BATTERY_VOLTAGE_EMPTY` reads exactly 0% and `BATTERY_VOLTAGE_FULL` exactly 100% on
the given hardware.

Bounds are **calibrated from observed field data** (in `config.h`):

| Platform        | Cells | Empty (0%) | Full (100%) | Basis                                             |
|-----------------|-------|------------|-------------|---------------------------------------------------|
| ESP8266 / ESP32 | 2     | 6.00 V     | 8.30 V      | Observed ceiling ~8.30 V, floor ~5.9 V            |
| MKR WiFi 1010   | 1     | 3.30 V     | 4.10 V      | Observed ceiling ~4.08 V, cutoff/recharge ~3.30 V |

### 2S (ESP8266 / ESP32-C3)

| Voltage (V) | SoC (%) | Notes                    |
|-------------|--------:|--------------------------|
| 8.30        |     100 | Fully charged (ceiling)  |
| 8.10        |      96 |                          |
| 7.90        |      84 | Plateau                  |
| 7.70        |      71 | Plateau                  |
| 7.55        |      52 | Plateau                  |
| 7.45        |      36 | Knee                     |
| 7.35        |      29 | Cliff begins             |
| 7.10        |      21 |                          |
| 6.90        |      17 | Warning (low_battery)    |
| 6.60        |      11 |                          |
| 6.35        |       5 | Error (critical_battery) |
| 6.15        |       2 |                          |
| 6.00        |       0 | Empty / cutoff           |

### 1S (MKR WiFi 1010)

| Voltage (V) | SoC (%) | Notes                    |
|-------------|--------:|--------------------------|
| 4.10        |     100 | Fully charged (ceiling)  |
| 4.05        |      96 |                          |
| 4.00        |      89 |                          |
| 3.90        |      75 | Plateau                  |
| 3.80        |      55 | Plateau                  |
| 3.75        |      35 | Knee                     |
| 3.70        |      23 | Cliff begins             |
| 3.65        |      17 | Warning (low_battery)    |
| 3.55        |      11 |                          |
| 3.45        |       7 | Error (critical_battery) |
| 3.35        |       2 |                          |
| 3.30        |       0 | Empty / cutoff (PMIC)    |

> **Note:** The curve is calibrated on voltage measured *while the radio is active* (there is some
> sag), so it is slightly conservative. The `LIPO_CURVE[]` table lives in `battery.cpp`; adjust the
> per-cell breakpoints there if a different cell chemistry is used. The observed MKR ceiling (~4.08 V
> instead of 4.20 V) suggests its built-in divider may read a few % low — verify with `calibrate_battery`.

## Calibration Notes

The divider ratio is a **runtime calibration** (`bat_divider`), not a build flag. It lives in the
config store and server mirror, and is set with the `calibrate_battery` command (measured pack voltage
→ derived ratio) or `set_calibration` (`bat_divider` directly). It survives OTA and is re-pushed by the
server after a reset. See [calibration](calibration.md).

1. **Divider tolerance:** 5% resistors shift the divider ratio. Run `calibrate_battery` with a known
   pack voltage at commissioning; the derived `bat_divider` is stored per unit.
2. **ADC offset:** The ESP8266 ADC has a documented non-linearity near 0 V and 3.3 V. Readings in
   the 0.5--3.0 V range are most accurate.
3. **12-bit boards (ESP32-C3 / MKR):** these MUST be calibrated after provisioning — an uncalibrated
   node over-reads. Compare `batv` with a multimeter and run `calibrate_battery`.
4. **ADC noise:** Consider averaging multiple ADC samples if readings fluctuate. The current
   implementation reads a single sample per cycle.
5. **Sanity bounds:** `bat_divider` is clamped to [0.5, 6.0] at boot and at runtime to reject bad
   calibration values.

## PlatformIO Configuration

Enable battery monitoring by adding `-DHAS_BATTERY` to the `build_flags` of the target environment
in `platformio.ini`:

```ini
build_flags =
    -DHAS_BATTERY
; ... other flags
```

## Firmware Files

| File                                             | Purpose                                                  |
|--------------------------------------------------|----------------------------------------------------------|
| `lib/thermo_core/src/battery.h`                  | Portable ADC/SoC API (includes `config.h` for constants) |
| `lib/thermo_core/src/battery.cpp`                | Implementation of `adc_to_voltage`, `voltage_to_soc`     |
| `lib/thermo_core/src/modules/battery_module.h`   | Module interface (register + contribute)                 |
| `lib/thermo_core/src/modules/battery_module.cpp` | Registers `bat` and `batv` metrics                       |
| `lib/thermo_platform/src/battery_adc.cpp`        | Unified ADC driver (`#if` per platform, 10/12-bit)       |
| `lib/thermo_core/src/config.h`                   | Pins, ADC constants, `BATTERY_WARN/CRITICAL_THRESHOLD`   |

## See Also

- [2S power supply](power-2s.md) — hardware power chain for battery-powered nodes
- [Deep sleep](deep-sleep.md) — one-shot mode for battery life extension
- [Calibration](calibration.md) — per-device sensor offset correction
- [Battery cells guide](../battery-cells.md) — testing and grading reclaimed 18650 cells
- [MQTT protocol](../mqtt-protocol.md) — battery metrics and alert format
- [Components inventory](../components.md) — battery hardware (BMS, cells, buck module)
- Configs using this module: [ESP+BMP280](../configs/esp-bmp280-battery.md),
  [ESP+SHT30](../configs/esp-sht30-battery.md), [MKR+ENV](../configs/mkr-env-battery.md)
