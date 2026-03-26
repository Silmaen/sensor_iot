#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)

#include "hw/samd_battery_adc.h"
#include "config.h"
#include <Arduino.h>

uint16_t read_battery_adc() {
    analogReadResolution(12);
    return static_cast<uint16_t>(analogRead(PIN_BATTERY_ADC));
}

#endif
