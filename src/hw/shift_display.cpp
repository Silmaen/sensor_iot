#if defined(ESP8266) && !defined(NATIVE)

#include "hw/shift_display.h"
#include "config.h"
#include <Arduino.h>

void ShiftDisplay::begin() {
    pinMode(PIN_SR_DATA, OUTPUT);
    pinMode(PIN_SR_CLOCK, OUTPUT);
    pinMode(PIN_SR_LATCH, OUTPUT);
    show(0x0000);
}

void ShiftDisplay::show(uint16_t data) {
    // Shift 16 bits MSB-first: HC595 #2 data first, then HC595 #1
    digitalWrite(PIN_SR_LATCH, LOW);
    shiftOut(PIN_SR_DATA, PIN_SR_CLOCK, MSBFIRST, (data >> 8) & 0xFF);
    shiftOut(PIN_SR_DATA, PIN_SR_CLOCK, MSBFIRST, data & 0xFF);
    digitalWrite(PIN_SR_LATCH, HIGH);
}

#endif
