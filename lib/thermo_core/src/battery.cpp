#include "battery.h"

float adc_to_voltage(uint16_t adc_value) {
    float adc_voltage = (static_cast<float>(adc_value) / ADC_MAX_VALUE) * ADC_REF_VOLTAGE;
    return adc_voltage * BATTERY_DIVIDER_RATIO;
}

uint8_t voltage_to_soc(float voltage) {
    if (voltage >= BATTERY_VOLTAGE_FULL) return 100;
    if (voltage <= BATTERY_VOLTAGE_EMPTY) return 0;
    float range = BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY;
    float pct = (voltage - BATTERY_VOLTAGE_EMPTY) / range * 100.0f;
    return static_cast<uint8_t>(pct);
}
