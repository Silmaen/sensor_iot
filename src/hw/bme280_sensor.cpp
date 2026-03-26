#ifndef NATIVE

#include "hw/bme280_sensor.h"
#include "config.h"
#include <Arduino.h>

// BME280 registers (from Bosch datasheet BST-BME280-DS002)
static constexpr uint8_t REG_CHIP_ID   = 0xD0;
static constexpr uint8_t REG_RESET     = 0xE0;
static constexpr uint8_t REG_CTRL_HUM  = 0xF2;
static constexpr uint8_t REG_STATUS    = 0xF3;
static constexpr uint8_t REG_CTRL_MEAS = 0xF4;
static constexpr uint8_t REG_CONFIG    = 0xF5;
static constexpr uint8_t REG_DATA      = 0xF7; // 8 bytes: press[2:0], temp[2:0], hum[1:0]

static constexpr uint8_t BME280_CHIP_ID  = 0x60;
static constexpr uint8_t BMP280_CHIP_ID  = 0x58;
static constexpr uint8_t RESET_CMD       = 0xB6;

bool Bme280Sensor::begin() {
    return begin(0x76);
}

bool Bme280Sensor::begin(uint8_t addr) {
    addr_ = addr;
#ifdef ESP8266
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
#else
    Wire.begin();
#endif

    uint8_t chip_id = read8(REG_CHIP_ID);
    if (chip_id == BME280_CHIP_ID) {
        has_humidity_ = true;
    } else if (chip_id == BMP280_CHIP_ID) {
        has_humidity_ = false;
    } else {
        return false;
    }

    // Soft reset
    write8(REG_RESET, RESET_CMD);
    delay(10);

    // Wait for NVM copy
    while (read8(REG_STATUS) & 0x01) {
        delay(1);
    }

    read_calibration();

    // Config: standby 1000ms, filter coeff 16, no SPI
    write8(REG_CONFIG, (0x05 << 5) | (0x04 << 2));

    // Humidity oversampling x1 (must be set before ctrl_meas)
    if (has_humidity_) {
        write8(REG_CTRL_HUM, 0x01);
    }

    // Temp oversampling x2, pressure oversampling x16, normal mode
    write8(REG_CTRL_MEAS, (0x02 << 5) | (0x05 << 2) | 0x03);

    return true;
}

void Bme280Sensor::read_calibration() {
    // Temperature and pressure calibration (0x88..0xA1)
    dig_T1_ = read16_le(0x88);
    dig_T2_ = read_s16_le(0x8A);
    dig_T3_ = read_s16_le(0x8C);

    dig_P1_ = read16_le(0x8E);
    dig_P2_ = read_s16_le(0x90);
    dig_P3_ = read_s16_le(0x92);
    dig_P4_ = read_s16_le(0x94);
    dig_P5_ = read_s16_le(0x96);
    dig_P6_ = read_s16_le(0x98);
    dig_P7_ = read_s16_le(0x9A);
    dig_P8_ = read_s16_le(0x9C);
    dig_P9_ = read_s16_le(0x9E);

    if (!has_humidity_) return;

    // Humidity calibration
    dig_H1_ = read8(0xA1);
    dig_H2_ = read_s16_le(0xE1);
    dig_H3_ = read8(0xE3);
    uint8_t e4 = read8(0xE4);
    uint8_t e5 = read8(0xE5);
    uint8_t e6 = read8(0xE6);
    dig_H4_ = static_cast<int16_t>((static_cast<int16_t>(e4) << 4) | (e5 & 0x0F));
    dig_H5_ = static_cast<int16_t>((static_cast<int16_t>(e6) << 4) | ((e5 >> 4) & 0x0F));
    dig_H6_ = static_cast<int8_t>(read8(0xE7));
}

