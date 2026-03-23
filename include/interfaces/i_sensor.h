#pragma once

#include "sensor_data.h"

class ISensor {
public:
    virtual ~ISensor() = default;
    virtual bool begin() = 0;
    virtual SensorData read() = 0;
};
