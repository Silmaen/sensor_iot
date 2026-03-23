#pragma once

#ifndef NATIVE

#include "interfaces/i_display.h"

class ShiftDisplay : public IDisplay {
public:
    void begin() override;
    void show(uint16_t data) override;
};

#endif