SensorData Bme280Sensor::read() {
    SensorData data = {};

    // Read raw data starting at 0xF7
    Wire.beginTransmission(addr_);
    Wire.write(REG_DATA);
    if (Wire.endTransmission() != 0) {
        data.valid = false;
        return data;
    }

    uint8_t count = has_humidity_ ? 8 : 6;
    Wire.requestFrom(addr_, count);
    if (Wire.available() < count) {
        data.valid = false;
        return data;
    }

    uint8_t buf[8];
    for (uint8_t i = 0; i < count; i++) {
        buf[i] = Wire.read();
    }

    const int32_t adc_P = (static_cast<int32_t>(buf[0]) << 12) |
                    (static_cast<int32_t>(buf[1]) << 4) | (buf[2] >> 4);
    const int32_t adc_T = (static_cast<int32_t>(buf[3]) << 12) |
                    (static_cast<int32_t>(buf[4]) << 4) | (buf[5] >> 4);

    // Temperature must be compensated first (sets t_fine_)
    data.temperature = compensate_temperature(adc_T);
    data.pressure = compensate_pressure(adc_P);

    if (has_humidity_) {
      const int32_t adc_H = (static_cast<int32_t>(buf[6]) << 8) | buf[7];
        data.humidity = compensate_humidity(adc_H);
    } else {
        data.humidity = 0.0f;
    }

    data.valid = true;
    return data;
}

// Compensation formulas from Bosch BME280 datasheet section 4.2.3

float Bme280Sensor::compensate_temperature(const int32_t adc_T) {
  const int32_t var1 = (((adc_T >> 3) - (static_cast<int32_t>(dig_T1_) << 1)) *
                    static_cast<int32_t>(dig_T2_)) >> 11;
  const int32_t var2 = ((((adc_T >> 4) - static_cast<int32_t>(dig_T1_)) *
                     ((adc_T >> 4) - static_cast<int32_t>(dig_T1_))) >> 12) *
                   static_cast<int32_t>(dig_T3_) >> 14;
    t_fine_ = var1 + var2;
    return static_cast<float>((t_fine_ * 5 + 128) >> 8) / 100.0f;
}

float Bme280Sensor::compensate_pressure(const int32_t adc_P) const {
    int64_t var1 = static_cast<int64_t>(t_fine_) - 128000;
    int64_t var2 = var1 * var1 * static_cast<int64_t>(dig_P6_);
    var2 = var2 + ((var1 * static_cast<int64_t>(dig_P5_)) << 17);
    var2 = var2 + (static_cast<int64_t>(dig_P4_) << 35);
    var1 = ((var1 * var1 * static_cast<int64_t>(dig_P3_)) >> 8) +
           ((var1 * static_cast<int64_t>(dig_P2_)) << 12);
    var1 = ((static_cast<int64_t>(1) << 47) + var1) *
           static_cast<int64_t>(dig_P1_) >> 33;

    if (var1 == 0) return 0.0f;

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (static_cast<int64_t>(dig_P9_) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (static_cast<int64_t>(dig_P8_) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (static_cast<int64_t>(dig_P7_) << 4);

    return static_cast<float>(static_cast<uint32_t>(p)) / 25600.0f; // Pa -> hPa
}

float Bme280Sensor::compensate_humidity(const int32_t adc_H) const {
    int32_t v = t_fine_ - 76800;
    v = ((((adc_H << 14) - (static_cast<int32_t>(dig_H4_) << 20) -
           (static_cast<int32_t>(dig_H5_) * v)) + 16384) >> 15) *
        (((((((v * static_cast<int32_t>(dig_H6_)) >> 10) *
             (((v * static_cast<int32_t>(dig_H3_)) >> 11) + 32768)) >> 10) +
           2097152) * static_cast<int32_t>(dig_H2_) + 8192) >> 14);
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * static_cast<int32_t>(dig_H1_)) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    return static_cast<float>(static_cast<uint32_t>(v >> 12)) / 1024.0f;
}

// Low-level I2C helpers

uint8_t Bme280Sensor::read8(uint8_t reg) const {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(addr_, static_cast<uint8_t>(1));
    return Wire.read();
}

uint16_t Bme280Sensor::read16_le(uint8_t reg) const {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(addr_, static_cast<uint8_t>(2));
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    return static_cast<uint16_t>(hi << 8) | lo;
}

int16_t Bme280Sensor::read_s16_le(uint8_t reg) const {
    return static_cast<int16_t>(read16_le(reg));
}

void Bme280Sensor::write8(uint8_t reg, uint8_t value) const {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

#endif
