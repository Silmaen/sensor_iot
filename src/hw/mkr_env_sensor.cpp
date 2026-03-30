#if !defined(NATIVE) && defined(ARDUINO_SAMD_MKRWIFI1010)

#include "hw/mkr_env_sensor.h"
#include <Arduino.h>

// --- HTS221 registers (from ST datasheet) ---
static constexpr uint8_t HTS221_WHO_AM_I    = 0x0F;
static constexpr uint8_t HTS221_WHO_AM_I_ID = 0xBC;
static constexpr uint8_t HTS221_CTRL_REG1   = 0x20;
static constexpr uint8_t HTS221_HUMIDITY_OUT_L  = 0x28;
static constexpr uint8_t HTS221_TEMP_OUT_L      = 0x2A;
// Calibration registers
static constexpr uint8_t HTS221_H0_RH_X2   = 0x30;
static constexpr uint8_t HTS221_H1_RH_X2   = 0x31;
static constexpr uint8_t HTS221_T0_DEGC_X8 = 0x32;
static constexpr uint8_t HTS221_T1_DEGC_X8 = 0x33;
static constexpr uint8_t HTS221_T1_T0_MSB  = 0x35;
static constexpr uint8_t HTS221_H0_T0_OUT_L = 0x36;
static constexpr uint8_t HTS221_H1_T0_OUT_L = 0x3A;
static constexpr uint8_t HTS221_T0_OUT_L   = 0x3C;
static constexpr uint8_t HTS221_T1_OUT_L   = 0x3E;

// --- LPS22HB registers (from ST datasheet) ---
static constexpr uint8_t LPS22HB_WHO_AM_I    = 0x0F;
static constexpr uint8_t LPS22HB_WHO_AM_I_ID = 0xB1;
static constexpr uint8_t LPS22HB_CTRL_REG1   = 0x10;
static constexpr uint8_t LPS22HB_CTRL_REG2   = 0x11;
static constexpr uint8_t LPS22HB_PRESS_OUT_XL = 0x28;

// --- VEML6075 registers (from Vishay datasheet) ---
static constexpr uint8_t VEML6075_UV_CONF  = 0x00;
static constexpr uint8_t VEML6075_UVA_DATA = 0x07;
static constexpr uint8_t VEML6075_UVB_DATA = 0x09;
static constexpr uint8_t VEML6075_UVCOMP1  = 0x0A;
static constexpr uint8_t VEML6075_UVCOMP2  = 0x0B;
static constexpr uint8_t VEML6075_ID       = 0x0C;
static constexpr uint16_t VEML6075_ID_VALUE = 0x0026;

// ============================================================
// Public API
// ============================================================

bool MkrEnvSensor::begin() {
    Wire.begin();
    analogReadResolution(12);

    if (!hts221_begin()) return false;
    if (!lps22hb_begin()) return false;
    if (!veml6075_begin()) return false;

    // TEMT6000 is analog, no init needed
    return true;
}

SensorData MkrEnvSensor::read() {
    SensorData data = {};

    hts221_read(data.temperature, data.humidity);
    lps22hb_read(data.pressure);
    veml6075_read(data.uv_index);
    temt6000_read(data.light_lux);

    data.valid = true;
    return data;
}

// ============================================================
// HTS221 (Temperature + Humidity)
// ============================================================

bool MkrEnvSensor::hts221_begin() {
    uint8_t id = read8(HTS221_ADDR, HTS221_WHO_AM_I);
    if (id != HTS221_WHO_AM_I_ID) {
        return false;
    }

    // Power on, BDU enabled, continuous mode, 1 Hz ODR
    write8(HTS221_ADDR, HTS221_CTRL_REG1, 0x85);
    delay(50);

    hts221_load_calibration();
    return true;
}

