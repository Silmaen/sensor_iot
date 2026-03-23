#include <unity.h>
#include "battery.h"

void test_adc_to_voltage_zero(void) {
    float v = adc_to_voltage(0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
}

void test_adc_to_voltage_max(void) {
    // ADC 1023 => 3.3V at pin => 3.3 * 2.5 = 8.25V battery
    float v = adc_to_voltage(1023);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8.25f, v);
}

void test_adc_to_voltage_midpoint(void) {
    // ADC 512 => ~1.65V at pin => ~4.125V battery
    float v = adc_to_voltage(512);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 4.13f, v);
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
    // 7.2V: (7.2-6.0)/(8.4-6.0) = 1.2/2.4 = 50%
    TEST_ASSERT_EQUAL_UINT8(50, voltage_to_soc(7.2f));
}

void test_voltage_to_soc_quarter(void) {
    // 6.6V: (6.6-6.0)/(8.4-6.0) = 0.6/2.4 = 25%
    TEST_ASSERT_EQUAL_UINT8(25, voltage_to_soc(6.6f));
}
