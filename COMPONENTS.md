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

| Component   | Quantity | Notes                                                                                            |
|-------------|----------|--------------------------------------------------------------------------------------------------|
| H78M05BT    | 9        | 5V linear regulator 500mA                                                                        |
| L78M09CV    | 10       | 9V linear regulator 500mA                                                                        |
| L78m33ACV   | 6        | 3.3V linear regulator 500mA                                                                      |
| 18650 cells | several  | Reclaimed from laptop packs, ~3000mAh, 3.7V nominal (see [battery guide](docs/battery-cells.md)) |

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

Two hardware configurations share the same Wemos D1 Mini and BME/BMP280 sensor.

### Display Config (USB-powered)

Powered via the Wemos D1 Mini USB port (5V). Includes the 7-segment display
circuit and a push button to cycle display modes. WiFi always active.

```
  USB 5V ──► Wemos D1 Mini USB port
                  │
                  ├──► 5V pin ──► LS247 x3, LS04 x2 (display logic)
                  │
                  ├──► 3V3 pin ──► BME/BMP280
                  │                HC595 x2
                  │
                  ├──► D7,D5,D8 ──► HC595 (shift registers) ──► display
                  │
                  ├──► D1 (GPIO5) ──► BME/BMP280 SCL (I2C)
                  ├──► D2 (GPIO4) ──► BME/BMP280 SDA (I2C)
                  │
                  └──► D6 (GPIO12) ──► push button ──► GND
                                       + 100nF debounce cap
```

#### Display config — Power Budget

| Subsystem             | Rail   | Avg current | Peak current |
|-----------------------|--------|-------------|--------------|
| ESP8266 (WiFi active) | 3.3V   | 80mA        | 300mA        |
| BME/BMP280            | 3.3V   | <1mA        | <1mA         |
| HC595 ×2              | 3.3V   | 1mA         | 1mA          |
| Display (3 digits)    | 5V     | 75mA        | 75mA         |
| LS247 ×3 + LS04 ×2    | 5V     | 50mA        | 50mA         |
| **Total from USB**    | **5V** | **~210mA**  | **~430mA**   |

Standard USB 2.0 provides 500mA — sufficient with margin.

#### Display config — GPIO Assignment

| Wemos Pin | GPIO   | Function                  |
|-----------|--------|---------------------------|
| D7        | GPIO13 | HC595 SER (shift data)    |
| D5        | GPIO14 | HC595 SRCLK (shift clock) |
| D8        | GPIO15 | HC595 RCLK (latch)        |
| D1        | GPIO5  | I2C SCL (BME/BMP280)      |
| D2        | GPIO4  | I2C SDA (BME/BMP280)      |
| D6        | GPIO12 | Push button (active LOW)  |

---

### Battery Config (Battery-powered, Deep Sleep)

Minimal sensor node. No display, no buttons. Powered by a 2S 18650 pack via
H78M05BT. Maximizes autonomy using ESP8266 deep sleep. Battery voltage is
reported as a sensor value via WiFi.

Requires **D0 (GPIO16) wired to RST** for deep sleep wake-up.

```
  [18650]──┬──[18650]──► 2S BMS ──► H78M05BT ──► 5V ──► Wemos 5V pin
   3.7V    └── series    (6.0-8.4V)     │
                                   [100nF in/out]
                                   [10µF/400V in]
                                        │
                          R1 (150kΩ) ───┤
                                        ├──► Wemos A0 (battery voltage)
                          R2 (100kΩ) ───┤
                                       GND

  Wemos D1 Mini
    ├──► D0 (GPIO16) ──► RST  (deep sleep wake-up)
    ├──► D1 (GPIO5)  ──► BME/BMP280 SCL (I2C)
    ├──► D2 (GPIO4)  ──► BME/BMP280 SDA (I2C)
    ├──► 3V3         ──► BME/BMP280 VCC
    └──► A0          ──► battery voltage divider
```

#### Battery config — Power Budget

| Subsystem         | Rail   | Active    | Deep sleep |
|-------------------|--------|-----------|------------|
| ESP8266           | 3.3V   | 80mA      | 20µA       |
| BME/BMP280        | 3.3V   | <1mA      | <1µA       |
| Voltage divider   | -      | 34µA      | 34µA       |
| **Total from 5V** | **5V** | **~81mA** | **~55µA**  |

#### Battery config — Deep Sleep Cycle

| Phase               | Duration | Current       | Energy per cycle |
|---------------------|----------|---------------|------------------|
| Wake + WiFi         | ~5s      | 81mA          | 0.113 mAh        |
| Deep sleep          | ~295s    | 55µA          | 0.005 mAh        |
| **Per 5 min cycle** |          | avg **1.4mA** | 0.118 mAh        |

