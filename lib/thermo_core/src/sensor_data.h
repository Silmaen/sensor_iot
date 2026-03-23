#pragma once

struct SensorData {
    float temperature;  // Celsius
    float humidity;     // % RH
    float pressure;     // hPa
    bool valid;         // true if sensor read succeeded
};
