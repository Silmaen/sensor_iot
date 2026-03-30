#pragma once

struct SensorData {
    float temperature;  // Celsius
    float humidity;     // % RH
    float pressure;     // hPa
    float light_lux;    // lux (ambient light)
    float uv_index;     // UV index
    bool valid;         // true if sensor read succeeded
};
