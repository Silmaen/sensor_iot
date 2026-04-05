#pragma once

#ifndef NATIVE

#include <stdint.h>

class Bh1750Sensor {
public:
    // Initialize the sensor. Returns false if not detected on I2C bus.
    bool begin();
    bool begin(uint8_t addr);  // 0x23 (ADDR LOW/float) or 0x5C (ADDR HIGH)

    // Perform a one-shot high-resolution measurement.
    // Returns lux value, or -1.0f on failure.
    float read_lux();

private:
    uint8_t addr_ = 0x23;
    bool send_command(uint8_t cmd);
};

#endif
