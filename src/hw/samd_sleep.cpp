#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)

#include "hw/samd_sleep.h"
#include <RTCZero.h>

static RTCZero rtc;

// Empty ISR — the alarm interrupt just needs to wake the CPU.
static void alarm_isr() {}

void SamdSleep::begin() {
    rtc.begin();
    rtc.attachInterrupt(alarm_isr);
}

void SamdSleep::standby(uint32_t seconds) {
    if (seconds == 0) return;
    // Clamp to 23:59:59 (max single RTC alarm span)
    if (seconds > 86399) seconds = 86399;

    // Reset RTC time to 00:00:00, set alarm at +seconds
    rtc.setTime(0, 0, 0);
    rtc.setDate(1, 1, 0);

    uint8_t h = seconds / 3600;
    uint8_t m = (seconds % 3600) / 60;
    uint8_t s = seconds % 60;

    rtc.setAlarmTime(h, m, s);
    rtc.enableAlarm(rtc.MATCH_HHMMSS);
    rtc.standbyMode();
    // Execution resumes here after alarm
}

#endif
