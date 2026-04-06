# Hardware Configurations

Overview of all available device assemblies. Each configuration combines reusable
[modules](modules/) into a specific hardware build. See [architecture](architecture.md) for how the
module system works and [components](components.md) for the available hardware inventory.

## Configurations

| Config                                                     | PlatformIO env            | MCU           | Sensor              | Power    | Use case                      |
|------------------------------------------------------------|---------------------------|---------------|---------------------|----------|-------------------------------|
| [Cell Tester](configs/cell-tester.md)                      | `cell_tester`             | MKR WiFi 1010 | --                  | USB 5V   | 18650 charge/discharge test   |
| [MKR + ENV + Battery](configs/mkr-env-battery.md)          | `thermo_mkr`              | MKR WiFi 1010 | BME280 (ENV Shield) | 1S 18650 | Indoor monitoring, compact    |
| [ESP + BMP280 + 2S Battery](configs/esp-bmp280-battery.md) | `thermo_1`                | D1 Mini v3    | BME/BMP280 breakout | 2S 18650 | Long-autonomy outdoor sensor  |
| [ESP + SHT30 + 2S Battery](configs/esp-sht30-battery.md)   | `thermo_sht30`            | D1 Mini v3    | SHT30 Shield        | 2S 18650 | Compact outdoor, no pressure  |
| [ESP + SHT30 + Display](configs/esp-display-sht30.md)      | `thermo_display_sht30`    | D1 Mini v3    | SHT30 Shield        | USB 5V   | Indoor display station        |
| [C3 + BME280 + BH1750](configs/esp32c3-bme280-bh1750.md)   | `sensor_c3_bme280_bh1750` | ESP32-C3      | BME280 + BH1750     | 2S 18650 | Multi-sensor, ultra-low power |

## Module matrix

Shows which `HAS_xxx` flags each configuration uses.

| Config                | [`BME280`](modules/bme280.md) | [`SHT30`](modules/sht30.md) | [`BH1750`](modules/bh1750.md) | [`BATTERY`](modules/battery.md) | [`DEEP_SLEEP`](modules/deep-sleep.md) | [`DISPLAY`](modules/display.md) | [`CALIB`](modules/calibration.md) |
|-----------------------|:-----------------------------:|:---------------------------:|:-----------------------------:|:-------------------------------:|:-------------------------------------:|:-------------------------------:|:---------------------------------:|
| Cell Tester           |              --               |             --              |              --               |               --                |                  --                   |               --                |                --                 |
| MKR + ENV + Battery   |               x               |             --              |              --               |                x                |                  --                   |               --                |                 x                 |
| ESP + BMP280 + 2S     |               x               |             --              |              --               |                x                |                   x                   |               --                |                 x                 |
| ESP + SHT30 + 2S      |              --               |              x              |              --               |                x                |                   x                   |               --                |                 x                 |
| ESP + SHT30 + Display |              --               |              x              |              --               |               --                |                  --                   |                x                |                 x                 |
| C3 + BME280 + BH1750  |               x               |             --              |               x               |                x                |                   x                   |               --                |                 x                 |

**Notes:**

- Cell Tester is standalone firmware (`cell_tester.cpp`), no `HAS_xxx` flags.
- `HAS_BME280` and `HAS_SHT30` are mutually exclusive.
- `HAS_DEEP_SLEEP` requires `HAS_BATTERY` (battery monitoring needed for sleep decisions).
