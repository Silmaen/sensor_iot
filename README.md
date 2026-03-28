# Thermo

Multi-platform modular IoT sensor node. Publishes sensor data over MQTT to a
[sensor_server](https://github.com/Silmaen/sensor_server) instance. Hardware
features are composed at compile time via feature flags.

## Supported platforms

| Platform | Board                 | WiFi                        | Notes                                             |
|----------|-----------------------|-----------------------------|---------------------------------------------------|
| ESP8266  | Wemos D1 Mini v3.0.0  | Built-in                    | Display, deep sleep, 2S LiPo battery              |
| SAMD21   | Arduino MKR WiFi 1010 | WiFiNINA (u-blox NINA-W102) | MKR ENV Shield (BME280), built-in 1S LiPo charger |

Platform selection is automatic based on the PlatformIO environment. The same
`main.cpp` compiles for both platforms using `#if defined(ESP8266)` /
`#elif defined(ARDUINO_SAMD_MKRWIFI1010)` guards.

## Modular architecture

A single `main.cpp` orchestrates all behavior. Hardware modules are enabled
via `-DHAS_xxx` build flags in `platformio.ini`. Device identity (`DEVICE_ID`,
`MQTT_DEVICE_TYPE`) is also defined there.

### Available modules

| Flag               | Description                        | MQTT metrics                          | Platform     |
|--------------------|------------------------------------|---------------------------------------|--------------|
| `HAS_BME280`       | BME/BMP280 I2C sensor              | `temperature`, `humidity`, `pressure` | All          |
| `HAS_BATTERY`      | Battery voltage monitoring (ADC)   | `battery_pct`, `battery_v`            | All          |
| `HAS_DISPLAY`      | 3-digit 7-segment display + button | —                                     | ESP8266 only |
| `HAS_DEEP_SLEEP`   | Deep sleep between readings        | —                                     | ESP8266 only |
| `HAS_SERIAL_DEBUG` | Verbose serial debug logging       | —                                     | All          |

Each module registers its metrics and commands into a `ModuleRegistry` at
startup. The capabilities message is built dynamically from this registry.

### Example configurations

```ini
; ESP8266: wired sensor with display
[env:thermo_display]
extends = common_esp8266
build_flags = ... -DHAS_BME280 -DHAS_DISPLAY -DHAS_SERIAL_DEBUG

; ESP8266: battery-powered deep sleep sensor
[env:thermo_1]
extends = common_esp8266
build_flags = ... -DHAS_BME280 -DHAS_BATTERY -DHAS_DEEP_SLEEP

; MKR WiFi 1010: BME280 + 1S LiPo battery
[env:thermo_mkr]
extends = common_samd
build_flags = ... -DHAS_BME280 -DHAS_BATTERY -DHAS_SERIAL_DEBUG
```

See [COMPONENTS.md](COMPONENTS.md) for hardware details, wiring, and power budgets.

## MQTT communication

Devices communicate with the server via MQTT topics:

```
{type}/{id}/sensors        Device -> Server   Sensor readings (JSON)
{type}/{id}/status         Device -> Server   Alerts: warning/error/ok (JSON)
{type}/{id}/command        Server -> Device   Commands (JSON)
{type}/{id}/capabilities   Device -> Server   Device identity & capabilities (JSON)
{type}/{id}/ack            Device -> Server   Command acknowledgement (JSON)
```

The server auto-discovers devices on first message. An admin must approve
a device before its data is stored. Online/offline status is computed
server-side from the last received message timestamp.

See [docs/mqtt-protocol.md](docs/mqtt-protocol.md) for the full protocol spec.

## Getting started

### Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html)
  or PlatformIO IDE

### Credentials

```bash
cp include/credentials.h.example include/credentials.h
# Edit with your WiFi SSID/password, MQTT server, optional MQTT auth
```

### Build

```bash
# ESP8266
pio run -e thermo_display       # Build display variant
pio run -e thermo_1             # Build battery/deep sleep variant

# SAMD21 (Arduino MKR WiFi 1010)
pio run -e thermo_mkr           # Build MKR variant (BME280 + 1S LiPo)

# Utilities
pio run -e cell_tester          # Build 18650 cell tester (MKR, serial only)

# Tests
pio run -e native               # Build native (local debug)
```

### Upload & Monitor

```bash
# ESP8266
pio run -e thermo_display -t upload
pio device monitor                      # Serial monitor (115200 baud)

# MKR WiFi 1010
pio run -e thermo_mkr -t upload
pio device monitor                      # Serial monitor (115200 baud, native USB)

# Cell tester
pio run -e cell_tester -t upload
pio device monitor                      # Interactive cell tester console
```

### Test

```bash
pio test -e native              # Run all unit tests (57 tests)
```

## Project structure

```
src/
  main.cpp                        Modular orchestrator (#ifdef HAS_xxx)
  cell_tester.cpp                 Standalone 18650 cell tester firmware (MKR)
  hw/                             Hardware drivers (platform-specific)
    esp_network.cpp/.h              ESP8266 WiFi + MQTT
    nina_network.cpp/.h             MKR WiFi 1010 WiFiNINA + MQTT
    bme280_sensor.cpp/.h            BME/BMP280 I2C driver (shared, custom)
    battery_adc.cpp/.h              ESP8266 battery ADC (10-bit)
    samd_battery_adc.cpp/.h         MKR battery ADC (12-bit, built-in divider)
    shift_display.cpp/.h            7-segment display via shift registers (ESP8266)
    esp_sleep.cpp/.h                Deep sleep + RTC memory (ESP8266)

include/
  config.h                        MQTT topics, pin assignments (per-platform), timing
  debug.h                         Portable serial debug macros
  credentials.h                   WiFi & MQTT credentials (gitignored)
  credentials.h.example           Credentials template
  interfaces/                     Abstract interfaces (INetwork, ISensor, ISleep, IDisplay)

lib/thermo_core/                  Platform-independent, fully testable library
  src/
    module_registry.h/cpp           Module registration (metrics, commands, handlers)
    payload_builder.h/cpp           Incremental JSON payload construction
    mqtt_payload.h/cpp              Payload formatting & command parsing
    modules/
      bme280_module.h/cpp           BME280 module (register + contribute)
      battery_module.h/cpp          Battery module (register + contribute)
    sensor_data.h                   SensorData struct
    battery.h/cpp                   ADC-to-voltage and voltage-to-SoC (per-platform constants)
    display_encoding.h/cpp          BCD encoding for 7-segment display

test/test_native/                 Unit tests (Unity framework, 57 tests)
docs/                             Architecture, MQTT protocol, battery guide
datasheets/                       Component datasheets
```

## Adding a new module

1. Create `lib/thermo_core/src/modules/<name>_module.h/cpp`:
    - `<name>_module_register(ModuleRegistry&)` — add metrics and/or commands
    - `<name>_module_contribute(PayloadBuilder&, ...)` — add fields to sensor payload
2. In `src/main.cpp`, add `#ifdef HAS_<NAME>` blocks for include, register, contribute
3. Add `-DHAS_<NAME>` to the relevant environments in `platformio.ini`
4. Add tests in `test/test_native/`

## Adding a new platform

1. Create a network driver `src/hw/<platform>_network.h/cpp` implementing `INetwork`
2. Add platform-specific pin/ADC definitions in `include/config.h`
3. Add `#elif defined(<PLATFORM_MACRO>)` branches in `src/main.cpp` for network,
   battery ADC, and hardware ID selection
4. Add `[common_<platform>]` section and device environment(s) in `platformio.ini`
5. If battery ADC differs, create `src/hw/<platform>_battery_adc.h/cpp` and add
   constants in `lib/thermo_core/src/battery.h`

## Documentation

| Document                                       | Description                                                        |
|------------------------------------------------|--------------------------------------------------------------------|
| [docs/architecture.md](docs/architecture.md)   | Modular architecture, module system, data flow, how to add modules |
| [docs/mqtt-protocol.md](docs/mqtt-protocol.md) | MQTT protocol spec (topics, payloads, capabilities, lifecycle)     |
| [docs/battery-cells.md](docs/battery-cells.md) | Reclaimed 18650 cell testing, grading, cell tester firmware usage  |
| [COMPONENTS.md](COMPONENTS.md)                 | Electronic components, wiring diagrams, GPIO, power budgets        |
| [CLAUDE.md](CLAUDE.md)                         | Development conventions, build commands, protocol rules summary    |

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
