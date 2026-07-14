# Architecture

This document describes the modular architecture of the thermo firmware. For the MQTT
protocol details, see [mqtt-protocol.md](mqtt-protocol.md). For available hardware, see
[components.md](components.md).

## Design goals

- **Multi-platform** — supports ESP8266 (D1 Mini), ESP32-C3 (Super Mini) and
  SAMD21 (MKR WiFi 1010) from a single codebase. Platform selection is automatic
  via compiler macros.
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
See [configurations](configurations.md) for which flags each device assembly uses.

| Flag               | What it enables                                                          | Module doc                            |
|--------------------|--------------------------------------------------------------------------|---------------------------------------|
| `HAS_BME280`       | BME/BMP280 I2C sensor initialization, reading, metrics                   | [bme280](modules/bme280.md)           |
| `HAS_SHT30`        | SHT30 I2C sensor initialization, reading, metrics (no pressure)          | [sht30](modules/sht30.md)             |
| `HAS_MKR_ENV`      | MKR ENV Shield sensors (HTS221 + LPS22HB + TEMT6000 + VEML6075)          | [mkr-env](modules/mkr-env.md)         |
| `HAS_BATTERY`      | Battery ADC reading, SoC calculation, low-battery alert                  | [battery](modules/battery.md)         |
| `HAS_LIGHT`        | Analog light sensor reading                                              | [light](modules/light.md)             |
| `HAS_BH1750`       | BH1750 I2C ambient-light sensor, `lux` metric                            | [bh1750](modules/bh1750.md)           |
| `HAS_CALIBRATION`  | Per-device sensor offsets (temperature, humidity, pressure)              | [calibration](modules/calibration.md) |
| `HAS_RELAY`        | Dual relay board control (toggle + timed contact)                        | [relay](modules/relay.md)             |
| `HAS_DISPLAY`      | 7-segment display, push button, display mode cycling                     | [display](modules/display.md)         |
| `HAS_DEEP_SLEEP`   | One-shot execution mode with deep sleep (ESP8266 + ESP32-C3, timer wake) | [deep-sleep](modules/deep-sleep.md)   |
| `HAS_OTA`          | Over-the-air firmware updates (ESP8266 / ESP32-C3 only)                  | [ota](modules/ota.md)                 |
| `HAS_SERIAL_DEBUG` | Verbose serial logging, shorter dev publish interval                     | —                                     |

The `NATIVE` flag is set automatically for the native test build. Code
inside `#ifndef NATIVE` is excluded from tests (hardware drivers, Arduino
calls, etc.).

### Platform selection macros

Platform-specific code is selected at compile time using compiler-defined macros:

| Macro                      | Set by                                | Used for                                 |
|----------------------------|---------------------------------------|------------------------------------------|
| `ESP8266`                  | ESP8266 Arduino core                  | ESP8266-specific drivers, pins, chip ID  |
| `ESP32`                    | Arduino-ESP32 core                    | ESP32-C3-specific drivers, pins, chip ID |
| `ARDUINO_SAMD_MKRWIFI1010` | SAMD Arduino core (board=mkrwifi1010) | MKR-specific drivers, pins, chip ID      |
| `NATIVE`                   | `-DNATIVE` build flag                 | Exclude all hardware code in tests       |

In `main.cpp` and `config.h`, platform selection follows this pattern:

```c++
#if defined(ESP8266)
    // ESP8266 (D1 Mini) specific
#elif defined(ESP32)
    // ESP32-C3 (Super Mini) specific
#elif defined(ARDUINO_SAMD_MKRWIFI1010)
    // MKR WiFi 1010 specific
#endif
```

### Device identity

Since Stage B (see [ota-calibration-protocol.md](ota-calibration-protocol.md)),
`device_id` is **no longer a build flag**. A single firmware image per
`(HW_CODE, HW_REV)` couple serves every unit; each unit is given its `device_id`
once over serial (`provision <id>`) and stores it in the on-device config store
(LittleFS on ESP8266, NVS on ESP32-C3, FlashStorage on SAMD). A `-DDEVICE_ID`
flag survives only as a store *seed* on dev builds (`HAS_SERIAL_DEBUG`) and on the
`native` test env — never as the normal way to set identity.

The identity flags that remain build-time constants:

| Flag               | Example                         | Purpose                                                        |
|--------------------|---------------------------------|----------------------------------------------------------------|
| `MQTT_DEVICE_TYPE` | `-DMQTT_DEVICE_TYPE='"thermo"'` | Device category (first topic segment), one per firmware type   |
| `HW_CODE`          | `-DHW_CODE='"E8BMEBAT"'`        | 8-char hardware code `^[A-Z0-9]{8}$`; one OTA image per code   |
| `HW_REV`           | `-DHW_REV=1`                    | Integer hardware revision; gates `#if HW_REV >= n`, pairs `hw` |

Note the quoting on string flags: single quotes wrap double quotes so the
preprocessor receives a string literal (`'"E8BMEBAT"'` → `"E8BMEBAT"`).

