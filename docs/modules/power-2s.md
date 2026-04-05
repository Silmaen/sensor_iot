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

The 5V regulator converts the 6.0--8.4V pack voltage to a stable 5.0V for the Wemos 5V pin.

Two regulators have been used in this project:

| Regulator    | Type   | Iq (quiescent)  | Efficiency @80mA | Status              |
|--------------|--------|-----------------|-------------------|---------------------|
| MC78M05BTG   | LDO    | **~3 mA**       | ~68%              | Legacy (high drain) |
| HT7350       | LDO    | **~4 µA**       | ~68%              | **Recommended**     |

> **Important:** The MC78M05BTG draws ~3 mA even with no load, which dominates power consumption
> during deep sleep. Replacing it with an HT7350 reduces sleep current by **~750x** on the
> regulator alone. Both are TO-92 packages — drop-in mechanical replacement.

#### HT7350 (recommended)

| Parameter      | Value             |
|----------------|-------------------|
| IC             | Holtek HT7350     |
| Type           | LDO (linear)      |
| Input range    | 5.1--12V          |
| Output         | 5.0V fixed        |
| Max current    | 250 mA            |
| Dropout        | 100 mV @ 40 mA    |
| Quiescent      | ~4 µA (typ.)      |
| Package        | TO-92             |

Pin order (facing flat side, left to right): **GND — Vout — Vin**.

> **Note:** The HT7350 maximum current (250 mA) is sufficient for the ESP8266 average consumption
> (~80 mA), including WiFi TX peaks (~170 mA sustained). Brief 350 mA TX spikes are absorbed by
> the 100 µF output capacitor. If TX peak dropouts are observed, add a second 100 µF cap.

#### MC78M05BTG (legacy)

| Parameter      | Value                |
|----------------|----------------------|
| Input range    | 7--35V               |
| Output         | 5.0V                 |
| Max current    | 500 mA               |
| Dropout        | ~2V                  |
| Quiescent      | ~3 mA (typ.)         |
| Package        | TO-252 / TO-92       |

> **Warning:** Dropout voltage is ~2V, meaning the regulator stops regulating below ~7V input.
> With a 2S pack that reaches 6.0V at empty, the last ~15% of battery capacity is unusable.
> The HT7350 (100 mV dropout) works down to 5.1V input, using the full pack range.

#### MP1584 Buck Module (alternative)

For non-battery configs or high-current loads, the MP1584 buck converter is more efficient
under sustained load (~90%) but has ~500 µA quiescent current.

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

## Regulator Comparison

The MP1584 buck module is cheap and efficient under load, but its **~500 µA quiescent current** dominates
power consumption during deep sleep. This table compares alternatives for battery-powered nodes where
sleep current matters.

### Buck Converters (step-down, high efficiency under load)

| Module/IC      | Vin range  | Vout   | Iq (quiescent) | Imax | Efficiency | Form       | Price  | Notes                                 |
|----------------|------------|--------|----------------|------|------------|------------|--------|---------------------------------------|
| **MP1584**     | 4.5-28V    | Adj.   | ~500 µA        | 3A   | ~90%       | Module     | <1 EUR | Current choice. High Iq.              |
| **TPS62203**   | 3-17V      | 3.3V   | **~15 µA**     | 300mA | 90%       | SOT-23-5   | ~2 EUR | Excellent Iq. Needs 3.3V output.      |
| **TPS62200**   | 3-17V      | Adj.   | **~15 µA**     | 300mA | 90%       | SOT-23-5   | ~2 EUR | Adjustable version. Needs ext R.      |
| **RT8059**     | 2.5-5.5V   | Adj.   | ~30 µA         | 600mA | 92%       | SOT-23-5   | ~1 EUR | Vin too low for 2S direct.            |
| **AP63203**    | 3.8-32V    | Adj.   | ~22 µA         | 2A   | 93%       | TSOT-26    | ~1 EUR | Good middle ground. SMD only.         |
| **MCP16331**   | 4-50V      | Adj.   | **~10 µA**     | 500mA | 89%       | SOT-23-6   | ~2 EUR | Lowest Iq buck. Wide Vin.             |

