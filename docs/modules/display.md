# 7-Segment Display Module

**Build flag:** `-DHAS_DISPLAY`
**Platform:** ESP8266 (Wemos D1 Mini) only

## Overview

The display module drives a 3-digit 7-segment display via two daisy-chained SN74HC595N shift registers. A push button
allows the user to cycle through display modes showing temperature, humidity, or pressure readings from the BME280
sensor. The entire 16-bit display state is shifted out over 3 GPIOs using a standard SER/SRCLK/RCLK protocol.

### Display Modes

| Mode        | Format | Example | Decimal point               |
|-------------|--------|---------|-----------------------------|
| Temperature | `XX.X` | `23.5`  | Between dig1 and dig2 (DP1) |
| Humidity    | `XX.X` | `65.2`  | Between dig1 and dig2 (DP1) |
| Pressure    | `XXX`  | `013`   | None (last 3 digits of hPa) |

Pressing the button cycles: Temperature -> Humidity -> Pressure -> Temperature -> ...

## Component List

| Component      | Part Number | Quantity | Description                         |
|----------------|-------------|----------|-------------------------------------|
| Shift register | SN74HC595N  | 2        | 8-bit serial-in, parallel-out, 3.3V |
| BCD decoder    | SN74LS247N  | 3        | BCD-to-7-segment, active-low, 5V    |
| Hex inverter   | SN54LS04J   | 2        | Hex inverter, 4 gates used each, 5V |
| 7-seg display  | 5082-7651   | 2        | Common-cathode, digits 0 and 1      |
| 7-seg display  | 5082-7653   | 1        | Common-anode, digit 2               |
| Voltage reg.   | H78M05BT    | 1        | 5V linear regulator (7-12V input)   |
| Resistor       | —           | 3        | 120 ohm, common pin current limit   |
| Capacitor      | —           | 1        | 100nF ceramic, button debounce      |
| Push button    | —           | 1        | Momentary, normally open            |

## Pin Usage

| D1 Mini Pin | GPIO | Function      | Direction | Connected to                               |
|-------------|------|---------------|-----------|--------------------------------------------|
| D7          | 13   | SER (data)    | Output    | HC595 #1 pin 14                            |
| D5          | 14   | SRCLK (clock) | Output    | HC595 #1 & #2 pin 11                       |
| D8          | 15   | RCLK (latch)  | Output    | HC595 #1 & #2 pin 12                       |
| D6          | 12   | Button        | Input     | Push button (active LOW, internal pull-up) |

## Wiring Diagram

![Display wiring diagram](../img/display-wiring.svg)

## Schematic

![Display schematic](../img/display-schematic.svg)

## Architecture

![Display shift register chain architecture](../img/display-architecture.svg)

### Why inverters on digits 0 and 1?

The SN74LS247N outputs are active-low (open-collector, designed to sink current). Digits 0 and 1 use
common-cathode displays (5082-7651), which need active-high segment drive. The SN54LS04J inverters convert
the LS247 active-low outputs to active-high for these CC displays. Digit 2 uses a common-anode display
(5082-7653), which works directly with the LS247 active-low outputs -- no inverter needed.

### Power domains

- **3.3V rail** (from D1 Mini regulator): HC595 shift registers only
- **5V rail** (from H78M05BT, 7-12V DC input): LS247 decoders, LS04 inverters, 7-segment displays
- The HC595 3.3V outputs are TTL-compatible with the 5V LS-series inputs (3.3V > V_IH = 2.0V for LS-TTL)

## Data Format

The firmware shifts 16 bits MSB-first. The first 8 bits go to HC595 #2, the second 8 bits to HC595 #1.

### Bit layout (as composed by `encode_display()`)

![Display 16-bit data format](../img/display-data-format.svg)

| Bits   | HC595 | Outputs | Destination                         |
|--------|-------|---------|-------------------------------------|
| [3:0]  | #1    | Q0-Q3   | LS04 #1 -> LS247 #1 -> Digit 0 (CC) |
| [7:4]  | #1    | Q4-Q7   | LS04 #2 -> LS247 #2 -> Digit 1 (CC) |
| [11:8] | #2    | Q0-Q3   | LS247 #3 -> Digit 2 (CA, direct)    |
| [12]   | #2    | Q4      | DP0 (decimal point digit 0)         |
| [13]   | #2    | Q5      | DP1 (decimal point digit 1)         |
| [14]   | #2    | Q6      | DP2 (decimal point digit 2)         |
| [15]   | #2    | Q7      | Unused                              |

### Shift sequence

1. Pull RCLK (latch) LOW
2. Shift out high byte (HC595 #2 data) MSB-first via `shiftOut()`
3. Shift out low byte (HC595 #1 data) MSB-first via `shiftOut()`
4. Pull RCLK HIGH -- both registers latch simultaneously

## Firmware Files

| File                                       | Role                                                                          |
|--------------------------------------------|-------------------------------------------------------------------------------|
| `src/hw/shift_display.h`                   | `ShiftDisplay` class declaration (implements `IDisplay`)                      |
| `src/hw/shift_display.cpp`                 | GPIO init and 16-bit shift-out implementation                                 |
| `lib/thermo_core/src/display_encoding.h`   | Encoding function declarations                                                |
| `lib/thermo_core/src/display_encoding.cpp` | BCD encoding for temperature, humidity, pressure                              |
| `include/config.h`                         | Pin definitions (`PIN_SR_DATA`, `PIN_SR_CLOCK`, `PIN_SR_LATCH`, `PIN_BUTTON`) |

## PlatformIO Environment

The display module is used in the `thermo_display` environment:

- `thermo_display` -- ESP8266 D1 Mini with BME280 + 7-segment display + USB power
