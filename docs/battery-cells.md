# Reclaimed 18650 Cells — Testing, Grading, and Usage Guide

This project uses reclaimed 18650 cells from old laptop battery packs.
Not all cells are usable — some are degraded or dead. This document
describes how to test, grade, and safely use them.

## Safety first

- **Never short-circuit** a lithium cell (risk of fire/explosion).
- **Never charge** a cell below 2.5V — it may be internally damaged.
- **Never charge unattended** without a proper BMS/charger with protection.
- Work in a fire-safe area. Have a sand bucket or Class D extinguisher nearby.
- Wear safety glasses when disassembling packs.
- If a cell is **swollen, dented, leaking, or corroded** — discard it immediately.

## Equipment needed

This project includes a dedicated **cell tester firmware** for the Arduino
MKR WiFi 1010 that automates OCV measurement, charge monitoring, and capacity
testing. See the [Cell tester firmware](#cell-tester-firmware) section below.

For manual testing without the firmware:

| Tool                                 | Purpose                                   | Approx. cost |
|--------------------------------------|-------------------------------------------|--------------|
| Digital multimeter                   | Measure open-circuit voltage (OCV)        | ~15€         |
| TP4056 module (or similar)           | 1S LiPo charger, 1A, with DW01 protection | ~1€          |
| 18650 cell holder                    | Spring-loaded holder for testing          | ~2€          |
| Adjustable load (or 10Ω 5W resistor) | Capacity test discharge                   | ~3€          |
| Timer or capacity tester (optional)  | Automated mAh measurement (e.g. ZB2L3)    | ~5€          |

A dedicated capacity tester (ZB2L3 or similar) automates the process and
gives a direct mAh reading. Highly recommended if you have many cells.

## Step 1 — Visual inspection

Discard any cell that shows:

- Swelling or deformation
- Leaking electrolyte (white/brown crust around terminals)
- Deep rust or corrosion on the casing
- Dents that may have damaged the internal separator

## Step 2 — Open-circuit voltage (OCV) measurement

Measure the cell voltage with a multimeter, no load.

| OCV reading | Status          | Action                               |
|-------------|-----------------|--------------------------------------|
| 3.6V - 4.2V | Good            | Proceed to capacity test             |
| 2.5V - 3.6V | Discharged      | Charge slowly at 0.5A, then retest   |
| 1.0V - 2.5V | Deep discharged | Attempt recovery (see below)         |
| < 1.0V      | Dead or shorted | **Discard** — internal damage likely |

### Deep discharge recovery attempt

A cell between 1.0V and 2.5V *may* be recoverable:

1. Charge at very low current (100mA max) until it reaches 3.0V
2. If it reaches 3.0V within 2 hours, switch to normal 0.5A charging
3. If it does not reach 3.0V after 2 hours, **discard it**
4. After full charge, let it rest 1 hour and measure OCV
5. If OCV < 4.0V, the cell has high internal resistance — **discard it**

The [cell tester firmware](#cell-tester-firmware) automates this process:
it pre-charges deeply discharged cells via the BQ24195L and automatically
detects dead cells (voltage dropping or stalling during charge).

## Step 3 — Capacity test

This is the most important test. It tells you how much energy the cell
can actually store.

### Method with a capacity tester (ZB2L3)

1. Fully charge the cell to 4.2V
2. Place it in the tester, set cutoff to 2.8V
3. The tester discharges at a constant current and reports mAh
4. Wait for the test to complete (1-3 hours depending on cell)

### Manual method with a resistor

1. Fully charge the cell to 4.2V
2. Connect a **10Ω 5W resistor** across the terminals (~400mA discharge)
3. Start a timer
4. Measure voltage every 15-30 minutes
5. Stop when voltage reaches **2.8V** (do NOT go below)
6. Capacity (mAh) = average current (mA) x time (hours) x 1000

Example: 10Ω load at ~3.7V average = 370mA. If discharge lasts 6 hours:
370 x 6 = **2220 mAh**.

## Step 4 — Grading

| Grade | Capacity      | Internal resistance           | Use case                             |
|-------|---------------|-------------------------------|--------------------------------------|
| A     | > 2000 mAh    | Low (OCV stable after charge) | Primary cells for battery packs      |
| B     | 1000-2000 mAh | Medium                        | Low-drain applications (IoT sensors) |
| C     | 500-1000 mAh  | High                          | LED flashlights, non-critical use    |
| D     | < 500 mAh     | —                             | **Recycle** — not worth using        |

For this IoT project:

- **ESP8266 battery config** (2S): pair two cells of the **same grade and
  similar capacity** (within 10%). Use a 2S BMS for cell balancing.
- **MKR WiFi 1010** (1S): any single Grade A or B cell works directly.

### Self-discharge test (optional, for Grade A verification)

1. Fully charge the cell to 4.2V
2. Let it rest for **7 days** with no load
3. Measure OCV again
4. Acceptable: > 4.10V (< 2% loss per week)
5. If OCV < 4.0V after 7 days — downgrade to Grade B or discard

## Usage in this project

### ESP8266 battery config (2S pack)

Two matched cells in series (7.4V nominal) through a 2S BMS module:

```
[Cell A+]──[Cell B+]──► 2S BMS ──► H78M05BT ──► Wemos 5V pin
[Cell A-]──[Cell B-]──┘  (balance + protection)
```

**Important:** Always use a BMS with:

- Overcharge protection (4.25V per cell)
- Over-discharge protection (2.5V per cell)
- Short-circuit protection
- Cell balancing

### MKR WiFi 1010 (1S cell)

Single cell directly into the JST connector:

```
[Cell +] ──► JST PH 2-pin ──► MKR WiFi 1010
[Cell -] ──┘                   (built-in charge circuit via USB)
```

The MKR board handles charging when USB is connected. No external BMS
needed for 1S, but verify that the cell has a built-in protection circuit
(most reclaimed 18650 cells do NOT — consider adding a DW01+FS8205 module
between the cell and the JST connector for over-discharge protection).

## Labeling

Mark each tested cell with:

- Date tested
- Grade (A/B/C)
- Capacity in mAh
- Internal notes (e.g. "from HP pack #3")

Example: `2026-03 | B | 1850mAh | HP#3`

This makes it easy to pair cells for 2S packs and track degradation over time.

## Cell tester firmware

The project includes a standalone firmware (`src/cell_tester.cpp`) that turns
the Arduino MKR WiFi 1010 into an automated 18650 cell tester. It uses the
MKR's built-in BQ24195L PMIC for charging and the ADC_BATTERY pin for voltage
monitoring. No WiFi or MQTT — serial console only.

### What it does

1. **OCV measurement** — insert a cell into the JST connector, the firmware
   reads the open-circuit voltage and classifies the cell automatically
2. **Charge monitoring** — the BQ24195L charges the cell via USB. The firmware
   configures the PMIC for safe recovery of deeply discharged cells:
    - Pre-charge at 256mA for cells below 3.0V
    - Fast charge at 512mA above 3.0V
    - Watchdog and safety timer disabled (allows slow recovery)
3. **Discharge test** — once charged, connect an external load resistor
   (default 7.3Ω) and start the test. The firmware samples voltage every
   5 seconds, integrates current (trapezoidal method), and stops at 2.8V cutoff
4. **Grading** — reports capacity in mAh and assigns a grade (A/B/C/D)

### Build and upload

```bash
pio run -e cell_tester -t upload
pio device monitor
```

### Serial commands

| Command | Action                                                          |
|---------|-----------------------------------------------------------------|
| `H`     | Show help                                                       |
| `N`     | Reset for next cell (increments cell counter)                   |
| `D`     | Start discharge test (load resistor must be connected)          |
| `S`     | Stop discharge test manually                                    |
| `R<n>`  | Set load resistance in ohms (e.g. `R10`, `R4.7`). Default: 7.3Ω |
| `C`     | Show BQ24195 charger configuration and fault status             |

### Typical workflow

```
1. Flash firmware:     pio run -e cell_tester -t upload
2. Open serial:        pio device monitor
3. Insert cell into JST connector
4. Firmware reads OCV and classifies:
     < 1.0V  → DEAD, discard
     1-2.5V  → DEEP DISCHARGED, USB will pre-charge slowly
     2.5-3.6V → DISCHARGED, USB will fast-charge
     > 4.15V → FULLY CHARGED, ready for discharge test
5. Wait for "charge complete" status (monitor shows voltage + charge state)
6. Switch on the discharge resistor (see hardware setup below)
7. Send 'D' to start discharge test (firmware disables charging automatically)
8. Wait for cutoff (2.8V) — firmware reports capacity and grade
9. Switch off the discharge resistor
10. Send 'N' for next cell (firmware re-enables charging)
```

### Charge monitoring

During charging, the firmware displays a status line every 2 seconds with
elapsed time and voltage gain since the start:

```
  1.598V | 0m    | +0mV    | pre-charge (<3V)
  1.601V | 2m    | +3mV    | pre-charge (<3V)
  1.635V | 10m   | +37mV   | pre-charge (<3V)
  2.814V | 45m   | +1216mV | pre-charge (<3V)
  3.052V | 52m   | +1454mV | fast charging
  3.450V | 1h20m | +1852mV | fast charging
  4.182V | 3h12m | +2584mV | charge complete | FULL - send 'D' to test
```

The `(!)` marker appears if the charger reports "not charging" while the cell
is not yet full — this can mean USB is not connected or the PMIC has stopped
charging (fault condition). Use `C` to check for faults.

#### Automatic dead cell detection

The firmware checks charge health every 60 seconds and warns or flags
dead cells automatically:

| Condition                                             | Diagnostic             | Firmware message                              |
|-------------------------------------------------------|------------------------|-----------------------------------------------|
| Voltage drops >50mV below peak during charge          | Internal short-circuit | `CELL LIKELY DEAD (internal short) — discard` |
| Deep discharged cell (<2.5V) gains <50mV after 30 min | Cell not responding    | `CELL LIKELY DEAD — discard`                  |
| No voltage gain (<5mV) between checks, below 4.0V     | Charge stalled         | `WARNING: charge stalled`                     |

The first two conditions are strong indicators of a dead cell — discard
immediately (send `N` for next cell). The third is a warning that may clear
if progress resumes (e.g. temperature-related slowdown).

#### Expected charge rates (mV/min)

These are the typical voltage gain rates at 256mA pre-charge / 512mA fast
charge, as configured in the firmware. Use these to quickly judge if a cell
is viable by watching the monitoring output for a few minutes.

**Pre-charge phase (cell < 3.0V, 256mA):**

| Cell state               | Expected rate          | Verdict                                            |
|--------------------------|------------------------|----------------------------------------------------|
| Good cell, 1.5-2.5V      | 20-100 mV/min          | Normal — low capacity region, voltage rises fast   |
| Good cell, 2.5-3.0V      | 10-30 mV/min           | Normal — approaching plateau                       |
| Weak cell (low capacity) | 50-200 mV/min          | Voltage rises very fast = small capacity remaining |
| Dead cell                | < 5 mV/min or negative | Discard — internal short or irreversible damage    |

Rule of thumb for pre-charge: **if < 10 mV/min after 5 minutes, discard.**

**Fast charge phase (cell 3.0-4.2V, 512mA constant current):**

| Voltage range | Expected rate | Notes                                                 |
|---------------|---------------|-------------------------------------------------------|
| 3.0 - 3.3V    | 5-15 mV/min   | Leaving pre-charge zone                               |
| 3.3 - 3.8V    | 2-5 mV/min    | Li-ion plateau — voltage barely moves, this is normal |
| 3.8 - 4.0V    | 5-10 mV/min   | Rising toward CV threshold                            |
| 4.0 - 4.2V    | Decreasing    | CV phase — charger reduces current, rate slows to ~0  |

Rule of thumb for fast charge: **the 3.3-3.8V plateau is slow (2-5 mV/min),
don't confuse it with a stalled charge.** A stall is when voltage doesn't
move at all (<1 mV/min) for several minutes.

**Total charge time estimates (good cell ~2000mAh):**

| Starting voltage | Pre-charge | Fast charge (CC) | Terminal (CV) | Total   |
|------------------|------------|------------------|---------------|---------|
| 1.5V             | ~30-60 min | ~3-4h            | ~30-60 min    | ~4-6h   |
| 2.5V             | ~10-20 min | ~3-4h            | ~30-60 min    | ~3.5-5h |
| 3.5V             | —          | ~2h              | ~30-60 min    | ~2.5-3h |

A weak cell (500-1000mAh) will charge 2-4x faster and the voltage will rise
more rapidly in each phase — but it will also discharge faster in the capacity
test.

**Example: dead cell detected**

```
  1.598V | 0m   | +0mV  | pre-charge (<3V)
  1.601V | 2m   | +3mV  | pre-charge (<3V)
  1.595V | 10m  | -3mV  | pre-charge (<3V)
  1.590V | 20m  | -8mV  | pre-charge (<3V)

>>> WARNING: voltage DROPPING during charge <<<
    Peak was 1.601V, now 1.590V
>>> CELL LIKELY DEAD (internal short) — discard <<<

  1.588V | 22m  | -10mV | pre-charge (<3V)
```

When this happens, remove the cell and send `N` to proceed to the next one.

### Discharge test output example

```
>>> DISCHARGE TEST STARTED (charging disabled) <<<
Make sure 10.0 ohm resistor is connected!
Starting voltage: 4.182V
Cutoff: 2.8V

  0.1min | 4.175V | 418mA | 3mAh
  0.2min | 4.168V | 417mA | 7mAh
  ...
  312.5min | 2.801V | 280mA | 1847mAh

>>> CUTOFF REACHED <<<

╔══════════════════════════════════════╗
║       DISCHARGE TEST RESULTS         ║
╠══════════════════════════════════════╣
║  Cell #1
║  Start voltage:  4.182V
║  End voltage:    2.801V
║  Load:           10.0 ohm
║  Duration:       312.5 min
║  Capacity:       1847 mAh
║  Grade:          B (1000-2000mAh) - good for IoT
╚══════════════════════════════════════╝
```

### Hardware setup

![Cell Tester Setup](img/cell-tester-setup.svg)

#### Power supply

The BQ24195L charger requires sufficient USB current to charge cells,
especially in fast charge mode (512mA). **Not all USB ports are equal:**

| Source                   | Typical current    | Enough for fast charge?                  |
|--------------------------|--------------------|------------------------------------------|
| Laptop USB-A             | 500mA (often less) | Marginal — may stall at 3.0-3.8V plateau |
| Laptop USB-C             | 500mA-1.5A         | Usually OK                               |
| USB wall charger (phone) | 1-2A               | Best option                              |
| Desktop PC rear USB      | 500mA              | Usually OK                               |

**Recommended setup: dual USB** — one for serial, one for power. This gives
reliable charging current while keeping the serial monitor active.

```
PC (laptop/desktop)              USB wall charger (≥1A)
    │                                    │
    │ USB (data + limited power)         │ +5V / GND wires
    │                                    │
    ▼                                    ▼
MKR WiFi 1010 USB-C             MKR pin 5V ←── +5V
    (serial monitor)             MKR pin GND ←── GND
```

To get 5V from a USB wall charger: cut a USB cable and use the **red** (+5V)
and **black** (GND) wires. Connect to the MKR's **5V** and **GND** pins.
Do not exceed 5.5V.

If using a single USB port and the charge stalls (voltage doesn't progress),
try a different port or a wall charger. USB-C ports on laptops generally
provide more current than USB-A ports.

#### Discharge circuit

The recommended setup uses a **switch in series with the load resistor**,
wired in parallel across the cell. This allows charging and discharging
without unplugging anything.

```
MKR WiFi 1010
  JST + ─────────┬──────────────────────────────────┐
                 │                                  │
              cellule          [switch]───[7.3Ω 10W]
                 │                                  │
  JST - ─────────┴──────────────────────────────────┘
```

- **Switch OFF** → resistor disconnected, USB charges the cell normally
- **Switch ON** → resistor drains the cell for the discharge test

A simple toggle switch or even a jumper wire works. The charge circuit
(USB → BQ24195 → cell) passes through the JST connector and is completely
independent of the resistor path.

**Important:** the firmware automatically disables BQ24195 charging when
starting a discharge test (`D` command) and re-enables it when the test
completes. This prevents the charger from compensating the load during the
test, which would give false capacity readings.

**Important:** keep the switch **OFF** during charging. If the resistor is
connected while charging, it draws current that competes with the charger.
At low voltages (pre-charge at 256mA), the resistor may draw more than the
charger provides, making charge impossible.

#### Component choice

The current discharge resistor is a **Sfernice RH10 6.8Ω 10W 5%** (measured
7.3Ω with multimeter — use the measured value for accurate capacity readings).

| Resistor value | Current at 4.2V | Discharge time (2000mAh) | Power dissipated  |
|----------------|-----------------|--------------------------|-------------------|
| 4.7Ω           | 890mA           | ~2.5h                    | 3.8W (use 10W)    |
| **7.3Ω**       | **575mA**       | **~3.8h**                | **2.4W (10W OK)** |
| 10Ω            | 420mA           | ~5h                      | 1.8W (use 5W)     |
| 22Ω            | 190mA           | ~11h                     | 0.8W (use 2W)     |

Use a resistor rated at **least 2x the power dissipated** (safety margin).
The resistor gets warm during discharge — place it on a heat-resistant surface.

Change the firmware default with the `R` command if using a different value
(e.g. `R10` for a 10Ω resistor). The default is set to 7.3Ω in the firmware.

### Notes on deeply discharged cells

Cells below 2.5V are considered deeply discharged. The BQ24195L handles
these with a reduced pre-charge current (256mA). Expected behavior:

| Starting voltage | Time to 3.0V | Time to 4.2V | Total charge time |
|------------------|--------------|--------------|-------------------|
| 2.0 - 2.5V       | ~1-2h        | ~3-4h        | ~4-6h             |
| 1.0 - 2.0V       | ~2-4h        | ~4-6h        | ~6-10h            |

The firmware monitors charge progress automatically (see
[Automatic dead cell detection](#automatic-dead-cell-detection) above).
A cell that does not gain voltage after 30 minutes is flagged as dead.
Cells whose voltage *drops* during charge are flagged immediately as
having an internal short-circuit.

### BQ24195L configuration details

The firmware configures the PMIC at startup via I2C (address 0x6B):

| Register                | Value  | Purpose                                      |
|-------------------------|--------|----------------------------------------------|
| REG01 (Power-On Config) | `0x1A` | Enable charging, sys min 3.5V                |
| REG02 (Charge Current)  | `0x00` | 512mA fast charge (safe for reclaimed cells) |
| REG03 (Pre-Charge)      | `0x11` | 256mA pre-charge, 256mA termination          |
| REG04 (Charge Voltage)  | `0xB2` | 4.208V charge voltage                        |
| REG05 (Timer Control)   | `0x82` | Watchdog disabled, safety timer disabled     |

Use `C` command to verify configuration and check for faults at any time.

## Troubleshooting

### Charge stalls at 3.0-3.8V

The BQ24195L reports `fast charging` but the voltage doesn't progress.

**Cause:** insufficient USB current. In fast charge mode the BQ24195
requests 512mA. Laptop USB-A ports may not provide enough, especially on
battery power.

**Fix:** use a USB wall charger (≥1A) connected to the MKR's 5V pin, or
use a laptop USB-C port which typically provides more current. See the
[Power supply](#power-supply) section.

### "Cell removed" false triggers

The BQ24195L PMIC reconfigures when a cell is inserted or removed, which
can cause transient voltage readings. The firmware uses:

- **Raw voltage** for cell removal detection (not smoothed)
- **Sudden drop threshold** (>0.5V from last known value)
- **EMA-smoothed voltage** for display (reduces ADC noise)

If false triggers persist, the cell may have a poor contact with the JST
connector.

### Voltage readings are noisy (±5mV)

The SAMD21's 12-bit ADC has ~3mV of noise. The firmware applies an
exponential moving average (EMA, alpha=0.1) that smooths over ~20 seconds.
During the OCV settling phase, raw values are displayed for fast response.

If noise is problematic during charge monitoring, wait for the EMA to
settle (10-15 seconds after cell insertion).

### BQ24195 reports "not charging" with cell connected

Check with `C` command:

- **Faults: none, Charging: enabled** → USB port doesn't provide enough
  current. Try a different port or wall charger.
- **Faults: 0xXX** → PMIC fault. Remove cell, wait 5 seconds, reinsert.
  If fault persists, the cell may have an internal short.
- **Charging: DISABLED** → firmware bug or PMIC reset. Send `N` then
  reinsert the cell to reinitialize.

## Recycling

Cells graded D (< 500 mAh) or that fail testing should be recycled properly:

- EU: bring to any battery recycling point (supermarkets, electronics stores)
- Tape the terminals with electrical tape before transport to prevent short circuits
