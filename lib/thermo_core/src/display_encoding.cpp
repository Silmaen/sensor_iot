#include "display_encoding.h"

uint16_t encode_display(uint8_t dig0, uint8_t dig1, uint8_t dig2,
                        bool dp0, bool dp1, bool dp2) {
    uint16_t val = 0;
    val |= (dig0 & 0x0F);
    val |= (dig1 & 0x0F) << 4;
    val |= (dig2 & 0x0F) << 8;
    if (dp0) val |= (1 << 12);
    if (dp1) val |= (1 << 13);
    if (dp2) val |= (1 << 14);
    return val;
}

uint16_t encode_temperature(float temp_c) {
    // Clamp to displayable range -9.9 to 99.9
    if (temp_c > 99.9f) temp_c = 99.9f;
    if (temp_c < -9.9f) temp_c = -9.9f;

    // Display format: XX.X with DP on dig1 (between middle and right digit)
    int value;
    if (temp_c < 0) {
        // Negative: show as -X.X (dig0=blank/dash handled at display level)
        // For simplicity, we show absolute value — the display has no minus segment
        temp_c = -temp_c;
    }
    value = static_cast<int>(temp_c * 10 + 0.5f);
    if (value > 999) value = 999;

    uint8_t dig0 = (value / 100) % 10;
    uint8_t dig1 = (value / 10) % 10;
    uint8_t dig2 = value % 10;

    return encode_display(dig0, dig1, dig2, false, true, false);
}

uint16_t encode_humidity(float humidity) {
    if (humidity > 99.9f) humidity = 99.9f;
    if (humidity < 0.0f) humidity = 0.0f;

    int value = static_cast<int>(humidity * 10 + 0.5f);
    if (value > 999) value = 999;

    uint8_t dig0 = (value / 100) % 10;
    uint8_t dig1 = (value / 10) % 10;
    uint8_t dig2 = value % 10;

    return encode_display(dig0, dig1, dig2, false, true, false);
}

uint16_t encode_pressure(float pressure_hpa) {
    // Show last 3 integer digits (e.g. 1013 -> 013, 987 -> 987)
    int value = static_cast<int>(pressure_hpa + 0.5f);
    value = value % 1000;
    if (value < 0) value = 0;

    uint8_t dig0 = (value / 100) % 10;
    uint8_t dig1 = (value / 10) % 10;
    uint8_t dig2 = value % 10;

    return encode_display(dig0, dig1, dig2, false, false, false);
}
