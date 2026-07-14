# SHT30 Temperature & Humidity Module

## Overview

The SHT30 module provides **temperature** (°C) and **humidity** (% RH) readings via a Wemos SHT30 Shield v2.1.0
stacked directly on a Wemos D1 Mini (ESP8266). Communication uses I2C.

This module is **mutually exclusive** with `HAS_BME280` — only one temperature/humidity source can be active at a time.

| Metric | Unit | Accuracy |
|--------|------|----------|
| `temp` | °C   | ±0.2°C   |
| `humi` | % RH | ±2%      |

**Datasheet**: [Sensirion SHT3x-DIS](../datasheets/sht30-dis-datasheet.pdf)

## Shield Specifications

| Parameter          | Value                           |
|--------------------|---------------------------------|
| Sensor IC          | Sensirion SHT30-DIS             |
| Shield version     | v2.1.0                          |
| Supply voltage     | 2.15–5.5 V                      |
| Powered from       | D1 Mini 3.3 V rail (via shield) |
| I2C address        | 0x44 (default) or 0x45          |
| Temperature range  | -40 to 125°C                    |
| Humidity range     | 0 – 100% RH                     |
| Idle current       | 0.2µA                           |
| Measuring current  | 600µA (typ.)                    |
| Response time (T)  | >2s                             |
| Response time (RH) | 8s                              |
| Decoupling         | 100 nF cap on shield PCB        |
| Form factor        | Wemos D1 Mini shield, stacking  |

## Wiring

The SHT30 Shield stacks directly onto the D1 Mini headers — no external wiring is needed.

![SHT30 Shield Wiring](../img/sht30-wiring.svg)

### Pin Usage

| D1 Mini Pin | GPIO  | Function | Notes                  |
|-------------|-------|----------|------------------------|
| D1          | GPIO5 | SCL      | I2C clock              |
| D2          | GPIO4 | SDA      | I2C data               |
| 3V3         | —     | VDD      | 3.3 V supply to shield |
| GND         | —     | GND      | Ground                 |

## I2C Address Configuration

The SHT30 Shield has a solder jumper pad labeled **ADDR** on the PCB:

| ADDR Pad | I2C Address | Configuration      |
|----------|-------------|--------------------|
| Open     | **0x44**    | Default            |
| Shorted  | **0x45**    | Bridge with solder |

## Schematic

![SHT30 Schematic](../img/sht30-schematic.svg)

### Key Circuit Notes

- **Decoupling capacitor**: 100 nF between VDD and VSS, already present on the shield PCB.
- **Pull-up resistors**: 10 kΩ on SDA and SCL to VDD (on shield PCB).
- **ADDR pin**: connected to GND for address 0x44, to VDD for 0x45.
- **nRESET pin**: pulled high to VDD (inactive).

## PlatformIO Configuration

### Build Flag

```ini
build_flags = -DHAS_SHT30
```

> **Note**: Do not combine `-DHAS_SHT30` and `-DHAS_BME280` in the same environment — they are mutually exclusive.

### Example Environment

```ini
[env:sensor_8266_sht30]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHW_CODE='"E8SHTBAT"'
    -DHW_REV=1
    -DHAS_SHT30
    -DHAS_BATTERY
    -DHAS_DEEP_SLEEP
    -DHAS_CALIBRATION
    -DHAS_OTA
```

Build and upload with the real env name:

```bash
pio run -e sensor_8266_sht30            # build
pio run -e sensor_8266_sht30 -t upload  # upload
```

> **Identity (Stage B):** this production image carries no compiled `device_id`. A single binary per
> `(HW_CODE, HW_REV)` serves every unit; on first boot assign identity over serial with
> `provision <id>` (see [OTA](ota.md#first-provisioning)). `-DDEVICE_ID` survives only on dev builds
> (`-DHAS_SERIAL_DEBUG`) as a store seed. Calibration is runtime too (config store + server mirror).

## Firmware Files

| File                                           | Role                                    |
|------------------------------------------------|-----------------------------------------|
| `lib/thermo_drivers/src/sht30_sensor.h`        | Hardware driver header                  |
| `lib/thermo_drivers/src/sht30_sensor.cpp`      | Hardware driver (I2C, CRC-8 verified)   |
| `lib/thermo_core/src/modules/sht30_module.h`   | Module interface (register, contribute) |
| `lib/thermo_core/src/modules/sht30_module.cpp` | Module logic (metrics registration)     |

### Module API

```c++
void sht30_module_register(ModuleRegistry& reg);
void sht30_module_contribute(PayloadBuilder& pb, const SensorData& data);
```

## MQTT Payload

When active with battery monitoring, the device publishes on `thermo/{device_id}/sensors`:

```json
{"temp":22.5,"humi":48.3,"bat":85,"batv":7.92,"rssi":-42}
```

No `press` field — the SHT30 does not measure pressure.

## Autonomy Estimates (with 2S 18650 + deep sleep)

The SHT30 draws negligibly compared to the ESP8266 radio, so its autonomy matches the generic 2S
ESP8266 node. On **HW rev 1** (MC78M05BTG 5V LDO + always-on voltage divider) that is **~21 days
theoretical**. No SHT30 unit is deployed in the field yet, so there is no measured figure.

See [power-2s](power-2s.md) for the full current budget and the HW rev 2 target — the numbers are not
duplicated here. This config uses [battery](battery.md) and [deep-sleep](deep-sleep.md).

## Troubleshooting

| Symptom           | Cause                    | Fix                                       |
|-------------------|--------------------------|-------------------------------------------|
| SHT30 init failed | Wrong I2C address        | Check solder jumper on shield (0x44/0x45) |
| Humidity reads 0% | CRC check failing        | Check I2C wiring / try soft reset         |
| Self-heating      | Measuring too frequently | Increase publish interval (≥10s)          |

## See Also

- [Calibration](calibration.md) — per-device offset correction (recommended for cross-sensor consistency)
- [BME280](bme280.md), [MKR ENV](mkr-env.md) — mutually exclusive alternative sensor modules
- [MQTT protocol](../mqtt-protocol.md) — metric names and payload format
- [Components inventory](../components.md) — SHT30 Shield hardware specs
- [Architecture](../architecture.md) — module system and feature flags
- Configs using this module: [ESP+SHT30+Battery](../configs/esp-sht30-battery.md),
  [ESP+SHT30+Display](../configs/esp-display-sht30.md)
