#if defined(ESP8266) && !defined(NATIVE)

#include "esp8266/esp_sleep.h"
#include "config.h"
#include <Arduino.h>
#include <user_interface.h> // ESP8266 RTC memory API

// RTC memory slot (must be >= 64, user area starts at byte 128 = slot 64)
#define RTC_SLOT 64
// Diagnostics counters live in their own magic-guarded blob, past RtcData
// (slots 64-66). Slot 68 leaves a gap; DiagRtcBlob spans 8 slots (68-75).
#define RTC_DIAG_SLOT 68
struct DiagRtcBlob {
    uint32_t     magic;
    DiagCounters counters;
};

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
    // Read existing data to preserve battery_divider_ratio
    if (!system_rtc_mem_read(RTC_SLOT, &data, sizeof(data)) || data.magic != RTC_MAGIC) {
        data.battery_divider_ratio = 0.0f;
    }
    data.magic = RTC_MAGIC;
    data.sleep_interval_s = interval_s;
    system_rtc_mem_write(RTC_SLOT, &data, sizeof(data));
}

bool EspSleep::read_rtc_battery_ratio(float& ratio) {
    RtcData data;
    if (system_rtc_mem_read(RTC_SLOT, &data, sizeof(data))) {
        if (data.magic == RTC_MAGIC && data.battery_divider_ratio > 0.0f) {
            ratio = data.battery_divider_ratio;
            return true;
        }
    }
    return false;
}

void EspSleep::write_rtc_battery_ratio(float ratio) {
    RtcData data;
    // Read existing data to preserve sleep_interval_s
    if (!system_rtc_mem_read(RTC_SLOT, &data, sizeof(data)) || data.magic != RTC_MAGIC) {
        data.sleep_interval_s = DEFAULT_SLEEP_INTERVAL_S;
    }
    data.magic = RTC_MAGIC;
    data.battery_divider_ratio = ratio;
    system_rtc_mem_write(RTC_SLOT, &data, sizeof(data));
}

bool EspSleep::read_diag_counters(DiagCounters& out) {
    DiagRtcBlob b;
    if (system_rtc_mem_read(RTC_DIAG_SLOT, &b, sizeof(b)) && b.magic == RTC_MAGIC) {
        out = b.counters;
        return true;
    }
    return false;
}

void EspSleep::write_diag_counters(const DiagCounters& in) {
    DiagRtcBlob b;
    b.magic = RTC_MAGIC;
    b.counters = in;
    system_rtc_mem_write(RTC_DIAG_SLOT, &b, sizeof(b));
}

#endif
