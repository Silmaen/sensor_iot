#ifndef NATIVE

#include "hw_id.h"
#include <Arduino.h>
#include <stdio.h>

void get_hw_id(char* buf, size_t len) {
#if defined(ESP8266)
    snprintf(buf, len, "ESP-%06X", static_cast<unsigned int>(ESP.getChipId()));
#elif defined(ESP32)
    uint64_t mac = ESP.getEfuseMac();
    snprintf(buf, len, "C3-%06X", static_cast<unsigned int>(mac & 0xFFFFFF));
#elif defined(ARDUINO_SAMD_MKRWIFI1010)
    volatile uint32_t* uid = reinterpret_cast<volatile uint32_t*>(0x0080A00C);
    snprintf(buf, len, "MKR-%08lX",
             static_cast<unsigned long>(uid[0] ^ uid[1] ^ uid[2] ^ uid[3]));
#else
    snprintf(buf, len, "UNKNOWN");
#endif
}

#else // NATIVE

#include "hw_id.h"
#include <stdio.h>

void get_hw_id(char* buf, size_t len) {
    snprintf(buf, len, "TEST-000000");
}

#endif
