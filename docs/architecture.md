# Architecture

This document describes the modular architecture of the thermo firmware.

## Design goals

- **Multi-platform** — supports ESP8266 (D1 Mini) and SAMD21 (MKR WiFi 1010)
  from a single codebase. Platform selection is automatic via compiler macros.
- **Compose, don't copy** — hardware variants are assembled from reusable
  modules, not duplicated config files.
- **No dynamic allocation** — all data structures use fixed-size arrays
  allocated at compile time (ESP8266: 80 KB RAM, SAMD21: 32 KB RAM).
- **Testable core** — all logic in `lib/thermo_core/` compiles natively
  and runs in Unity tests without Arduino dependencies.
- **Self-describing** — the capabilities message sent to the server is
  built dynamically from the enabled modules, not hardcoded.

## Layers

![Architecture Layers](img/architecture-layers.svg)

## Feature flags

Feature flags are defined as `-DHAS_xxx` in `platformio.ini` build flags.
They control which code is compiled into the firmware via `#ifdef` guards.

| Flag             | What it enables                                                 |
|------------------|-----------------------------------------------------------------|
| `HAS_BME280`     | BME/BMP280 I2C sensor initialization, reading, metrics          |
| `HAS_SHT30`      | SHT30 I2C sensor initialization, reading, metrics (no pressure) |
| `HAS_BATTERY`    | Battery ADC reading, SoC calculation, low-battery alert         |
| `HAS_DISPLAY`    | 7-segment display, push button, display mode cycling            |
| `HAS_DEEP_SLEEP` | One-shot execution mode with ESP8266 deep sleep (ESP8266 only)  |

The `NATIVE` flag is set automatically for the native test build. Code
inside `#ifndef NATIVE` is excluded from tests (hardware drivers, Arduino
calls, etc.).

### Platform selection macros

Platform-specific code is selected at compile time using compiler-defined macros:

| Macro                      | Set by                                | Used for                            |
|----------------------------|---------------------------------------|-------------------------------------|
| `ESP8266`                  | ESP8266 Arduino core                  | ESP-specific drivers, pins, chip ID |
| `ARDUINO_SAMD_MKRWIFI1010` | SAMD Arduino core (board=mkrwifi1010) | MKR-specific drivers, pins, chip ID |
| `NATIVE`                   | `-DNATIVE` build flag                 | Exclude all hardware code in tests  |

In `main.cpp` and `config.h`, platform selection follows this pattern:

```c++
#if defined(ARDUINO_SAMD_MKRWIFI1010)
    // MKR WiFi 1010 specific
#elif defined(ESP8266)
    // ESP8266 specific
#endif
```

### Device identity flags

| Flag               | Example                         | Purpose                    |
|--------------------|---------------------------------|----------------------------|
| `DEVICE_ID`        | `-DDEVICE_ID='"thermo_1"'`      | Unique network identifier  |
| `MQTT_DEVICE_TYPE` | `-DMQTT_DEVICE_TYPE='"thermo"'` | Device category for topics |

These are used by the MQTT topic macros in `config.h`:

```text
thermo/thermo_1/sensors
thermo/thermo_1/status
thermo/thermo_1/command
thermo/thermo_1/capabilities
```

Note the quoting: single quotes wrap double quotes so the preprocessor
receives a string literal (`'"thermo_1"'` → `"thermo_1"`).

## Execution modes

### Continuous mode (no `HAS_DEEP_SLEEP`)

![Continuous execution mode](img/architecture-continuous.svg)

### One-shot mode (`HAS_DEEP_SLEEP`)

![One-shot execution mode](img/architecture-one-shot.svg)

## Module system

### ModuleRegistry

The `ModuleRegistry` is a simple struct with fixed-size arrays:

```c++
struct ModuleRegistry {
    const char* metrics[MAX_METRICS];   // up to 8
    const char* commands[MAX_COMMANDS]; // up to 4
    CommandHandler handlers[MAX_COMMANDS];
    size_t num_metrics, num_commands;

    bool add_metric(const char* name);
    bool add_command(const char* name, CommandHandler handler);
    bool dispatch(const char* action, const char* payload) const;
};
```

No heap allocation, no virtual dispatch. Total size: ~100 bytes.

### Module contract

Each module provides two functions:

**`<name>_module_register(ModuleRegistry& reg)`**
Called once at startup. Adds metric names and/or command names + handlers
to the registry. This determines what appears in the `capabilities` message.

**`<name>_module_contribute(PayloadBuilder& pb, ...)`**
Called each time a sensor payload is built. Adds the module's key-value
pairs to the JSON payload.

Example for a hypothetical DHT22 module:

```c++
// lib/thermo_core/src/modules/dht22_module.h
void dht22_module_register(ModuleRegistry& reg);
void dht22_module_contribute(PayloadBuilder& pb, float temp, float humidity);

// lib/thermo_core/src/modules/dht22_module.cpp
void dht22_module_register(ModuleRegistry& reg) {
    reg.add_metric("temperature");
    reg.add_metric("humidity");
}

void dht22_module_contribute(PayloadBuilder& pb, float temp, float humidity) {
    pb.add_float("temperature", temp, 1);
    pb.add_float("humidity", humidity, 1);
}
```

### PayloadBuilder

Builds JSON incrementally into a caller-provided buffer:

```c++
char buf[256];
PayloadBuilder pb{buf, sizeof(buf), 0, true};
pb.begin();                              // writes {
pb.add_float("temperature", 22.5, 1);   // writes "temperature":22.5
pb.add_uint("battery_pct", 75);         // writes ,"battery_pct":75
pb.end();                                // writes }
// buf = {"temperature":22.5,"battery_pct":75}
```

Uses `snprintf` internally — same approach as the legacy format functions,
but composable across modules.