void MkrEnvSensor::hts221_load_calibration() {
    // Temperature calibration points
    uint8_t t0_x8 = read8(HTS221_ADDR, HTS221_T0_DEGC_X8);
    uint8_t t1_x8 = read8(HTS221_ADDR, HTS221_T1_DEGC_X8);
    uint8_t t1_t0_msb = read8(HTS221_ADDR, HTS221_T1_T0_MSB);

    uint16_t t0_degc_x8 = t0_x8 | (static_cast<uint16_t>(t1_t0_msb & 0x03) << 8);
    uint16_t t1_degc_x8 = t1_x8 | (static_cast<uint16_t>((t1_t0_msb >> 2) & 0x03) << 8);

    float t0_degc = t0_degc_x8 / 8.0f;
    float t1_degc = t1_degc_x8 / 8.0f;

    uint8_t t0_out_l = read8(HTS221_ADDR, HTS221_T0_OUT_L);
    uint8_t t0_out_h = read8(HTS221_ADDR, HTS221_T0_OUT_L + 1);
    int16_t t0_out = static_cast<int16_t>(t0_out_l | (static_cast<uint16_t>(t0_out_h) << 8));

    uint8_t t1_out_l = read8(HTS221_ADDR, HTS221_T1_OUT_L);
    uint8_t t1_out_h = read8(HTS221_ADDR, HTS221_T1_OUT_L + 1);
    int16_t t1_out = static_cast<int16_t>(t1_out_l | (static_cast<uint16_t>(t1_out_h) << 8));

    // Linear interpolation: T = slope * raw + intercept
    if (t1_out != t0_out) {
        hts_t_slope_ = (t1_degc - t0_degc) / static_cast<float>(t1_out - t0_out);
    }
    hts_t_intercept_ = t0_degc - hts_t_slope_ * static_cast<float>(t0_out);

    // Humidity calibration points
    uint8_t h0_rh_x2 = read8(HTS221_ADDR, HTS221_H0_RH_X2);
    uint8_t h1_rh_x2 = read8(HTS221_ADDR, HTS221_H1_RH_X2);

    float h0_rh = h0_rh_x2 / 2.0f;
    float h1_rh = h1_rh_x2 / 2.0f;

    uint8_t h0_out_l = read8(HTS221_ADDR, HTS221_H0_T0_OUT_L);
    uint8_t h0_out_h = read8(HTS221_ADDR, HTS221_H0_T0_OUT_L + 1);
    int16_t h0_out = static_cast<int16_t>(h0_out_l | (static_cast<uint16_t>(h0_out_h) << 8));

    uint8_t h1_out_l = read8(HTS221_ADDR, HTS221_H1_T0_OUT_L);
    uint8_t h1_out_h = read8(HTS221_ADDR, HTS221_H1_T0_OUT_L + 1);
    int16_t h1_out = static_cast<int16_t>(h1_out_l | (static_cast<uint16_t>(h1_out_h) << 8));

    if (h1_out != h0_out) {
        hts_h_slope_ = (h1_rh - h0_rh) / static_cast<float>(h1_out - h0_out);
    }
    hts_h_intercept_ = h0_rh - hts_h_slope_ * static_cast<float>(h0_out);
}

// In continuous mode at 1 Hz, output registers always hold the latest
// measurement. No status check needed when reading every >=1s.
void MkrEnvSensor::hts221_read(float& temperature, float& humidity) {
    // Read temperature (2 bytes, little-endian)
    uint8_t t_l = read8(HTS221_ADDR, HTS221_TEMP_OUT_L);
    uint8_t t_h = read8(HTS221_ADDR, HTS221_TEMP_OUT_L + 1);
    int16_t t_raw = static_cast<int16_t>(t_l | (static_cast<uint16_t>(t_h) << 8));
    temperature = hts_t_slope_ * static_cast<float>(t_raw) + hts_t_intercept_;

    // Read humidity (2 bytes, little-endian)
    uint8_t h_l = read8(HTS221_ADDR, HTS221_HUMIDITY_OUT_L);
    uint8_t h_h = read8(HTS221_ADDR, HTS221_HUMIDITY_OUT_L + 1);
    int16_t h_raw = static_cast<int16_t>(h_l | (static_cast<uint16_t>(h_h) << 8));
    humidity = hts_h_slope_ * static_cast<float>(h_raw) + hts_h_intercept_;

    // Clamp humidity to valid range
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;
}

// ============================================================
// LPS22HB (Pressure)
// ============================================================

bool MkrEnvSensor::lps22hb_begin() {
    uint8_t id = read8(LPS22HB_ADDR, LPS22HB_WHO_AM_I);
    if (id != LPS22HB_WHO_AM_I_ID) {
        return false;
    }

    // Software reset
    write8(LPS22HB_ADDR, LPS22HB_CTRL_REG2, 0x04);
    delay(10);

    // ODR = 10 Hz, BDU enabled
    write8(LPS22HB_ADDR, LPS22HB_CTRL_REG1, 0x32);
    delay(50);

    return true;
}