Topics are built **at runtime** by `DeviceTopics::build(device_type, device_id)`
(see [`lib/thermo_core/src/device_topics.h`](../lib/thermo_core/src/device_topics.h)),
not by macros in `config.h`. For a device provisioned as `thermo_1` the set is:

```text
thermo/thermo_1/sensors
thermo/thermo_1/status
thermo/thermo_1/command
thermo/thermo_1/capabilities
thermo/thermo_1/commands
thermo/thermo_1/calibration
thermo/thermo_1/ack
```

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
to the registry. Metrics appear in the `capabilities` message; commands appear
in the `commands` message.

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
pb.add_float("temp", 22.5, 1);          // writes "temp":22.5
pb.add_uint("bat", 75);                 // writes ,"bat":75
pb.end();                                // writes }
// buf = {"temp":22.5,"bat":75}
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

The `calibrate` command will appear in the `commands` message and
the device will handle `{"action":"calibrate","value":...}` from the server.

### Capabilities and commands messages

Both are built dynamically at runtime from `ModuleRegistry`, but they are now
**two separate messages** (each must fit `MQTT_MAX_PACKET_SIZE`, 512 B):

- `capabilities` — identity + metrics, published in response to
  `request_capabilities`.
- `commands` — the actionable command list, published in response to
  `request_commands`.

The `capabilities` payload is produced by `format_capabilities_payload`:

```c++
format_capabilities_payload(
    hw_id,                // per-chip serial (e.g. "ESP-00A1B2")
    hw_code,              // fixed 8-char HW_CODE
    hw_rev,               // HW_REV
    fw_version,           // FIRMWARE_VERSION (semver)
    ota_capable,          // -> "ota":1
    calibration_present,  // -> "cal":1
    publish_interval_s,   // current interval -> "intrvl"
    registry,             // metrics come from all registered modules
    buf, sizeof(buf)
);
```

Metric names are short tokens (`temp`, `humi`, `press`, `bat`, `batv`, `lux`,
`light`, `relay1`, `relay2`, `uv`, `rssi`), each mapped to its unit string.
Example output with `HAS_BME280` + `HAS_BATTERY` + `HAS_OTA` (the core always
registers `rssi`):

```json
{
  "id": "ESP-00A1B2",
  "hw": "E8BMEBAT",
  "hwrev": 1,
  "fw": "1.0.0",
  "ota": 1,
  "cal": 1,
  "intrvl": 300,
  "metrics": {"rssi": "dBm", "temp": "°C", "humi": "%", "press": "hPa", "bat": "%", "batv": "V"}
}
```

The command list lives in the separate `commands` message
(`format_commands_payload`). The core registers `set_interval` and handles the
implicit `request_capabilities` / `request_commands` actions; modules add their
own (e.g. `set_offset`, `set_calibration`, `ota_update`). See
[mqtt-protocol.md](mqtt-protocol.md) §5 and §5b for both payload shapes.

## Data flow: sensor publish cycle

![Sensor publish data flow](img/architecture-data-flow.svg)

### Calibration step

When `HAS_CALIBRATION` is enabled, `calibration_apply(data)` is called between the sensor
read and the module contribute step. This adjusts temperature, humidity, and pressure values
in-place before they are serialized into the JSON payload. See [calibration module](modules/calibration.md).

```c++
SensorData data = sensor.read();
#ifdef HAS_CALIBRATION
calibration_apply(data);  // offsets applied in-place
#endif
bme280_module_contribute(pb, data);
```

Calibration (sensor offsets and the battery `bat_divider`) is **runtime only** — it lives
in the on-device config store and is mirrored on the server. It is set via the `set_offset`
and `set_calibration` MQTT commands; there are no build-time calibration flags.

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

If the module needs new hardware, add a cross-platform driver in
`lib/thermo_drivers/src/` (or, for platform-specific code, under
`lib/thermo_platform/src/<platform>/`):

```c++
lib/thermo_drivers/src/<name>_sensor.h
lib/thermo_drivers/src/<name>_sensor.cpp
```

Guard hardware calls with `#ifndef NATIVE`.

### 3. Wire into main.cpp

Add `#ifdef HAS_<NAME>` blocks in the following locations:

```c++
// Top: includes
#ifdef HAS_<NAME>
#include "modules/<name>_module.h"
#endif

// Inside #ifndef NATIVE: hardware includes + static instances
#ifdef HAS_<NAME>
#include "<name>_sensor.h"
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
    ${common_esp8266.build_flags}
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHW_CODE='"XXXXXXXX"'
    -DHW_REV=1
    -DHAS_BME280
    -DHAS_<NAME>
```

