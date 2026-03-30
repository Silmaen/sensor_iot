#pragma once

#if !defined(NATIVE) && defined(ARDUINO_SAMD_MKRWIFI1010)

#include "interfaces/i_sensor.h"
#include <Wire.h>
#include <stdint.h>

// MKR ENV Shield sensor driver
// Combines HTS221 (temperature + humidity), LPS22HB (pressure),
// TEMT6000 (ambient light), and VEML6075 (UV index)
class MkrEnvSensor : public ISensor {
public:
    bool begin() override;
    SensorData read() override;

private:
    // HTS221 (temperature + humidity)
    static constexpr uint8_t HTS221_ADDR = 0x5F;
    float hts_t_slope_ = 0.0f;
    float hts_t_intercept_ = 0.0f;
    float hts_h_slope_ = 0.0f;
    float hts_h_intercept_ = 0.0f;

    bool hts221_begin();
    void hts221_read(float& temperature, float& humidity);
    void hts221_load_calibration();

    // LPS22HB (pressure)
    static constexpr uint8_t LPS22HB_ADDR = 0x5C;

    bool lps22hb_begin();
    void lps22hb_read(float& pressure);

    // VEML6075 (UV)
    static constexpr uint8_t VEML6075_ADDR = 0x10;
    float veml_uva_a_ = 2.22f;   // UVA visible coefficient
    float veml_uva_b_ = 1.33f;   // UVA IR coefficient
    float veml_uvb_c_ = 2.95f;   // UVB visible coefficient
    float veml_uvb_d_ = 1.74f;   // UVB IR coefficient
    float veml_uva_resp_ = 0.001461f; // UVA responsivity
    float veml_uvb_resp_ = 0.002591f; // UVB responsivity

    bool veml6075_begin();
    void veml6075_read(float& uv_index);
    uint16_t read16_le(uint8_t dev_addr, uint8_t reg) const;

    // TEMT6000 (ambient light, analog)
    static constexpr uint8_t PIN_LIGHT = A2;

    void temt6000_read(float& lux);

    // I2C helpers
    uint8_t read8(uint8_t dev_addr, uint8_t reg) const;
    void write8(uint8_t dev_addr, uint8_t reg, uint8_t value) const;
    void write16_le(uint8_t dev_addr, uint8_t reg, uint16_t value) const;
};

#endif
