# SHT30 Temperature & Humidity Module

## Overview

The SHT30 module provides **temperature** (°C) and **humidity** (% RH) readings via a Wemos SHT30 Shield v2.1.0
stacked directly on a Wemos D1 Mini (ESP8266). Communication uses I2C.

This module is **mutually exclusive** with `HAS_BME280` — only one temperature/humidity source can be active at a time.

| Metric        | Unit | Accuracy |
|---------------|------|----------|
| `temperature` | °C   | ±0.2°C   |
| `humidity`    | % RH | ±2%      |

**Datasheet**: [Sensirion SHT3x-DIS](../datasheets/sht30-dis-datasheet.pdf)

## Shield Specifications

| Parameter            | Value                           |
|----------------------|---------------------------------|
| Sensor IC            | Sensirion SHT30-DIS             |
| Shield version       | v2.1.0                          |
| Supply voltage       | 2.15–5.5 V                      |
| Powered from         | D1 Mini 3.3 V rail (via shield) |
| I2C address          | 0x44 (default) or 0x45          |
| Temperature range    | -40 to 125°C                    |
| Humidity range       | 0 – 100% RH                     |
| Idle current         | 0.2µA                           |
| Measuring current    | 600µA (typ.)                    |
| Response time (T)    | >2s                             |
| Response time (RH)   | 8s                              |
| Decoupling           | 100 nF cap on shield PCB        |
| Form factor          | Wemos D1 Mini shield, stacking  |

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
[env:thermo_sht30]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DDEVICE_ID='"thermo_sht30"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_SHT30
    -DHAS_BATTERY
    -DHAS_DEEP_SLEEP
```

## Firmware Files

| File                                           | Role                                    |
|------------------------------------------------|-----------------------------------------|
| `src/hw/sht30_sensor.h`                        | Hardware driver header                  |
| `src/hw/sht30_sensor.cpp`                      | Hardware driver (I2C, CRC-8 verified)   |
| `lib/thermo_core/src/modules/sht30_module.h`   | Module interface (register, contribute) |
| `lib/thermo_core/src/modules/sht30_module.cpp` | Module logic (metrics registration)     |

### Module API

```cpp
void sht30_module_register(ModuleRegistry& reg);
void sht30_module_contribute(PayloadBuilder& pb, const SensorData& data);
```

## MQTT Payload

When active with battery monitoring, the device publishes on `thermo/{device_id}/sensors`:

```json
{"temperature":22.5,"humidity":48.3,"battery_pct":85,"battery_v":7.92,"wifi_rssi":-42}
```

No `pressure` field — the SHT30 does not measure pressure.

## Autonomy Estimates (with 2S 18650 + deep sleep)

Combined with [battery](battery.md), [power-2s](power-2s.md), and [deep-sleep](deep-sleep.md) modules:

| Subsystem       | Active  | Deep sleep |
|-----------------|---------|------------|
| ESP8266         | 80mA    | 20µA       |
| SHT30           | 0.6mA   | 0.2µA      |
| Voltage divider | 247µA   | 247µA      |
| MP1584 quiesc.  | ~0.5mA  | ~0.5mA     |
| **Total**       | ~82mA   | ~0.77mA    |

With 2× 3000mAh 18650 (usable ~4000mAh at 5V after buck efficiency):

| Interval | Avg current | Autonomy   |
|----------|-------------|------------|
| 5 min    | 1.4 mA      | 119 days   |
| 10 min   | 0.7 mA      | 238 days   |
| 30 min   | 0.25 mA     | ~1.8 years |

## Troubleshooting

| Symptom                   | Cause                         | Fix                                       |
|---------------------------|-------------------------------|-------------------------------------------|
| SHT30 init failed         | Wrong I2C address             | Check solder jumper on shield (0x44/0x45) |
| Humidity reads 0%         | CRC check failing             | Check I2C wiring / try soft reset         |
| Self-heating              | Measuring too frequently      | Increase publish interval (≥10s)          |
