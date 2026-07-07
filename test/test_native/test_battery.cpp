#include <unity.h>
#include "battery.h"

void test_adc_to_voltage_zero(void) {
    float v = adc_to_voltage(0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
}

void test_adc_to_voltage_max(void) {
    // ADC 1023 => 3.3V at pin => 3.3 * BATTERY_DIVIDER_RATIO
    float expected = 3.3f * BATTERY_DIVIDER_RATIO;
    float v = adc_to_voltage(1023);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, expected, v);
}

void test_adc_to_voltage_midpoint(void) {
    // ADC 512 => ~1.65V at pin => ~1.65 * BATTERY_DIVIDER_RATIO
    float expected = (512.0f / 1023.0f) * 3.3f * BATTERY_DIVIDER_RATIO;
    float v = adc_to_voltage(512);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, expected, v);
}

void test_voltage_to_soc_full(void) {
    TEST_ASSERT_EQUAL_UINT8(100, voltage_to_soc(8.40f));
}

void test_voltage_to_soc_above_full(void) {
    TEST_ASSERT_EQUAL_UINT8(100, voltage_to_soc(9.0f));
}

void test_voltage_to_soc_empty(void) {
    TEST_ASSERT_EQUAL_UINT8(0, voltage_to_soc(6.0f));
}

void test_voltage_to_soc_below_empty(void) {
    TEST_ASSERT_EQUAL_UINT8(0, voltage_to_soc(5.0f));
}

void test_voltage_to_soc_midpoint(void) {
    // Non-linear curve: 7.2V (3.60V/cell) sits on the discharge cliff, ~24%,
    // far below the 50% a linear model would report. This is the whole point.
    TEST_ASSERT_UINT8_WITHIN(3, 24, voltage_to_soc(7.2f));
}

void test_voltage_to_soc_quarter(void) {
    // 6.6V (3.30V/cell) is deep in the end-of-life cliff, ~11%.
    TEST_ASSERT_UINT8_WITHIN(3, 11, voltage_to_soc(6.6f));
}

void test_voltage_to_soc_monotonic(void) {
    // SoC must strictly increase with voltage across the range.
    uint8_t prev = 0;
    for (float v = 6.1f; v <= 8.3f; v += 0.1f) {
        uint8_t s = voltage_to_soc(v);
        TEST_ASSERT_TRUE(s >= prev);
        prev = s;
    }
}

void test_voltage_to_soc_below_linear_on_plateau(void) {
    // A linear map (6.0-8.4V) would put 7.5V at ~62%; the real curve is lower
    // because most capacity lives on the high-voltage plateau.
    TEST_ASSERT_TRUE(voltage_to_soc(7.5f) < 55);
}

void test_voltage_to_soc_reaches_full_at_ceiling(void) {
    // The realistic ceiling must read 100% (the old 8.40V full made it
    // unreachable). Native default full is 8.40V; 8.30V should be ~100%.
    TEST_ASSERT_TRUE(voltage_to_soc(8.30f) >= 98);
}

void test_battery_calibrate_set_ratio(void) {
    // Save original ratio, set a new one, verify conversion changes, restore
    float original_v = adc_to_voltage(512);
    battery_calibrate_set_ratio(3.0f);
    float new_v = adc_to_voltage(512);
    TEST_ASSERT(new_v > original_v); // higher ratio = higher voltage
    // Restore default
    battery_calibrate_set_ratio(BATTERY_DIVIDER_RATIO);
    float restored_v = adc_to_voltage(512);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, original_v, restored_v);
}

void test_battery_calibrate(void) {
    // Simulate: ADC reads 500, real voltage is 8.0V
    // pin_voltage = (500/1023)*3.3 = 1.6129V
    // expected ratio = 8.0 / 1.6129 = 4.96
    float ratio = battery_calibrate(8.0f, 500);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 4.96f, ratio);
    // Verify adc_to_voltage now uses new ratio
    float v = adc_to_voltage(500);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8.0f, v);
    // Restore default
    battery_calibrate_set_ratio(BATTERY_DIVIDER_RATIO);
}

void test_battery_calibrate_zero_adc(void) {
    // ADC 0 should not change the ratio
    float ratio_before = adc_to_voltage(512);
    battery_calibrate(8.0f, 0);
    float ratio_after = adc_to_voltage(512);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, ratio_before, ratio_after);
}
