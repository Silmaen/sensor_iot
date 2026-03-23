#pragma once

#ifndef NATIVE

#include "interfaces/i_sensor.h"
#include <Wire.h>
#include <stdint.h>

class Bme280Sensor : public ISensor {
public:
    bool begin() override;
    bool begin(uint8_t addr);
    SensorData read() override;

    // Returns true if chip is BME280 (has humidity), false if BMP280
    bool has_humidity() const { return has_humidity_; }

private:
    uint8_t addr_ = 0x76;
    bool has_humidity_ = false;

    // Temperature calibration
    uint16_t dig_T1_ = 0;
    int16_t  dig_T2_ = 0;
    int16_t  dig_T3_ = 0;

    // Pressure calibration
    uint16_t dig_P1_ = 0;
    int16_t  dig_P2_ = 0;
    int16_t  dig_P3_ = 0;
    int16_t  dig_P4_ = 0;
    int16_t  dig_P5_ = 0;
    int16_t  dig_P6_ = 0;
    int16_t  dig_P7_ = 0;
    int16_t  dig_P8_ = 0;
    int16_t  dig_P9_ = 0;

    // Humidity calibration (BME280 only)
    uint8_t  dig_H1_ = 0;
    int16_t  dig_H2_ = 0;
    uint8_t  dig_H3_ = 0;
    int16_t  dig_H4_ = 0;
    int16_t  dig_H5_ = 0;
    int8_t   dig_H6_ = 0;

    // Fine temperature for compensation
    int32_t t_fine_ = 0;

    void read_calibration();
    uint8_t read8(uint8_t reg) const;
    uint16_t read16_le(uint8_t reg) const;
    int16_t read_s16_le(uint8_t reg) const;
    void write8(uint8_t reg, uint8_t value) const;

    float compensate_temperature(int32_t adc_T);
    float compensate_pressure(int32_t adc_P) const;
    float compensate_humidity(int32_t adc_H) const;
};

#endif
