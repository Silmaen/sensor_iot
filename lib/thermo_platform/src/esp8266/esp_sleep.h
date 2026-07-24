#pragma once

#ifndef NATIVE

#include "interfaces/i_sleep.h"
#include "diagnostics.h"  // DiagCounters

class EspSleep : public ISleep {
public:
    void deep_sleep(uint32_t seconds) override;
    bool read_rtc_interval(uint32_t& interval_s) override;
    void write_rtc_interval(uint32_t interval_s) override;
    bool read_rtc_battery_ratio(float& ratio) override;
    void write_rtc_battery_ratio(float ratio) override;

    // Diagnostics counters persistence across deep sleep (ESP8266 has no
    // RTC_DATA_ATTR like the ESP32; use a dedicated RTC user-memory slot).
    // read returns false when no valid blob is stored yet (cold boot).
    bool read_diag_counters(DiagCounters& out);
    void write_diag_counters(const DiagCounters& in);
};

#endif
