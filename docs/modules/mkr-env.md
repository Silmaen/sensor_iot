# MKR ENV Shield Module

## Overview

The MKR ENV module provides environmental sensing via the Arduino MKR ENV Shield stacked on an Arduino MKR WiFi 1010.
The shield carries four sensors, all used by this module:

- **HTS221** (ST) -- temperature and humidity (I2C)
- **LPS22HB** (ST) -- barometric pressure (I2C)
- **TEMT6000** (Vishay) -- ambient light (analog)
- **VEML6075** (Vishay) -- UV index (I2C)

This module is **mutually exclusive** with `HAS_BME280` and `HAS_SHT30` -- only one temperature/humidity source can be
active at a time.

| Metric        | Unit | Sensor   | Accuracy    |
|---------------|------|----------|-------------|
| `temperature` | °C   | HTS221   | ±0.5°C      |
| `humidity`    | % RH | HTS221   | ±3.5%       |
| `pressure`    | hPa  | LPS22HB  | ±0.025 hPa  |
| `light_lux`   | lux  | TEMT6000 | ±10% (typ.) |
| `uv_index`    | --   | VEML6075 | ±1 UVI      |

## Shield Specifications

### HTS221 (Temperature + Humidity)

| Parameter            | Value                 |
|----------------------|-----------------------|
| Sensor IC            | ST HTS221             |
| I2C address          | 0x5F (fixed)          |
| Temperature range    | -40 to +120°C         |
| Temperature accuracy | ±0.5°C (15--40°C)     |
| Humidity range       | 0--100% RH            |
| Humidity accuracy    | ±3.5% RH (20--80% RH) |
| Supply voltage       | 1.7--3.6 V            |
| Current (active)     | 2µA @ 1 Hz ODR        |
| Resolution           | 0.004°C / 0.004% RH   |

### LPS22HB (Pressure)

| Parameter            | Value                    |
|----------------------|--------------------------|
| Sensor IC            | ST LPS22HB               |
| I2C address          | 0x5C (fixed on shield)   |
| Pressure range       | 260--1260 hPa            |
| Pressure accuracy    | ±0.025 hPa               |
| Supply voltage       | 1.7--3.6 V               |
| Current (active)     | 12µA @ 1 Hz ODR          |
| Resolution           | 1/4096 hPa               |

### TEMT6000 (Ambient Light)

| Parameter            | Value                        |
|----------------------|------------------------------|
| Sensor IC            | Vishay TEMT6000              |
| Interface            | Analog (pin A2)              |
| Spectral range       | 440--800 nm (peak 570 nm)    |
| Sensitivity          | ~10µA per lux                |
| Load resistor        | 10 kΩ (on shield PCB)        |
| Range                | 0--330 lux (with 12-bit ADC) |
| Response time        | 10µs (rise/fall)             |

### VEML6075 (UV Index)

| Parameter            | Value                             |
|----------------------|-----------------------------------|
| Sensor IC            | Vishay VEML6075                   |
| I2C address          | 0x10 (fixed)                      |
| Channels             | UVA (365 nm), UVB (330 nm)        |
| Integration time     | 100 ms (default)                  |
| Supply voltage       | 1.7--3.6 V                        |
| Current (active)     | 480µA (typ.)                      |
| UV index output      | Computed from compensated UVA/UVB |

## Wiring

The MKR ENV Shield stacks directly onto the MKR WiFi 1010 headers -- no external wiring is needed.

### Pin Usage

| MKR Pin       | Function            | Notes                     |
|---------------|---------------------|---------------------------|
| `SDA`         | I2C data            | HTS221, LPS22HB, VEML6075 |
| `SCL`         | I2C clock           | HTS221, LPS22HB, VEML6075 |
| `A2`          | TEMT6000 analog out | Ambient light             |
| VCC           | VDD                 | 3.3 V from regulator      |
| GND           | GND                 | Ground                    |

## I2C Bus

All I2C sensors share the default Wire bus at their fixed addresses:

| Device   | Address | Function        |
|----------|---------|-----------------|
| HTS221   | 0x5F    | Temp + humidity |
| LPS22HB  | 0x5C    | Pressure        |
| VEML6075 | 0x10    | UV (UVA/UVB)    |

## Calibration

**HTS221**: Factory calibration coefficients are stored in internal registers. The driver reads these at startup and
computes linear interpolation parameters for both temperature and humidity. No user calibration required.

**LPS22HB**: Factory-calibrated. Raw output is divided by 4096 to obtain hPa.

**TEMT6000**: Linear analog output. Conversion uses the sensor's typical sensitivity (10µA/lux) with the shield's 10 kΩ
load resistor: `lux = V_adc / 0.01`.

**VEML6075**: UV index is computed from compensated UVA/UVB channels using Vishay's application note coefficients. The
compensation subtracts visible and IR crosstalk from the raw UVA/UVB readings.

## PlatformIO Configuration

### Build Flag

```ini
build_flags = -DHAS_MKR_ENV
```

> **Note**: Do not combine `-DHAS_MKR_ENV` with `-DHAS_BME280` or `-DHAS_SHT30` -- they are mutually exclusive.

### Example Environment

```ini
[env:sensor_mkr_env]
extends = common_samd
board = mkrwifi1010
build_flags =
    ${common_samd.build_flags}
    -DDEVICE_ID='"thermo_mkr"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_MKR_ENV
    -DHAS_BATTERY
    -DHAS_SERIAL_DEBUG
```

## Firmware Files

| File                                             | Role                                    |
|--------------------------------------------------|-----------------------------------------|
| `src/hw/mkr_env_sensor.h`                        | Hardware driver header (all 4 sensors)  |
| `src/hw/mkr_env_sensor.cpp`                      | I2C/analog communication, calibration   |
| `lib/thermo_core/src/modules/mkr_env_module.h`   | Module interface (register, contribute) |
| `lib/thermo_core/src/modules/mkr_env_module.cpp` | Module logic (5 metrics registration)   |

### Module API

```cpp
void mkr_env_module_register(ModuleRegistry& reg);
void mkr_env_module_contribute(PayloadBuilder& pb, const SensorData& data);
```

## MQTT Payload

When active with battery monitoring, the device publishes on `thermo/{device_id}/sensors`:

```json
{"temperature":22.5,"humidity":48.3,"pressure":1013.2,"light_lux":150,"uv_index":3.45,"battery_pct":85,"battery_v":3.95,"wifi_rssi":-42}
```

## Troubleshooting

| Symptom                    | Cause                            | Fix                                              |
|----------------------------|----------------------------------|--------------------------------------------------|
| MKR ENV init failed        | Shield not seated properly       | Reseat the shield on MKR headers                 |
| HTS221 not found (0x5F)    | I2C bus issue                    | Check shield connection, try Wire scanner sketch |
| LPS22HB not found (0x5C)   | I2C bus issue                    | Check shield connection, try Wire scanner sketch |
| VEML6075 not found (0x10)  | I2C bus issue                    | Check shield connection, try Wire scanner sketch |
| Temperature offset         | HTS221 self-heating              | Increase publish interval (>=10s)                |
| Humidity reads 0% or 100%  | Calibration data corrupt         | Power cycle the shield                           |
| Light reads 0 indoors      | TEMT6000 range limited           | Sensor maxes at ~330 lux (bright indoor)         |
| UV index always 0          | Indoors / no UV source           | VEML6075 needs direct UV exposure                |
| Previously used HAS_BME280 | Wrong build flag for ENV Shield  | Change to `-DHAS_MKR_ENV` in platformio.ini      |
