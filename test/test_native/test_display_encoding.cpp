#include <unity.h>
#include "display_encoding.h"

void test_encode_display_zeros(void) {
    uint16_t val = encode_display(0, 0, 0, false, false, false);
    TEST_ASSERT_EQUAL_HEX16(0x0000, val);
}

void test_encode_display_digits_only(void) {
    // dig0=2, dig1=3, dig2=5 => 0x0532
    uint16_t val = encode_display(2, 3, 5, false, false, false);
    TEST_ASSERT_EQUAL_HEX16(0x0532, val);
}

void test_encode_display_with_dp1(void) {
    // "23.5": dig0=2, dig1=3, dig2=5, DP1=ON
    uint16_t val = encode_display(2, 3, 5, false, true, false);
    TEST_ASSERT_EQUAL_HEX16(0x2532, val);
}

void test_encode_display_all_dp(void) {
    uint16_t val = encode_display(0, 0, 0, true, true, true);
    TEST_ASSERT_EQUAL_HEX16(0x7000, val);
}

void test_encode_display_example_from_spec(void) {
    // COMPONENTS.md: "23.5" => 0x2532
    uint16_t val = encode_display(2, 3, 5, false, true, false);
    TEST_ASSERT_EQUAL_HEX16(0x2532, val);
}

void test_encode_temperature_23_5(void) {
    uint16_t val = encode_temperature(23.5f);
    // dig0=2, dig1=3, dig2=5, DP1=ON
    TEST_ASSERT_EQUAL_HEX16(0x2532, val);
}

void test_encode_temperature_0(void) {
    uint16_t val = encode_temperature(0.0f);
    // 0.0 => dig0=0, dig1=0, dig2=0, DP1=ON
    TEST_ASSERT_EQUAL_HEX16(0x2000, val);
}

void test_encode_temperature_99_9(void) {
    uint16_t val = encode_temperature(99.9f);
    // dig0=9, dig1=9, dig2=9, DP1=ON
    TEST_ASSERT_EQUAL_HEX16(0x2999, val);
}

void test_encode_temperature_clamped_high(void) {
    uint16_t val = encode_temperature(150.0f);
    TEST_ASSERT_EQUAL_HEX16(0x2999, val);
}

void test_encode_humidity_65_2(void) {
    uint16_t val = encode_humidity(65.2f);
    // dig0=6, dig1=5, dig2=2, DP1=ON
    TEST_ASSERT_EQUAL_HEX16(0x2256, val);
}

void test_encode_pressure_1013(void) {
    uint16_t val = encode_pressure(1013.2f);
    // 1013 % 1000 = 13 => dig0=0, dig1=1, dig2=3, no DP
    TEST_ASSERT_EQUAL_HEX16(0x0310, val);
}

void test_encode_pressure_987(void) {
    uint16_t val = encode_pressure(987.0f);
    // 987 % 1000 = 987 => dig0=9, dig1=8, dig2=7
    TEST_ASSERT_EQUAL_HEX16(0x0789, val);
}
