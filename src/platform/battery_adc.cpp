// Unified battery ADC driver — platform-selected via #if.
// Supports optional MOSFET switch (PIN_BATTERY_SWITCH) to cut divider
// quiescent current during deep sleep.

#ifndef NATIVE

#include "platform/battery_adc.h"
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
    digitalWrite(PIN_BATTERY_SWITCH, HIGH);
    delayMicroseconds(500); // let RC settle
#endif

    // 12-bit resolution on SAMD and ESP32 (ESP8266 is always 10-bit)
#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ESP32)
    analogReadResolution(12);
#endif

    uint16_t raw = static_cast<uint16_t>(analogRead(PIN_BATTERY_ADC));

#ifdef PIN_BATTERY_SWITCH
    digitalWrite(PIN_BATTERY_SWITCH, LOW);
#endif

    return raw;
}

#endif