// In continuous mode, output registers always hold the latest measurement.
void MkrEnvSensor::lps22hb_read(float& pressure) {
    // Read pressure (3 bytes, little-endian, 24-bit two's complement)
    uint8_t p_xl = read8(LPS22HB_ADDR, LPS22HB_PRESS_OUT_XL);
    uint8_t p_l  = read8(LPS22HB_ADDR, LPS22HB_PRESS_OUT_XL + 1);
    uint8_t p_h  = read8(LPS22HB_ADDR, LPS22HB_PRESS_OUT_XL + 2);

    int32_t p_raw = static_cast<int32_t>(p_xl) |
                    (static_cast<int32_t>(p_l) << 8) |
                    (static_cast<int32_t>(p_h) << 16);
    // Sign-extend 24-bit to 32-bit
    if (p_raw & 0x800000) {
        p_raw |= 0xFF000000;
    }

    // Output in hPa (datasheet: raw / 4096 = hPa)
    pressure = static_cast<float>(p_raw) / 4096.0f;
}

// ============================================================
// VEML6075 (UV Index)
// ============================================================

bool MkrEnvSensor::veml6075_begin() {
    uint16_t id = read16_le(VEML6075_ADDR, VEML6075_ID);
    if (id != VEML6075_ID_VALUE) {
        return false;
    }

    // UV_CONF: power on, integration time 100ms, normal dynamic, no trigger
    write16_le(VEML6075_ADDR, VEML6075_UV_CONF, 0x0010);
    delay(10);

    return true;
}

void MkrEnvSensor::veml6075_read(float& uv_index) {
    float uva = static_cast<float>(read16_le(VEML6075_ADDR, VEML6075_UVA_DATA));
    float uvb = static_cast<float>(read16_le(VEML6075_ADDR, VEML6075_UVB_DATA));
    float comp1 = static_cast<float>(read16_le(VEML6075_ADDR, VEML6075_UVCOMP1));
    float comp2 = static_cast<float>(read16_le(VEML6075_ADDR, VEML6075_UVCOMP2));

    // Compensated UVA/UVB (Vishay application note)
    float uva_comp = uva - veml_uva_a_ * comp1 - veml_uva_b_ * comp2;
    float uvb_comp = uvb - veml_uvb_c_ * comp1 - veml_uvb_d_ * comp2;

    if (uva_comp < 0.0f) uva_comp = 0.0f;
    if (uvb_comp < 0.0f) uvb_comp = 0.0f;

    // UV index = average of UVA and UVB contributions
    uv_index = (uva_comp * veml_uva_resp_ + uvb_comp * veml_uvb_resp_) / 2.0f;
}

// ============================================================
// TEMT6000 (Ambient Light)
// ============================================================

void MkrEnvSensor::temt6000_read(float& lux) {
    // TEMT6000 on MKR ENV Shield is connected to A2
    // 12-bit ADC, 3.3V reference, ~2 lux per ADC count (linear response)
    int raw = analogRead(PIN_LIGHT);
    // Voltage across the load resistor (10k on shield)
    float voltage = raw * 3.3f / 4095.0f;
    // TEMT6000: ~10µA per lux, with 10kΩ load: V = I × R = lux × 10e-6 × 10e3 = lux × 0.01
    lux = voltage / 0.01f;
}

// ============================================================
// I2C helpers
// ============================================================

uint8_t MkrEnvSensor::read8(uint8_t dev_addr, uint8_t reg) const {
    Wire.beginTransmission(dev_addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(dev_addr, static_cast<uint8_t>(1));
    return Wire.read();
}

uint16_t MkrEnvSensor::read16_le(uint8_t dev_addr, uint8_t reg) const {
    Wire.beginTransmission(dev_addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(dev_addr, static_cast<uint8_t>(2));
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
}

void MkrEnvSensor::write8(uint8_t dev_addr, uint8_t reg, uint8_t value) const {
    Wire.beginTransmission(dev_addr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

void MkrEnvSensor::write16_le(uint8_t dev_addr, uint8_t reg, uint16_t value) const {
    Wire.beginTransmission(dev_addr);
    Wire.write(reg);
    Wire.write(static_cast<uint8_t>(value & 0xFF));
    Wire.write(static_cast<uint8_t>(value >> 8));
    Wire.endTransmission();
}

#endif
