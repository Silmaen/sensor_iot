je vois 2 fichie# Available Electronic Components

Components listed here should be used in priority when designing circuits and features for this project.

## MCU

| Component              | Quantity | Notes                                                    |
|------------------------|----------|----------------------------------------------------------|
| Wemos D1 Mini v3.0.0   | 1        | ESP8266, for display and battery/deep-sleep configs      |
| Arduino MKR WiFi 1010  | 1        | SAMD21 + WiFiNINA, built-in LiPo charger (JST connector) |
| Arduino MKR ENV Shield | 1        | BME280 + UV + lux sensors, plugs on top of MKR board     |
| ESP32 CAM              | 2        | ESP32 + OV2640 camera, WiFi/BT, limited GPIO             |

## Sensors

| Component               | Quantity | Notes                                                                   |
|-------------------------|----------|-------------------------------------------------------------------------|
| bme/bmp280              | 1        | I2C temperature/humidity/pressure (standalone, for ESP8266 configs)     |
| SHT30 Shield v2.1.0     | several  | Wemos D1 Mini stackable shield, I2C 0x44/0x45, ±0.2°C / ±2%RH           |
| MKR ENV Shield BME280   | 1        | Same sensor, integrated on the shield (for MKR config)                  |
| Seeedstudio Light Sens. | 1        | Analog light sensor (photoresistor), outputs 0–3.3V proportional to lux |

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

![Display wiring diagram](img/display-wiring.svg)

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

![Display data format](img/display-data-format.svg)

Example for "23.5": send `0x2523` → dig0=2, dig1=3, dig2=5, DP1=ON (decimal between dig1 and dig2).

## Actuators

| Component              | Quantity | Notes                                                            |
|------------------------|----------|------------------------------------------------------------------|
| Dual Relay Board 10A   | 1        | 2-channel relay module, 5V coil, SPDT (NO+NC+COM), optocoupled  |

## Inputs

| Component   | Quantity | Notes                               |
|-------------|----------|-------------------------------------|
| Push button | 1        | Momentary, for display mode cycling |

The push button is used in Display config (with display) to cycle through display modes
(e.g. temperature, humidity, pressure). Connect to a GPIO with internal pull-up,
active LOW with 100nF debounce capacitor to GND.

## Breakout Boards

| Component                           | Quantity | Notes                                                   |
|-------------------------------------|----------|---------------------------------------------------------|
| Chronodot v2.1 (macetech)           | 1        | DS3231 RTC, ±2 ppm accuracy, I2C 0x68, coin cell backup |
| MicroSD Breakout Board+ (Adafruit)  | 1        | 5V-ready, SPI, level-shifted, supports FAT16/FAT32      |
| Ultimate GPS Breakout v3 (Adafruit) | 1        | MTK3339 chipset, -165 dBm, 10 Hz updates, 3.3–5V, UART  |

## Power

| Component          | Quantity | Notes                                                                                  |
|--------------------|----------|----------------------------------------------------------------------------------------|
| H78M05BT           | 9        | 5V linear regulator 500mA                                                              |
| L78M09CV           | 10       | 9V linear regulator 500mA                                                              |
| L78m33ACV          | 6        | 3.3V linear regulator 500mA                                                            |
| 18650 cells        | several  | Reclaimed from laptop packs, 3.7V nominal (see [battery guide](docs/battery-cells.md)) |
| BMS 2S (7.4V/8.4V) | 5        | JZK HX-2S, overcharge/overdischarge/short-circuit protection for 2S 18650              |
| MP1584 buck module | -        | Adjustable step-down 4.5–28V → 0.8–20V, 3A max, set to 5V for D1 Mini                  |
| 100µF electrolytic | -        | Bulk decoupling on buck output, filters ripple for ESP8266 WiFi + ADC                  |

### 18650 Cell References

| Reference         | Manufacturer      | Chemistry  | Nominal capacity | Voltage | Max discharge | Color  | Notes                     |
|-------------------|-------------------|------------|------------------|---------|---------------|--------|---------------------------|
| **UR18650ZT**     | Sanyo (Panasonic) | Li-ion NMC | 2800 mAh         | 3.6V    | ~5.6A (2C)    | Salmon |                           |
| **ICR18650-28A**  | Samsung           | Li-ion     | 2800 mAh         | 3.7V    | ~5.6A (2C)    | Violet |                           |
| **ICR18650-22FU** | Samsung           | Li-ion     | 2200 mAh         | 3.7V    | ~4.4A (2C)    | Green  |                           |
| **ICR18650-22HU** | Samsung           | Li-ion     | 2200 mAh         | 3.7V    | ~4.4A (2C)    | Green  |                           |
| **US18650GR**     | Sony Energy (SE)  | Li-ion     | 2200 mAh         | 3.7V    | ~4.4A (2C)    | Green  | Sony Energy (now Murata). |

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

See [Hardware Configurations](configurations.md) for detailed documentation of each
device assembly, including wiring diagrams, power budgets, GPIO assignments, and PlatformIO
environments.
