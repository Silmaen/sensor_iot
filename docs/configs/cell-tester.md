# Cell Tester

MKR WiFi 1010 as an 18650 cell tester and charger. Serial only, no WiFi/MQTT.

## Overview

| Parameter      | Value                                              |
|----------------|----------------------------------------------------|
| MCU            | Arduino MKR WiFi 1010                              |
| Sensor         | --                                                 |
| Power          | External 5V USB wall charger                       |
| Charger        | BQ24195L (built-in on MKR)                         |
| Discharge load | Sfernice RH10 6.8 ohm 10W resistor + toggle switch |
| Connectivity   | Serial (115200 baud)                               |
| PlatformIO env | `cell_tester`                                      |

## Modules Used

None -- this is standalone firmware (`src/cell_tester.cpp`), separate from the MQTT
module system.

Reference: [battery-cells.md](../battery-cells.md) for the testing workflow and cell
grading criteria.

## Wiring

![Cell Tester Setup](../img/cell-tester-setup.svg)

- 18650 cell connects to the MKR's JST battery connector
- Toggle switch in series with the RH10 resistor between battery+ and battery-
- USB connected to a **5V wall charger** (not a laptop -- insufficient current for
  charging)

### GPIO Assignment

| Pin           | Function                                                |
|---------------|---------------------------------------------------------|
| `ADC_BATTERY` | Battery voltage reading (on-board divider, ratio 1.276) |
| `SDA` / `SCL` | I2C to BQ24195L PMIC (on-board)                         |

## PlatformIO Environment

```ini
[env:cell_tester]
platform = atmelsam
board = mkrwifi1010
framework = arduino
monitor_speed = 115200
build_flags = -Wall -Wextra -std=gnu++17 -DCELL_TESTER
src_filter = +<cell_tester.cpp>
lib_deps = Wire
```

## Build & Upload

```bash
pio run -e cell_tester
pio run -e cell_tester -t upload
pio device monitor
```

## Serial Commands

| Command | Action                                                 |
|---------|--------------------------------------------------------|
| `N`     | New cell (reset for next cell)                         |
| `D`     | Start discharge test (load resistor must be connected) |
| `S`     | Stop discharge test                                    |
| `R`     | Set load resistance in ohms (default 10)               |
| `H`     | Help                                                   |

## Notes

- **USB power source matters:** The BQ24195L draws up to 1A for charging. Laptop USB
  ports may not supply enough current, causing charge failures. Always use a dedicated
  5V wall charger.
- The firmware reads OCV, classifies the cell, monitors charge via PMIC registers, then
  integrates discharge current over time to compute capacity.
- No `HAS_xxx` flags -- the `CELL_TESTER` define and `src_filter` select this firmware
  exclusively.
