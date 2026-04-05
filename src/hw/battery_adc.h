#pragma once

#ifndef NATIVE

#include <stdint.h>

uint16_t read_battery_adc();

// Initialize the battery divider switch GPIO (if PIN_BATTERY_SWITCH is defined).
// Call once in setup() before any ADC read.
#ifdef PIN_BATTERY_SWITCH
void battery_adc_begin();
#endif

#endif