### Command dispatch

When an MQTT command arrives:

![Command dispatch](img/architecture-command-dispatch.svg)

Modules can register their own commands. The core always registers
`set_interval`. A module adding a custom command:

```c++
static bool handle_calibrate(const char* payload) {
    // parse payload, apply calibration...
    return true;
}

void my_module_register(ModuleRegistry& reg) {
    reg.add_metric("my_metric");
    reg.add_command("calibrate", handle_calibrate);
}
```

The `calibrate` command will appear in the capabilities message and
the device will handle `{"action":"calibrate","value":...}` from the server.

### Capabilities message

Built dynamically at runtime from `ModuleRegistry`:

```c++
format_capabilities_payload(
    hw_id,                     // ESP.getChipId() as hex
    publish_interval_s,        // current interval
    registry.metrics,          // array from all registered modules
    registry.num_metrics,
    registry.commands,         // array from all registered handlers
    registry.num_commands,
    buf, sizeof(buf)
);
```

Example output with `HAS_BME280` + `HAS_BATTERY`:

```json
{
  "hardware_id": "ESP-00A1B2",
  "publish_interval": 300,
  "metrics": [
    "temperature",
    "humidity",
    "pressure",
    "battery_pct",
    "battery_v"
  ],
  "commands": [
    "set_interval"
  ]
}
```

## Data flow: sensor publish cycle

![Sensor publish data flow](img/architecture-data-flow.svg)

## Adding a new module: step by step

### 1. Create the module in thermo_core

```text
lib/thermo_core/src/modules/<name>_module.h
lib/thermo_core/src/modules/<name>_module.cpp
```

Implement `<name>_module_register()` and `<name>_module_contribute()`.
If the module accepts commands, add a `CommandHandler` function and
register it with `reg.add_command()`.

### 2. Create or reuse a hardware driver

If the module needs new hardware, add a driver in `src/hw/`:

```c++
src/hw/<name>_driver.h
src/hw/<name>_driver.cpp
```

Guard with `#ifndef NATIVE`.

### 3. Wire into main.cpp

Add `#ifdef HAS_<NAME>` blocks in the following locations:

```c++
// Top: includes
#ifdef HAS_<NAME>
#include "modules/<name>_module.h"
#endif

// Inside #ifndef NATIVE: hardware includes + static instances
#ifdef HAS_<NAME>
#include "hw/<name>_driver.h"
static <DriverClass> my_driver;
#endif

// register_modules(): register the module
#ifdef HAS_<NAME>
    <name>_module_register(registry);
#endif

// publish_sensor_data(): contribute to payload
#ifdef HAS_<NAME>
    auto reading = my_driver.read();
    <name>_module_contribute(pb, reading);
#endif

// setup(): init hardware
#ifdef HAS_<NAME>
    my_driver.begin();
#endif
```

### 4. Add the flag to platformio.ini

```ini
[env:my_device]
extends = common_esp8266
build_flags =
    ${common.build_flags}
    -DDEVICE_ID='"my_device"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_BME280
    -DHAS_<NAME>
```

### 5. Add tests

Create `test/test_native/test_<name>_module.cpp` with tests for:

- `<name>_module_register` adds expected metrics/commands
- `<name>_module_contribute` produces expected JSON fields
- Command handler (if any) parses payloads correctly

Add declarations and `RUN_TEST` calls to `test/test_native/test_main.cpp`.

## File reference

| File                                             | Layer  | Purpose                                             |
|--------------------------------------------------|--------|-----------------------------------------------------|
| `platformio.ini`                                 | Config | Environments, feature flags, device identity        |
| `include/config.h`                               | Config | MQTT topics, pins, timing (identity via -D flags)   |
| `include/credentials.h`                          | Config | WiFi & MQTT credentials (gitignored)                |
| `src/main.cpp`                                   | Glue   | Core orchestrator with `#ifdef` composition         |
| `src/hw/esp_network.cpp`                         | HW     | ESP8266 WiFi + MQTT via PubSubClient                |
| `src/hw/nina_network.cpp`                        | HW     | MKR WiFiNINA + MQTT via PubSubClient                |
| `src/hw/bme280_sensor.cpp`                       | HW     | BME/BMP280 I2C driver (shared, Bosch formulas)      |
| `src/hw/sht30_sensor.cpp`                        | HW     | SHT30 I2C driver (Sensirion, CRC-8 verified)        |
| `src/hw/shift_display.cpp`                       | HW     | 7-segment via shift registers (ESP8266 only)        |
| `src/hw/battery_adc.cpp`                         | HW     | ESP8266 ADC reading (10-bit, A0)                    |
| `src/hw/samd_battery_adc.cpp`                    | HW     | MKR ADC reading (12-bit, ADC_BATTERY)               |
| `src/hw/esp_sleep.cpp`                           | HW     | Deep sleep + RTC memory (ESP8266 only)              |
| `lib/thermo_core/src/module_registry.cpp`        | Core   | Module registration and command dispatch            |
| `lib/thermo_core/src/payload_builder.cpp`        | Core   | Incremental JSON builder                            |
| `lib/thermo_core/src/mqtt_payload.cpp`           | Core   | Capabilities format, command parsing, status format |
| `lib/thermo_core/src/modules/bme280_module.cpp`  | Module | BME280 metrics registration and contribution        |
| `lib/thermo_core/src/modules/sht30_module.cpp`   | Module | SHT30 metrics registration and contribution         |
| `lib/thermo_core/src/modules/battery_module.cpp` | Module | Battery metrics registration and contribution       |
| `lib/thermo_core/src/battery.cpp`                | Core   | ADC-to-voltage, voltage-to-SoC math                 |
| `lib/thermo_core/src/display_encoding.cpp`       | Core   | BCD encoding for 7-segment display                  |
