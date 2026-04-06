#pragma once

#ifndef NATIVE

#include "interfaces/i_sensor.h"
#include <Wire.h>
#include <stdint.h>

class Sht30Sensor : public ISensor {
public:
    bool begin() override;
    bool begin(uint8_t addr);
    SensorData read() override;

private:
    uint8_t addr_ = 0x44;

    static uint8_t crc8(const uint8_t* data, uint8_t len);
};

#endif
