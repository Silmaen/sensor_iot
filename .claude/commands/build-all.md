Build all PlatformIO environments to verify the project compiles everywhere.

Run each build sequentially and report results:

1. `pio run -e sensor_8266_empty`
2. `pio run -e sensor_8266_bmp80`
3. `pio run -e sensor_8266_sht30`
4. `pio run -e sensor_8266_display_sht30`
5. `pio run -e sensor_mkr_env`
6. `pio run -e cell_tester_mkr`
7. `pio run -e native`
8. `pio test -e native`

At the end, provide a summary table with pass/fail status for each environment.
