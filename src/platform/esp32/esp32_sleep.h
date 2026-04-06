#pragma once

#if defined(ESP32) && !defined(NATIVE)

#include "interfaces/i_sleep.h"

class Esp32Sleep : public ISleep {
public:
    void deep_sleep(uint32_t seconds) override;
    bool read_rtc_interval(uint32_t& interval_s) override;
    void write_rtc_interval(uint32_t interval_s) override;
    bool read_rtc_battery_ratio(float& ratio) override;
    void write_rtc_battery_ratio(float ratio) override;
};

#endif
