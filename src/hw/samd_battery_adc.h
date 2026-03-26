#pragma once

#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)

#include <stdint.h>

uint16_t read_battery_adc();

#endif
