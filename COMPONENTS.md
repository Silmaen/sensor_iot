# Available Electronic Components

Components listed here should be used in priority when designing circuits and features for this project.

## MCU

| Component              | Quantity | Notes                                                    |
|------------------------|----------|----------------------------------------------------------|
| Wemos D1 Mini v3.0.0   | 1        | ESP8266, for display and battery/deep-sleep configs      |
| Arduino MKR WiFi 1010  | 1        | SAMD21 + WiFiNINA, built-in LiPo charger (JST connector) |
| Arduino MKR ENV Shield | 1        | BME280 + UV + lux sensors, plugs on top of MKR board     |

## Sensors

| Component             | Quantity | Notes                                                               |
|-----------------------|----------|---------------------------------------------------------------------|
| bme/bmp280            | 1        | I2C temperature/humidity/pressure (standalone, for ESP8266 configs) |
| SHT30 Shield v2.1.0   | several  | Wemos D1 Mini stackable shield, I2C 0x44/0x45, ±0.2°C / ±2%RH       |
| MKR ENV Shield BME280 | 1        | Same sensor, integrated on the shield (for MKR config)              |

## Display

| Component  | Quantity | Notes                                               |
|------------|----------|-----------------------------------------------------|
| SN54LS04J  | 2        | Hex inverter (6x NOT gates), TTL                    |
| SN74LS247N | 3        | BCD-to-7-segment decoder/driver, active-low outputs |
| SN74HC595N | 2        | 8-bit shift register with output latch, CMOS        |
| 5082-7651  | 2        | HP 7-segment common-cathode display, 10mm           |
| 5082-7653  | 1        | HP single-digit common-anode display, 10mm          |

### 3-Digit Display Wiring

Layout: `[5082-7651 #1] [5082-7651 #2] [5082-7653]` (common-cathode left & middle, common-anode right)

#### Architecture

Static display: one SN74LS247N decoder per digit, fed by two daisy-chained SN74HC595N
shift registers. The Wemos drives the chain with 3 GPIO pins.

The SN74LS247N has active-low open-collector outputs, designed for common-anode displays.
For the two common-cathode displays (5082-7651), an SN54LS04J inverter is placed between
the HC595 and the LS247 on each BCD line (4 inverters per digit, 8 total out of 12 available).

Decimal points are driven directly by spare HC595 outputs.