### LDO Regulators (linear, simpler but less efficient)

| IC             | Vin max | Vout   | Iq (quiescent) | Imax  | Dropout | Package  | Notes                                    |
|----------------|---------|--------|----------------|-------|---------|----------|------------------------------------------|
| **MCP1700**    | 6.0V    | 3.3V   | **~1.6 µA**    | 250mA | 178 mV  | TO-92    | Ultra-low Iq. Vin too low for 2S.        |
| **HT7333**     | 12V     | 3.3V   | **~4 µA**      | 250mA | 100 mV  | TO-92    | Works with 2S! Very low Iq.              |
| **HT7350**     | 12V     | 5.0V   | **~4 µA**      | 250mA | 100 mV  | TO-92    | 5V output for Wemos 5V pin.              |
| **XC6220**     | 6V      | 3.3V   | ~8 µA          | 700mA | 100 mV  | SOT-25   | Vin marginal for 2S (max 6V < 8.4V).     |
| **AP2112**     | 6V      | 3.3V   | ~55 µA         | 600mA | 250 mV  | SOT-23-5 | Common on breakouts. Still high Iq.      |
| **ME6211**     | 6V      | 3.3V   | ~40 µA         | 500mA | 100 mV  | SOT-23-5 | Vin too low for 2S.                      |
| H78M05BT       | 35V     | 5.0V   | ~3 mA          | 500mA | 2V      | TO-252   | Current stock. Way too high Iq for batt. |

### Recommendations for 2S Battery Nodes

**Drop-in replacement:** Replace the **MC78M05BTG** with an **HT7350**. Both are TO-92 — same
footprint, no PCB change.

| Criteria         | MC78M05BTG (legacy) | HT7350 (recommended)   |
|------------------|---------------------|-------------------------|
| Iq               | **~3000 µA**        | **~4 µA**               |
| Efficiency @80mA | ~68%                | ~68%                    |
| Dropout          | ~2V (loses 15% cap) | 100 mV (uses full pack) |
| Form factor      | TO-92               | TO-92                   |
| Vin range        | 7-35V               | 5.1-12V (OK for 2S)    |
| Cost             | ~0.5 EUR            | ~0.5 EUR                |

**Autonomy comparison** (2S 2200 mAh, 5 min deep sleep cycle, 4.5s active):

| | MC78M05BTG | MC78M05BTG + MOSFET switch | HT7350 + MOSFET switch |
|---|---|---|---|
| Sleep: regulator Iq | 3000 µA | 3000 µA | **4 µA** |
| Sleep: divider drain | 247 µA | **0 µA** | **0 µA** |
| Sleep: ESP8266 | 20 µA | 20 µA | 20 µA |
| **Total sleep current** | **3267 µA** | **3020 µA** | **24 µA** |
| Sleep charge (295.5s) | 965 mA·s | 893 mA·s | **7 mA·s** |
| Active charge (4.5s) | 356 mA·s | 356 mA·s | 356 mA·s |
| **Average current** | **4.40 mA** | **4.16 mA** | **1.21 mA** |
| **Autonomy (2200 mAh)** | **21 jours** | **22 jours** | **76 jours** |

> The MC78M05BTG also has a 2V dropout, meaning the pack must stay above ~7V for regulation.
> This wastes the bottom ~15% of 2S capacity (6.0-7.0V range). The HT7350 works down to 5.1V
> input, using the **full** battery range — effectively another ~15% gain on top of the Iq savings.

**Best upgrade (advanced, SMD):** Use an **MCP16331** buck (10 µA Iq, 89% efficient) — best of
both worlds but requires SMD soldering and external inductor/capacitors.

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
