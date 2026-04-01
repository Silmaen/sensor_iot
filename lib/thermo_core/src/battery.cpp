#include "battery.h"

static float divider_ratio = BATTERY_DIVIDER_RATIO;

float adc_to_voltage(uint16_t adc_value) {
    float adc_voltage = (static_cast<float>(adc_value) / ADC_MAX_VALUE) * ADC_REF_VOLTAGE;
    return adc_voltage * divider_ratio;
}

uint8_t voltage_to_soc(float voltage) {
    if (voltage >= BATTERY_VOLTAGE_FULL) return 100;
    if (voltage <= BATTERY_VOLTAGE_EMPTY) return 0;
    float range = BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY;
    float pct = (voltage - BATTERY_VOLTAGE_EMPTY) / range * 100.0f;
    return static_cast<uint8_t>(pct);
}

float battery_calibrate(float measured_voltage, uint16_t adc_value) {
    if (adc_value == 0) return divider_ratio;
    float pin_voltage = (static_cast<float>(adc_value) / ADC_MAX_VALUE) * ADC_REF_VOLTAGE;
    divider_ratio = measured_voltage / pin_voltage;
    return divider_ratio;
}

void battery_calibrate_set_ratio(float ratio) {
    if (ratio > 0.0f)
        divider_ratio = ratio;
}
