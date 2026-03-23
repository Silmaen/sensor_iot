#pragma once

#ifndef NATIVE

#include "interfaces/i_sleep.h"

class EspSleep : public ISleep {
public:
    void deep_sleep(uint32_t seconds) override;
    bool read_rtc_interval(uint32_t& interval_s) override;
    void write_rtc_interval(uint32_t interval_s) override;
};

#endif
