#if defined(ESP8266) && !defined(NATIVE)

#include "hw/esp_sleep.h"
#include "config.h"
#include <Arduino.h>
#include <user_interface.h> // ESP8266 RTC memory API

// RTC memory slot (must be >= 64, user area starts at byte 128 = slot 64)
#define RTC_SLOT 64

void EspSleep::deep_sleep(uint32_t seconds) {
    ESP.deepSleep(static_cast<uint64_t>(seconds) * 1000000ULL);
}

bool EspSleep::read_rtc_interval(uint32_t& interval_s) {
    RtcData data;
    if (system_rtc_mem_read(RTC_SLOT, &data, sizeof(data))) {
        if (data.magic == RTC_MAGIC) {
            interval_s = data.sleep_interval_s;
            return true;
        }
    }
    return false;
}

void EspSleep::write_rtc_interval(uint32_t interval_s) {
    RtcData data;
    data.magic = RTC_MAGIC;
    data.sleep_interval_s = interval_s;
    system_rtc_mem_write(RTC_SLOT, &data, sizeof(data));
}

#endif
