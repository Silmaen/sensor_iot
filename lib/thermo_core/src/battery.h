#pragma once

#include <stdint.h>

// Platform-specific battery constants
#if defined(ARDUINO_SAMD_MKRWIFI1010)
  // MKR WiFi 1010: built-in 1S LiPo, 12-bit ADC
  // BQ24195L PMIC routes battery voltage through an internal divider
  // to ADC_BATTERY. Empirical ratio ~1.27 (calibrate with multimeter).
  #define BATTERY_DIVIDER_RATIO 1.27f
  #define ADC_MAX_VALUE         4095
  #define ADC_REF_VOLTAGE       3.3f
  #define BATTERY_VOLTAGE_FULL  4.20f  // 1S LiPo full
  #define BATTERY_VOLTAGE_EMPTY 3.00f  // 1S LiPo empty
#else
  // ESP8266: external 2S LiPo, 10-bit ADC, R1=150k/R2=100k divider
  #define BATTERY_DIVIDER_RATIO 2.5f   // (150k+100k)/100k
  #define ADC_MAX_VALUE         1023
  #define ADC_REF_VOLTAGE       3.3f
  #define BATTERY_VOLTAGE_FULL  8.40f  // 2S LiPo full
  #define BATTERY_VOLTAGE_EMPTY 6.00f  // 2S LiPo empty
#endif

// Convert raw ADC value (0-1023) to battery voltage
float adc_to_voltage(uint16_t adc_value);

// Convert battery voltage to state of charge (0-100%)
uint8_t voltage_to_soc(float voltage);
