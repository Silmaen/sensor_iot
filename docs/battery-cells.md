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

## Recycling

Cells graded D (< 500 mAh) or that fail testing should be recycled properly:

- EU: bring to any battery recycling point (supermarkets, electronics stores)
- Tape the terminals with electrical tape before transport to prevent short circuits
