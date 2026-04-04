#pragma once

#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)

#include <stdint.h>

// Lightweight SAMD21 standby sleep using the internal RTC alarm.
// Unlike ESP8266 deep sleep, SAMD standby resumes execution (no reboot),
// so all variables survive — no RTC memory persistence needed.
class SamdSleep {
public:
    void begin();
    // Enter standby mode for the given duration. Execution resumes
    // at the next instruction after this call when the alarm fires.
    void standby(uint32_t seconds);
};

#endif
