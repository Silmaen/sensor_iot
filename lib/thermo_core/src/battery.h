#pragma once

#include <stdint.h>

// Platform-specific battery constants
#if defined(ARDUINO_SAMD_MKRWIFI1010)
// MKR WiFi 1010: built-in 1S LiPo, 12-bit ADC
// BQ24195L PMIC routes battery voltage through an internal divider
// to ADC_BATTERY. Empirical ratio ~1.27 (calibrate with multimeter).
#ifndef BATTERY_DIVIDER_RATIO
#define BATTERY_DIVIDER_RATIO 1.27f
#endif
#define ADC_MAX_VALUE         4095
#define ADC_REF_VOLTAGE       3.3f
#define BATTERY_VOLTAGE_FULL  4.20f // 1S LiPo full
#define BATTERY_VOLTAGE_EMPTY 3.00f // 1S LiPo empty
#else
// ESP8266: external 2S LiPo, 10-bit ADC, R1=22k/R2=12k divider
// Empirical ratio (theoretical 2.833 = (22k+12k)/12k, calibrated with multimeter)
#ifndef BATTERY_DIVIDER_RATIO
#define BATTERY_DIVIDER_RATIO 2.833f
#endif
#define ADC_MAX_VALUE         1023
#define ADC_REF_VOLTAGE       3.3f
#define BATTERY_VOLTAGE_FULL  8.40f // 2S LiPo full
#define BATTERY_VOLTAGE_EMPTY 6.00f // 2S LiPo empty
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
