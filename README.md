# Thermo

IoT thermostat built with a Wemos D1 Mini v3.0.0 (ESP8266).

## Overview

Thermo is a connected thermostat project using PlatformIO. It targets the Wemos D1 Mini v3.0.0 and includes a native build environment for local debugging and unit testing.

## Hardware

- **MCU**: Wemos D1 Mini v3.0.0 (ESP8266)
- See [COMPONENTS.md](COMPONENTS.md) for the full list of available electronic components.

## Getting Started

### Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html) or PlatformIO IDE

### Build

```bash
pio run -e esp8266          # Build for Wemos D1 Mini
pio run -e native           # Build native (local debug)
```

### Upload

```bash
pio run -e esp8266 -t upload
```

### Monitor

```bash
pio device monitor          # Serial monitor (115200 baud)
```

### Test

```bash
pio test -e native          # Run unit tests
```

## Project Structure

```
src/         Source code
include/     Shared headers
lib/         Project-specific libraries
test/        Tests (test_native/ for local tests)
```

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
