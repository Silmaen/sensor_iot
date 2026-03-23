#pragma once

#include <stdint.h>

class IDisplay {
public:
    virtual ~IDisplay() = default;
    virtual void begin() = 0;
    virtual void show(uint16_t data) = 0;
};
