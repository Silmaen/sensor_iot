#pragma once

#include <stdint.h>

struct RtcData {
    uint32_t magic;
    uint32_t sleep_interval_s;
    float    battery_divider_ratio; // 0.0 = use compile-time default
};

class ISleep {
public:
    virtual ~ISleep() = default;
    virtual void deep_sleep(uint32_t seconds) = 0;
    virtual bool read_rtc_interval(uint32_t& interval_s) = 0;
    virtual void write_rtc_interval(uint32_t interval_s) = 0;
    virtual bool read_rtc_battery_ratio(float& ratio) = 0;
    virtual void write_rtc_battery_ratio(float ratio) = 0;
};
