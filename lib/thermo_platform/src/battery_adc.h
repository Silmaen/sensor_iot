#pragma once

#ifndef NATIVE

#include <stdint.h>

uint16_t read_battery_adc();

#ifdef PIN_BATTERY_SWITCH
void battery_adc_begin();
#endif

#endif
