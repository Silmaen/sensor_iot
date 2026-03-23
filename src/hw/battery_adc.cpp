#ifndef NATIVE

#include "hw/battery_adc.h"
#include "config.h"
#include <Arduino.h>

uint16_t read_battery_adc() {
    return static_cast<uint16_t>(analogRead(PIN_BATTERY_ADC));
}

#endif
