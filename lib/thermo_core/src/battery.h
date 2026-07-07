#pragma once

#include <stdint.h>

// Platform-specific battery constants come from config.h (ESP8266, ESP32, SAMD).
// For native builds (tests), provide sensible defaults (2S LiPo, 10-bit ADC).
#ifndef ADC_MAX_VALUE
#define ADC_MAX_VALUE         1023
#endif
#ifndef ADC_REF_VOLTAGE
#define ADC_REF_VOLTAGE       3.3f
#endif
#ifndef BATTERY_VOLTAGE_FULL
#define BATTERY_VOLTAGE_FULL  8.40f
#endif
#ifndef BATTERY_VOLTAGE_EMPTY
#define BATTERY_VOLTAGE_EMPTY 6.00f
#endif
#ifndef BATTERY_DIVIDER_RATIO
#define BATTERY_DIVIDER_RATIO 2.833f
#endif
// Number of Li-ion/LiPo cells in series (2 for 2S packs, 1 for 1S).
// The state-of-charge curve is expressed per cell, so the pack voltage is
// divided by this before the lookup.
#ifndef BATTERY_CELLS
#define BATTERY_CELLS 2
#endif

// Convert raw ADC value to battery voltage using current divider ratio
float adc_to_voltage(uint16_t adc_value);

// Convert battery voltage to state of charge (0-100%)
uint8_t voltage_to_soc(float voltage);

// Calibrate the divider ratio from a known real voltage and the current ADC reading.
// Returns the new ratio, or the current ratio if adc_value is 0.
float battery_calibrate(float measured_voltage, uint16_t adc_value);

// Directly set the divider ratio (e.g. restored from RTC memory).
void battery_calibrate_set_ratio(float ratio);
