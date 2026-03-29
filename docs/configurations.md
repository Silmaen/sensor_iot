# Hardware Configurations

Overview of all available device assemblies. Each configuration combines reusable
[modules](modules/) into a specific hardware build.

## Configurations

| Config                                                     | PlatformIO env         | MCU           | Sensor              | Power    | Use case                     |
|------------------------------------------------------------|------------------------|---------------|---------------------|----------|------------------------------|
| [Cell Tester](configs/cell-tester.md)                      | `cell_tester`          | MKR WiFi 1010 | --                  | USB 5V   | 18650 charge/discharge test  |
| [MKR + ENV + Battery](configs/mkr-env-battery.md)          | `thermo_mkr`           | MKR WiFi 1010 | BME280 (ENV Shield) | 1S 18650 | Indoor monitoring, compact   |
| [ESP + BMP280 + 2S Battery](configs/esp-bmp280-battery.md) | `thermo_1`             | D1 Mini v3    | BME/BMP280 breakout | 2S 18650 | Long-autonomy outdoor sensor |
| [ESP + SHT30 + 2S Battery](configs/esp-sht30-battery.md)   | `thermo_sht30`         | D1 Mini v3    | SHT30 Shield        | 2S 18650 | Compact outdoor, no pressure |
| [ESP + SHT30 + Display](configs/esp-display-sht30.md)      | `thermo_display_sht30` | D1 Mini v3    | SHT30 Shield        | USB 5V   | Indoor display station       |

## Module matrix

Shows which `HAS_xxx` flags each configuration uses.

| Config                | `HAS_BME280` | `HAS_SHT30` | `HAS_BATTERY` | `HAS_DEEP_SLEEP` | `HAS_DISPLAY` | `HAS_SERIAL_DEBUG` |
|-----------------------|:------------:|:-----------:|:-------------:|:----------------:|:-------------:|:------------------:|
| Cell Tester           |      --      |     --      |      --       |        --        |      --       |         --         |
| MKR + ENV + Battery   |      x       |     --      |       x       |        --        |      --       |         x          |
| ESP + BMP280 + 2S     |      x       |     --      |       x       |        x         |      --       |         --         |
| ESP + SHT30 + 2S      |      --      |      x      |       x       |        x         |      --       |         --         |
| ESP + SHT30 + Display |      --      |      x      |      --       |        --        |       x       |         x          |

**Notes:**

- Cell Tester is standalone firmware (`cell_tester.cpp`), no `HAS_xxx` flags.
- `HAS_BME280` and `HAS_SHT30` are mutually exclusive.
- `HAS_DEEP_SLEEP` requires `HAS_BATTERY` (battery monitoring needed for sleep decisions).
