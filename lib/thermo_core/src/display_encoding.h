#pragma once

#include <stdint.h>

// Encode 3 BCD digits + decimal point bits into 16-bit shift register format.
// Bit layout: [15]=free [14]=DP2 [13]=DP1 [12]=DP0 [11..8]=dig2 [7..4]=dig1 [3..0]=dig0
uint16_t encode_display(uint8_t dig0, uint8_t dig1, uint8_t dig2,
                        bool dp0, bool dp1, bool dp2);

// Encode temperature (e.g. 23.5 -> "23.5", DP between dig1 and dig2)
uint16_t encode_temperature(float temp_c);

// Encode humidity (e.g. 65.2 -> "65.2", DP between dig1 and dig2)
uint16_t encode_humidity(float humidity);

// Encode pressure (e.g. 1013.2 -> "013" — last 3 digits, no DP)
uint16_t encode_pressure(float pressure_hpa);