```
                         ┌───────────────────────────────────────────────────┐
                         │              5V rail (from H78M05BT)              │
                         │                                                   │
                         │                                        CA──[120Ω]─┤
                         │                                           │       │
                         │     ┌────────┐  ┌────────┐  ┌────────┐    │       │
                         │     │ 7651#1 │  │ 7651#2 │  │  7653  │    │       │
                         │     │  (CC)  │  │  (CC)  │  │  (CA)  │    │       │
                         │     └─┬┬┬┬┬┬┬┘  └─┬┬┬┬┬┬┬┘  └─┬┬┬┬┬┬┬┘    │       │
                         │       │││││││     │││││││     │││││││     │       │
                         │     a-g  DP     a-g  DP     a-g  DP       │       │
                         │       │││││││     │││││││     │││││││     │       │
                         │  ┌────┴┴┴┴┴┴┘  ┌──┴┴┴┴┴┴┘   ┌─┴┴┴┴┴┴┘     │       │
                         │  │  ┌──┴┘      │  ┌──┴┘     │  ┌──┴┘      │       │
                         │  │  │DP        │  │DP       │  │DP        │       │
                    CC───┼──┘  │     CC───┼──┘  │      │  │          │       │
                    [120Ω]     │     [120Ω]     │      │  │          │       │
                     │         │      │         │      │  │          │       │
                    GND        │     GND        │      │  │          │       │
                         │     │          │     │      │  │          │       │
                    ┌────┴─────┴┐   ┌─────┴─────┴┐   ┌─┴──┴──────┐   │       │
                    │  LS247 #1 │   │  LS247 #2  │   │  LS247 #3 │   │       │
                    └──┬┬┬┬─────┘   └──┬┬┬┬──────┘   └──┬┬┬┬─────┘   │       │
                       ││││            ││││             ││││         │       │
                    ┌──┴┴┴┴──┐      ┌──┴┴┴┴──┐          ││││         │       │
                    │ LS04#1 │      │ LS04#2 │          ││││         │       │
                    │ (inv)  │      │ (inv)  │          ││││(direct) │       │
                    └──┬┬┬┬──┘      └──┬┬┬┬──┘          ││││         │       │
                       ││││            ││││             ││││         │       │
               ┌───────┴┴┴┴────────────┴┴┴┴─────────────┴┴┴┴──┐      │       │
               │  HC595 #1                                    │      │       │
               │  Q0-Q3 → LS04#1 → LS247#1 (dig0, CC left)    │      │       │
               │  Q4-Q7 → LS04#2 → LS247#2 (dig1, CC middle)  │      │       │
               │  QH' ──────────────────────────────┐         │      │       │
               └──────┬─┬───────────────────────────┼─────────┘      │       │
                      │ │                           │                │       │
               ┌──────┴─┴───────────────────────────┼─────────┐      │       │
               │  HC595 #2                          │         │      │       │
               │  SER ◄─────────────────────────────┘         │      │       │
               │  Q0-Q3 → LS247#3 (dig2, CA right, direct)    │      │       │
               │  Q4 → DP digit 0                             │      │       │
               │  Q5 → DP digit 1                             │      │       │
               │  Q6 → DP digit 2                             │      │       │
               │  Q7 → (unused)                               │      │       │
               └──────┬─┬─────────────────────────────────────┘      │       │
                      │ │                                            │       │
 Wemos D1 Mini        │ │                                            │       │
 ┌────────────┐       │ │                                            │       │
 │      D7────┼───────┼─┼──► SER  (HC595 #1 pin 14)                  │       │
 │      D5────┼───────┼─┼──► SRCLK (HC595 #1 & #2 pin 11)            │       │
 │      D8────┼───────┼─┘─► RCLK  (HC595 #1 & #2 pin 12)             │       │
 │     3V3────┼───────┼───► VCC HC595 #1 & #2 (pin 16)               │       │
 │      5V────┼───────┘───► VCC LS247 x3 + LS04 x2 (pin 14/16)───────┘       │
 │     GND────┼───────────► GND all ICs + OE HC595 (pin 13)                  │
 │            │            + LT & RBI LS247 → 5V (pins 3, 5)                 │
 └────────────┘                                                              │
                                                                             │
                 DC in (7-12V) ──► H78M05BT ──► 5V rail ─────────────────────┘
                                   (+ 100nF ceramic in/out, 10µF/400V out)
```

#### Wemos → Shift Registers (SN74HC595N)

| Wemos Pin   | HC595 Pin        | Signal | Notes                            |
|-------------|------------------|--------|----------------------------------|
| D7 (GPIO13) | #1 SER (14)      | Data   | Serial data input                |
| D5 (GPIO14) | #1+#2 SRCLK (11) | Clock  | Shift clock (shared)             |
| D8 (GPIO15) | #1+#2 RCLK (12)  | Latch  | Storage register clock (shared)  |
| 3V3         | #1+#2 VCC (16)   | Power  | HC595 powered at 3.3V            |
| GND         | #1+#2 GND (8)    | Ground |                                  |
| GND         | #1+#2 OE (13)    | Enable | Tied LOW = outputs always active |

Daisy-chain: HC595 #1 QH' (pin 9) → HC595 #2 SER (pin 14).

#### Shift Registers → Inverters → Decoders

**Common-cathode digits (5082-7651, left & middle):**

| HC595 Output | LS04              | LS247 Input | Digit                |
|--------------|-------------------|-------------|----------------------|
| #1 Q0-Q3     | LS04 #1 (4 gates) | #1 A-D      | Digit 0 (left, CC)   |
| #1 Q4-Q7     | LS04 #2 (4 gates) | #2 A-D      | Digit 1 (middle, CC) |

**Common-anode digit (5082-7653, right):**

| HC595 Output | LS247 Input | Digit                       |
|--------------|-------------|-----------------------------|
| #2 Q0-Q3     | #3 A-D      | Digit 2 (right, CA, direct) |