**Autonomy**: 3000mAh / 1.4mA ≈ **2140 hours ≈ 89 days**

Adjustable via sleep interval:

| Interval | Avg current | Autonomy  |
|----------|-------------|-----------|
| 1 min    | 7.0 mA      | 18 days   |
| 5 min    | 1.4 mA      | 89 days   |
| 10 min   | 0.7 mA      | 178 days  |
| 30 min   | 0.25 mA     | ~1.4 year |

#### Battery config — GPIO Assignment

| Wemos Pin | GPIO   | Function                   |
|-----------|--------|----------------------------|
| D0        | GPIO16 | → RST (deep sleep wake-up) |
| D1        | GPIO5  | I2C SCL (BME/BMP280)       |
| D2        | GPIO4  | I2C SDA (BME/BMP280)       |
| A0        | ADC    | Battery voltage (divider)  |

#### Battery config — Battery Monitoring

Voltage divider R1=150kΩ / R2=100kΩ scales 2S voltage (6.0-8.4V) to Wemos A0 range
(0-3.3V). Quiescent current through divider: 8.4V / 250kΩ = 34µA.

| 2S Voltage | A0 reading | Approx. SoC |
|------------|------------|-------------|
| 8.40V      | 3.36V      | 100%        |
| 7.80V      | 3.12V      | 75%         |
| 7.40V      | 2.96V      | 50%         |
| 7.00V      | 2.80V      | 25%         |
| 6.00V      | 2.40V      | 0% (cutoff) |

Battery level is sent as a sensor value alongside temperature/humidity/pressure
via WiFi on each wake cycle.

#### Battery config — Parts to Buy

| Part            | Purpose                     | Approx. cost |
|-----------------|-----------------------------|--------------|
| 2S BMS module   | Cell balancing + protection | ~2€          |
| 2× 18650 holder | Or spot-weld a 2S pack      | ~2€          |

---

### MKR Config (Arduino MKR WiFi 1010 + MKR ENV Shield)

Compact sensor node with built-in WiFi and battery charging. The MKR ENV
Shield provides a BME280 sensor. A single 18650 cell (or any 1S LiPo 3.7V)
plugs into the MKR's JST PH 2-pin connector and charges automatically via USB.

No deep sleep mode — the MKR runs in continuous loop mode.

```
  [18650 cell] ──► JST connector ──► MKR WiFi 1010
   3.7V (1S)        (built-in             │
                     charge circuit)       ├──► WiFiNINA module (on-board)
                                           │
                                           ├──► MKR ENV Shield (stacked)
                                           │      └──► BME280 (I2C, default Wire)
                                           │
                                           └──► ADC_BATTERY (built-in voltage divider)
                                                  reads battery voltage
```

#### MKR config — Power Budget

| Subsystem               | Rail     | Active     | Idle (WiFi connected) |
|-------------------------|----------|------------|-----------------------|
| SAMD21                  | 3.3V     | 15mA       | 6mA                   |
| WiFiNINA (NINA-W102)    | 3.3V     | 120mA      | 80mA                  |
| BME280 (MKR ENV Shield) | 3.3V     | <1mA       | <1µA                  |
| **Total from battery**  | **3.7V** | **~140mA** | **~86mA**             |

**Autonomy** (continuous mode, 3000mAh cell): 3000 / 86 ≈ **35 hours** idle.
For longer autonomy, consider implementing SAMD21 low-power sleep via the
`ArduinoLowPower` library (future improvement).

#### MKR config — Battery Monitoring

The MKR WiFi 1010 has a built-in voltage divider on the `ADC_BATTERY` pin. The
firmware reads it with 12-bit resolution (0-4095). Battery constants in
`battery.h` are configured for 1S LiPo (3.0-4.2V).

| Battery voltage | Approx. SoC |
|-----------------|-------------|
| 4.20V           | 100%        |
| 3.90V           | 75%         |
| 3.70V           | 58%         |
| 3.50V           | 42%         |
| 3.00V           | 0% (cutoff) |

The `BATTERY_DIVIDER_RATIO` in `battery.h` is set to an approximate value
(2.0) and should be calibrated against a multimeter on the actual hardware.

#### MKR config — PlatformIO Environment

```ini
[env:thermo_mkr]
extends = common_samd
board = mkrwifi1010
build_flags =
    ${common_samd.build_flags}
    -DDEVICE_ID='"thermo_mkr"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_BME280
    -DHAS_BATTERY
    -DHAS_SERIAL_DEBUG
```
