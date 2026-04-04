# 2S Power Supply Module

## Overview

The 2S power supply provides regulated 5V from two 18650 lithium-ion cells in series. This is a **hardware-only
assembly** -- there is no firmware module or `HAS_` build flag. It powers battery-operated sensor nodes where USB is not
available.

**Power chain**: 2x 18650 (series) → BMS 2S (protection) → MP1584 (buck to 5V) → Wemos D1 Mini 5V pin
**Datasheet (5V regulator alternative):** [H78M05](../datasheets/H78M05AT.PDF)

| Parameter          | Value                                    |
|--------------------|------------------------------------------|
| Pack voltage       | 6.0-8.4V (2S nominal 7.4V)               |
| Regulated output   | 5.0V DC                                  |
| Max output current | 3A (MP1584 rated)                        |
| Typical load       | ~80mA average, ~350mA WiFi TX peaks      |
| Protection         | Overcharge, overdischarge, short circuit |

## Wiring

![2S Power Supply Wiring](../img/power-2s-wiring.svg)

## Schematic

![2S Power Supply Schematic](../img/power-2s-schematic.svg)

## Components

### BMS 2S -- JZK HX-2S

The BMS (Battery Management System) protects both cells from damage. It sits between the battery pack and the load.

| Parameter             | Value               |
|-----------------------|---------------------|
| Module                | JZK HX-2S           |
| Overcharge cutoff     | 4.25-4.35V per cell |
| Overdischarge cutoff  | 2.5-3.0V per cell   |
| Short circuit protect | Yes                 |
| Quiescent current     | < 10uA              |
| Continuous current    | 3A (typical)        |

#### BMS Pin Connections

| BMS Pad | Connects To               | Description                    |
|---------|---------------------------|--------------------------------|
| B-      | Cell #1 negative          | Pack ground (bottom of series) |
| BM      | Cell #1+ / Cell #2-       | Series mid-point               |
| B+      | Cell #2 positive          | Pack top (highest voltage)     |
| P-      | MP1584 IN- (and load GND) | Protected ground output        |
| P+      | MP1584 IN+                | Protected positive output      |

### MP1584 Buck Converter

Adjustable step-down converter that regulates the 6.0-8.4V pack voltage to a stable 5.0V for the Wemos.

| Parameter      | Value        |
|----------------|--------------|
| Input range    | 4.5-28V      |
| Output (set)   | 5.0V         |
| Max current    | 3A           |
| Efficiency     | ~90% typical |
| Switching freq | 1.5 MHz      |

#### Trimpot Adjustment

The MP1584 output voltage is set with an onboard trimpot. **Adjust before connecting the Wemos.**

1. Connect the BMS P+/P- outputs to MP1584 IN+/IN-.
2. With no load on OUT+/OUT-, measure voltage across OUT+ and OUT- with a multimeter.
3. Turn the trimpot **clockwise** to increase voltage, **counter-clockwise** to decrease.
4. Adjust to read **5.0V** (+/- 0.05V).
5. Verify under load (connect the Wemos) -- voltage should remain stable.

### Decoupling Capacitor -- 100uF Electrolytic

A 100uF electrolytic capacitor across the buck converter output filters switching noise. This is critical for stable
ESP8266 WiFi operation and clean ADC readings on A0 (battery voltage divider).

| Parameter   | Value            |
|-------------|------------------|
| Capacitance | 100uF            |
| Voltage     | >= 10V rated     |
| Type        | Electrolytic     |
| Placement   | Across OUT+/OUT- |

> **Note**: The 100uF cap should be placed as close as possible to the MP1584 output terminals.

## Output Wiring to Wemos

| MP1584 Output | Wemos Pin | Notes                 |
|---------------|-----------|-----------------------|
| OUT+          | 5V        | Regulated 5.0V supply |
| OUT-          | GND       | Common ground         |

> **Warning**: Connect to the Wemos **5V** pin, not 3V3. The onboard regulator steps down to 3.3V for the ESP8266.

## Battery Selection

Use matched 18650 cells -- same brand, capacity, and internal resistance. The cell tester firmware
(`pio run -e cell_tester`) measures capacity and grades cells. Pair cells from the same grade for balanced operation.

| Criteria         | Requirement             |
|------------------|-------------------------|
| Cell type        | 18650 Li-ion            |
| Capacity match   | Within 5% of each other |
| IR match         | Same grade from tester  |
| Min cell voltage | 2.5V (BMS cutoff)       |
| Max cell voltage | 4.2V (BMS cutoff)       |

## Safety Notes

- **Always use the BMS** -- never connect bare 18650 cells directly to the buck converter. The BMS prevents
  overdischarge (which permanently damages cells) and overcharge (fire risk).
- **Match cells** -- mismatched cells in series can cause one cell to be overdischarged while the other still has
  charge.
- **Adjust voltage first** -- set the MP1584 to 5.0V before connecting the Wemos. Excessive voltage can damage the
  board.
- **Polarity** -- double-check all connections before powering on. Reversed polarity on the BMS or buck converter can
  cause permanent damage.
- **Heat** -- the MP1584 may get warm under sustained load. Ensure adequate ventilation in the enclosure.
- **Charging** -- this module does not include charging circuitry. Remove cells for charging with an external charger.

## Related Documentation

- [Battery module (firmware)](battery.md) -- `HAS_BATTERY` flag for voltage monitoring
- [Deep sleep module](deep-sleep.md) -- one-shot mode for battery life extension
- [Battery cells guide](../battery-cells.md) -- testing and grading reclaimed 18650 cells
- [Components inventory](../components.md) -- BMS, buck module, regulator specs
- Configs using 2S power: [ESP+BMP280](../configs/esp-bmp280-battery.md),
  [ESP+SHT30](../configs/esp-sht30-battery.md)
