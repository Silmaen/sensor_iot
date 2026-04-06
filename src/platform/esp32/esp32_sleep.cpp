#if defined(ESP32) && !defined(NATIVE)

#include "platform/esp32/esp32_sleep.h"
#include "config.h"
#include <Arduino.h>
#include <esp_sleep.h>

// RTC memory survives deep sleep on ESP32 (not power-off).
static constexpr uint32_t RTC_MAGIC_VALUE = 0xDEADBEEF;

static RTC_DATA_ATTR uint32_t rtc_magic = 0;
static RTC_DATA_ATTR uint32_t rtc_interval = 0;
static RTC_DATA_ATTR float    rtc_battery_ratio = 0.0f;

void Esp32Sleep::deep_sleep(uint32_t seconds) {
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
    esp_deep_sleep_start();
}

bool Esp32Sleep::read_rtc_interval(uint32_t& interval_s) {
    if (rtc_magic == RTC_MAGIC_VALUE) {
        interval_s = rtc_interval;
        return true;
    }
    return false;
}

void Esp32Sleep::write_rtc_interval(uint32_t interval_s) {
    rtc_magic = RTC_MAGIC_VALUE;
    rtc_interval = interval_s;
}

bool Esp32Sleep::read_rtc_battery_ratio(float& ratio) {
    if (rtc_magic == RTC_MAGIC_VALUE && rtc_battery_ratio > 0.0f) {
        ratio = rtc_battery_ratio;
        return true;
    }
    return false;
}

void Esp32Sleep::write_rtc_battery_ratio(float ratio) {
    rtc_magic = RTC_MAGIC_VALUE;
    rtc_battery_ratio = ratio;
}

#endif
