# Thermo

Modular IoT sensor node built with a Wemos D1 Mini v3.0.0 (ESP8266). Publishes
sensor data over MQTT to a [sensor_server](https://github.com/Silmaen/sensor_server)
instance. Hardware features are composed at compile time via feature flags.

## Modular architecture

A single `main.cpp` orchestrates all behavior. Hardware modules are enabled
via `-DHAS_xxx` build flags in `platformio.ini`. Device identity (`DEVICE_ID`,
`MQTT_DEVICE_TYPE`) is also defined there.

### Available modules

| Flag             | Description                        | MQTT metrics                          | MQTT commands |
|------------------|------------------------------------|---------------------------------------|---------------|
| `HAS_BME280`     | BME/BMP280 I2C sensor              | `temperature`, `humidity`, `pressure` | —             |
| `HAS_BATTERY`    | Battery voltage monitoring (ADC)   | `battery_pct`, `battery_v`            | —             |
| `HAS_DISPLAY`    | 3-digit 7-segment display + button | —                                     | —             |
| `HAS_DEEP_SLEEP` | Deep sleep between readings        | —                                     | —             |

Each module registers its metrics and commands into a `ModuleRegistry` at
startup. The capabilities message is built dynamically from this registry.

### Example configurations

```ini
; Wired sensor with display
[env:thermo1_display]
build_flags = -DDEVICE_ID='"thermo1"' -DMQTT_DEVICE_TYPE='"thermo"'
              -DHAS_BME280 -DHAS_DISPLAY

; Battery-powered sensor node
[env:thermo1_battery]
build_flags = -DDEVICE_ID='"thermo1b"' -DMQTT_DEVICE_TYPE='"thermo"'
              -DHAS_BME280 -DHAS_BATTERY -DHAS_DEEP_SLEEP

; Minimal sensor (no display, no battery)
[env:thermo2_minimal]
build_flags = -DDEVICE_ID='"thermo2"' -DMQTT_DEVICE_TYPE='"thermo"'
              -DHAS_BME280
```

See [COMPONENTS.md](COMPONENTS.md) for hardware details, wiring, and power budgets.

## MQTT communication

Devices communicate with the server via four MQTT topics:

```
{type}/{id}/sensors        Device -> Server   Sensor readings (JSON)
{type}/{id}/status         Device -> Server   Alerts: warning/error/ok (JSON)
{type}/{id}/command        Server -> Device   Commands (JSON)
{type}/{id}/capabilities   Device -> Server   Device identity & capabilities (JSON)
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
pio run -e thermo1_display     # Build display variant
pio run -e thermo1_battery     # Build battery variant
pio run -e native              # Build native (local debug)
```

### Upload & Monitor

```bash
pio run -e thermo1_display -t upload
pio device monitor                      # Serial monitor (115200 baud)
```

### Test

```bash
pio test -e native              # Run all unit tests (54 tests)
```

## Project structure

```
src/
  main.cpp                        Modular orchestrator (#ifdef HAS_xxx)
  hw/                             Hardware drivers (ESP8266-specific)
    esp_network.cpp/.h              WiFi + MQTT (PubSubClient)
    bme280_sensor.cpp/.h            BME/BMP280 I2C driver (custom, no Adafruit)
    shift_display.cpp/.h            7-segment display via shift registers
    battery_adc.cpp/.h              Battery voltage ADC reading
    esp_sleep.cpp/.h                Deep sleep + RTC memory

include/
  config.h                        MQTT topics, pin assignments, timing constants
  credentials.h                   WiFi & MQTT credentials (gitignored)
  credentials.h.example           Credentials template
  interfaces/                     Abstract interfaces

lib/thermo_core/                  Platform-independent, fully testable library
  src/
    module_registry.h/cpp           Module registration (metrics, commands, handlers)
    payload_builder.h/cpp           Incremental JSON payload construction
    mqtt_payload.h/cpp              Legacy payload formatting & command parsing
    modules/
      bme280_module.h/cpp           BME280 module (register + contribute)
      battery_module.h/cpp          Battery module (register + contribute)
    sensor_data.h                   SensorData struct
    battery.h/cpp                   ADC-to-voltage and voltage-to-SoC conversion
    display_encoding.h/cpp          BCD encoding for 7-segment display

test/test_native/                 Unit tests (Unity framework, 54 tests)
docs/                             MQTT protocol specification
datasheets/                       Component datasheets (BME280, regulators)
```

## Adding a new module

1. Create `lib/thermo_core/src/modules/<name>_module.h/cpp`:
    - `<name>_module_register(ModuleRegistry&)` — add metrics and/or commands
    - `<name>_module_contribute(PayloadBuilder&, ...)` — add fields to sensor payload
2. In `src/main.cpp`, add `#ifdef HAS_<NAME>` blocks for include, register, contribute
3. Add `-DHAS_<NAME>` to the relevant environments in `platformio.ini`
4. Add tests in `test/test_native/`

## Documentation

| Document                                       | Description                                                        |
|------------------------------------------------|--------------------------------------------------------------------|
| [docs/architecture.md](docs/architecture.md)   | Modular architecture, module system, data flow, how to add modules |
| [docs/mqtt-protocol.md](docs/mqtt-protocol.md) | MQTT protocol spec (topics, payloads, capabilities, lifecycle)     |
| [COMPONENTS.md](COMPONENTS.md)                 | Electronic components, wiring diagrams, GPIO, power budgets        |
| [CLAUDE.md](CLAUDE.md)                         | Development conventions, build commands, protocol rules summary    |

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
