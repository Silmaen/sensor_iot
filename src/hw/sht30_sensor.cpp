#ifndef NATIVE

#include "hw/sht30_sensor.h"
#include "config.h"
#include <Arduino.h>

// SHT3x-DIS commands (from Sensirion datasheet, Table 9)
// Single shot, high repeatability, no clock stretching
static constexpr uint8_t CMD_MEASURE_MSB = 0x24;
static constexpr uint8_t CMD_MEASURE_LSB = 0x00;

// Soft reset (Table 14)
static constexpr uint8_t CMD_RESET_MSB = 0x30;
static constexpr uint8_t CMD_RESET_LSB = 0xA2;

bool Sht30Sensor::begin() {
    return begin(0x44);
}

bool Sht30Sensor::begin(uint8_t addr) {
    addr_ = addr;
#ifdef ESP8266
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
#else
    Wire.begin();
#endif

    // Soft reset
    Wire.beginTransmission(addr_);
    Wire.write(CMD_RESET_MSB);
    Wire.write(CMD_RESET_LSB);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    delay(2); // Wait for reset (tSR max = 1.5ms)

    // Verify communication with a status register read (0xF32D)
    Wire.beginTransmission(addr_);
    Wire.write(0xF3);
    Wire.write(0x2D);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    delay(1);

    Wire.requestFrom(addr_, static_cast<uint8_t>(3));
    if (Wire.available() < 3) {
        return false;
    }
    uint8_t status_msb = Wire.read();
    uint8_t status_lsb = Wire.read();
    uint8_t status_crc = Wire.read();

    uint8_t check_data[2] = {status_msb, status_lsb};
    if (crc8(check_data, 2) != status_crc) {
        return false;
    }

    return true;
}

SensorData Sht30Sensor::read() {
    SensorData data = {};

    // Send single-shot measurement command (high repeatability, no clock stretching)
    Wire.beginTransmission(addr_);
    Wire.write(CMD_MEASURE_MSB);
    Wire.write(CMD_MEASURE_LSB);
    if (Wire.endTransmission() != 0) {
        data.valid = false;
        return data;
    }

    // Wait for measurement (high repeatability max = 15ms)
    delay(16);

    // Read 6 bytes: temp_MSB, temp_LSB, CRC, hum_MSB, hum_LSB, CRC
    Wire.requestFrom(addr_, static_cast<uint8_t>(6));
    if (Wire.available() < 6) {
        data.valid = false;
        return data;
    }

    uint8_t buf[6];
    for (uint8_t i = 0; i < 6; i++) {
        buf[i] = Wire.read();
    }

    // Verify CRC for temperature
    if (crc8(buf, 2) != buf[2]) {
        data.valid = false;
        return data;
    }

    // Verify CRC for humidity
    if (crc8(buf + 3, 2) != buf[5]) {
        data.valid = false;
        return data;
    }

    // Convert raw values (datasheet section 4.13)
    // T [degC] = -45 + 175 * S_T / (2^16 - 1)
    uint16_t raw_temp = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    data.temperature = -45.0f + 175.0f * static_cast<float>(raw_temp) / 65535.0f;

    // RH [%] = 100 * S_RH / (2^16 - 1)
    uint16_t raw_hum = (static_cast<uint16_t>(buf[3]) << 8) | buf[4];
    data.humidity = 100.0f * static_cast<float>(raw_hum) / 65535.0f;

    // SHT30 has no pressure sensor
    data.pressure = 0.0f;

    data.valid = true;
    return data;
}

// CRC-8 algorithm from SHT3x datasheet (Table 20)
// Polynomial: 0x31 (x^8 + x^5 + x^4 + 1), Init: 0xFF
uint8_t Sht30Sensor::crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

#endif