**Decimal points (direct from HC595 #2):**

| HC595 #2 Output | Target     |
|-----------------|------------|
| Q4              | DP digit 0 |
| Q5              | DP digit 1 |
| Q6              | DP digit 2 |
| Q7              | (unused)   |

Inverter usage: 8 of 12 gates used (4 spare in LS04 #2).

#### Decoders → Displays

Each LS247N segment output (a-g) connects directly to the corresponding segment
pin of the display. Single **120Ω resistor** on the common pin of each digit:

- CC digits (7651): common cathode → 120Ω → GND
- CA digit (7653): common anode → 120Ω → 5V

Each SN74LS247N: VCC (pin 16) → 5V, GND (pin 8) → GND, LT (pin 3) → 5V, RBI (pin 5) → 5V.
Each SN54LS04J: VCC (pin 14) → 5V, GND (pin 7) → GND.

#### Voltage Levels

- HC595 at 3.3V: outputs 3.3V HIGH, compatible with LS04/LS247 TTL input (VIH min = 2V).
- LS04 and LS247 at 5V: TTL logic levels.
- LS247: open-collector active-low outputs sink current through display LEDs.

#### Power Supply

- DC input (7-12V) → H78M05BT → 5V rail
- Decoupling: 100nF ceramic on regulator input and output
- Bulk capacitor: **10µF / 400V** on power input line

#### Data Format

Shift 16 bits MSB-first (HC595 #2 data first, then HC595 #1):

```
Bit:  [15] [14] [13] [12]  [11..8]   [7..4]    [3..0]
       --   DP2  DP1  DP0   dig2      dig1      dig0
      free               (BCD,CA)  (BCD,CC)  (BCD,CC)
```

Example for "23.5": send `0x2523` → dig0=2, dig1=3, dig2=5, DP1=ON (decimal between dig1 and dig2).

## Inputs

| Component   | Quantity | Notes                               |
|-------------|----------|-------------------------------------|
| Push button | 1        | Momentary, for display mode cycling |

The push button is used in Display config (with display) to cycle through display modes
(e.g. temperature, humidity, pressure). Connect to a GPIO with internal pull-up,
active LOW with 100nF debounce capacitor to GND.

## Power

| Component          | Quantity | Notes                                                                                            |
|--------------------|----------|--------------------------------------------------------------------------------------------------|
| H78M05BT           | 9        | 5V linear regulator 500mA                                                                        |
| L78M09CV           | 10       | 9V linear regulator 500mA                                                                        |
| L78m33ACV          | 6        | 3.3V linear regulator 500mA                                                                      |
| 18650 cells        | several  | Reclaimed from laptop packs, ~3000mAh, 3.7V nominal (see [battery guide](docs/battery-cells.md)) |
| BMS 2S (7.4V/8.4V) | 5        | JZK HX-2S, overcharge/overdischarge/short-circuit protection for 2S 18650                        |
| MP1584 buck module | -        | Adjustable step-down 4.5–28V → 0.8–20V, 3A max, set to 5V for D1 Mini                            |
| 100µF electrolytic | -        | Bulk decoupling on buck output, filters ripple for ESP8266 WiFi + ADC                            |

## Cell Tester

Components for the 18650 cell tester (see [battery guide](docs/battery-cells.md)):

| Component                           | Quantity | Notes                                                   |
|-------------------------------------|----------|---------------------------------------------------------|
| Arduino MKR WiFi 1010               | 1        | Provides BQ24195L charger + ADC_BATTERY voltage reading |
| Sfernice RH10 6.8Ω 10W (meas. 7.3Ω) | 1        | Discharge load (~575mA at 4.2V, ~3.8h for 2000mAh cell) |
| Toggle switch                       | 1        | In series with resistor to enable/disable discharge     |
| USB wall charger (≥1A)              | 1        | External 5V power for reliable charging (on MKR 5V pin) |

---

## Device Configurations

See [Hardware Configurations](docs/configurations.md) for detailed documentation of each
device assembly, including wiring diagrams, power budgets, GPIO assignments, and PlatformIO
environments.
