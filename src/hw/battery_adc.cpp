#if defined(ESP8266) && !defined(NATIVE)

#include "hw/battery_adc.h"
#include "config.h"
#include <Arduino.h>

#ifdef PIN_BATTERY_SWITCH
void battery_adc_begin() {
    pinMode(PIN_BATTERY_SWITCH, OUTPUT);
    digitalWrite(PIN_BATTERY_SWITCH, LOW);
}
#endif

uint16_t read_battery_adc() {
#ifdef PIN_BATTERY_SWITCH
    // Power on the voltage divider via MOSFET gate
    digitalWrite(PIN_BATTERY_SWITCH, HIGH);
    delayMicroseconds(500); // let RC settle (10k source impedance + ADC cap)
#endif

    uint16_t raw = static_cast<uint16_t>(analogRead(PIN_BATTERY_ADC));

#ifdef PIN_BATTERY_SWITCH
    digitalWrite(PIN_BATTERY_SWITCH, LOW);
#endif

    return raw;
}

#endif