Production envs carry no `-DDEVICE_ID`; the unit is provisioned over serial after
flashing (see [Device identity](#device-identity)).

### 5. Add tests

Create `test/test_native/test_<name>_module.cpp` with tests for:

- `<name>_module_register` adds expected metrics/commands
- `<name>_module_contribute` produces expected JSON fields
- Command handler (if any) parses payloads correctly

Add declarations and `RUN_TEST` calls to `test/test_native/test_main.cpp`.

## File reference

| File                                                      | Layer    | Purpose                                                           |
|-----------------------------------------------------------|----------|-------------------------------------------------------------------|
| `platformio.ini`                                          | Config   | Environments, feature flags, build-time identity                  |
| `lib/thermo_core/src/config.h`                            | Config   | Pins, ADC, timing, MQTT settings (`HW_CODE`/`HW_REV`/type via -D) |
| `lib/thermo_core/src/debug.h`                             | Config   | Serial debug macros (`HAS_SERIAL_DEBUG`)                          |
| `include/credentials.h`                                   | Config   | WiFi & MQTT credentials (gitignored)                              |
| `src/main.cpp`                                            | Glue     | Core orchestrator with `#ifdef` composition                       |
| `src/cell_tester.cpp`                                     | Glue     | Standalone 18650 cell-tester firmware (MKR)                       |
| `lib/thermo_core/src/module_registry.cpp`                 | Core     | Module registration and command dispatch                          |
| `lib/thermo_core/src/payload_builder.cpp`                 | Core     | Incremental JSON builder                                          |
| `lib/thermo_core/src/mqtt_payload.cpp`                    | Core     | Capabilities/commands/status/ack format, command parsing          |
| `lib/thermo_core/src/device_topics.cpp`                   | Core     | Runtime MQTT topic build from type + provisioned id               |
| `lib/thermo_core/src/battery.cpp`                         | Core     | ADC-to-voltage, voltage-to-SoC math                               |
| `lib/thermo_core/src/display_encoding.cpp`                | Core     | BCD encoding for 7-segment display                                |
| `lib/thermo_core/src/modules/bme280_module.cpp`           | Module   | BME280 metrics registration and contribution                      |
| `lib/thermo_core/src/modules/sht30_module.cpp`            | Module   | SHT30 metrics registration and contribution                       |
| `lib/thermo_core/src/modules/bh1750_module.cpp`           | Module   | BH1750 `lux` metric registration and contribution                 |
| `lib/thermo_core/src/modules/battery_module.cpp`          | Module   | Battery metrics registration and contribution                     |
| `lib/thermo_core/src/modules/mkr_env_module.cpp`          | Module   | MKR ENV Shield metrics registration and contribution              |
| `lib/thermo_core/src/modules/light_module.cpp`            | Module   | Analog light metric registration and contribution                 |
| `lib/thermo_core/src/modules/calibration_module.cpp`      | Module   | `set_offset` / `set_calibration` command handlers                 |
| `lib/thermo_core/src/modules/relay_module.cpp`            | Module   | Relay control, toggle/contact commands                            |
| `lib/thermo_core/src/modules/ota_module.cpp`              | Module   | `ota_update` command + OTA capability flag                        |
| `lib/thermo_drivers/src/bme280_sensor.cpp`                | Driver   | BME/BMP280 I2C driver (Bosch formulas)                            |
| `lib/thermo_drivers/src/sht30_sensor.cpp`                 | Driver   | SHT30 I2C driver (Sensirion, CRC-8 verified)                      |
| `lib/thermo_drivers/src/bh1750_sensor.cpp`                | Driver   | BH1750 I2C lux sensor driver                                      |
| `lib/thermo_drivers/src/mkr_env_sensor.cpp`               | Driver   | MKR ENV Shield driver                                             |
| `lib/thermo_drivers/src/shift_display.cpp`                | Driver   | 7-segment display via shift registers                             |
| `lib/thermo_platform/src/battery_adc.cpp`                 | Platform | Unified battery ADC read (per-platform `#if`)                     |
| `lib/thermo_platform/src/hw_id.cpp`                       | Platform | Per-chip hardware serial ID                                       |
| `lib/thermo_platform/src/esp8266/esp_network.cpp`         | Platform | ESP8266 WiFi + MQTT via PubSubClient                              |
| `lib/thermo_platform/src/esp8266/esp_ota.cpp`             | Platform | ESP8266 OTA update                                                |
| `lib/thermo_platform/src/esp8266/esp_sleep.cpp`           | Platform | ESP8266 deep sleep + RTC memory                                   |
| `lib/thermo_platform/src/esp8266/littlefs_config_store.h` | Platform | ESP8266 config store (LittleFS)                                   |
| `lib/thermo_platform/src/esp32/esp32_network.cpp`         | Platform | ESP32-C3 WiFi + MQTT via PubSubClient                             |
| `lib/thermo_platform/src/esp32/esp32_ota.cpp`             | Platform | ESP32-C3 OTA update                                               |
| `lib/thermo_platform/src/esp32/esp32_sleep.cpp`           | Platform | ESP32-C3 deep sleep (`RTC_DATA_ATTR`)                             |
| `lib/thermo_platform/src/esp32/nvs_config_store.h`        | Platform | ESP32-C3 config store (NVS)                                       |
| `lib/thermo_platform/src/samd/nina_network.cpp`           | Platform | MKR WiFiNINA + MQTT via PubSubClient                              |
| `lib/thermo_platform/src/samd/samd_sleep.cpp`             | Platform | SAMD standby (RTCZero)                                            |
| `lib/thermo_platform/src/samd/flash_config_store.cpp`     | Platform | SAMD config store (FlashStorage)                                  |
