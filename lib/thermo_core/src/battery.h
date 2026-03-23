#pragma once

#include <stdint.h>

// Hardware constants for battery monitoring
// R1=150k, R2=100k voltage divider
#define BATTERY_DIVIDER_RATIO 2.5f  // (150k+100k)/100k

// ESP8266 ADC
#define ADC_MAX_VALUE 1023
#define ADC_REF_VOLTAGE 3.3f

// 2S LiPo thresholds
#define BATTERY_VOLTAGE_FULL  8.40f
#define BATTERY_VOLTAGE_EMPTY 6.00f

// Convert raw ADC value (0-1023) to battery voltage
float adc_to_voltage(uint16_t adc_value);

// Convert battery voltage to state of charge (0-100%)
uint8_t voltage_to_soc(float voltage);
