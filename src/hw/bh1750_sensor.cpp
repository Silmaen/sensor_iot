#ifndef NATIVE

#include "hw/bh1750_sensor.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

// BH1750 opcodes
static constexpr uint8_t CMD_POWER_ON       = 0x01;
static constexpr uint8_t CMD_RESET          = 0x07;
static constexpr uint8_t CMD_ONE_TIME_H_RES = 0x20; // One-shot, 1 lx resolution, auto power-down

bool Bh1750Sensor::begin() {
    return begin(0x23);
}

bool Bh1750Sensor::begin(uint8_t addr) {
    addr_ = addr;
#ifdef ESP8266
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
#else
    Wire.begin();
#endif

    // Power on + reset to clear data register
    if (!send_command(CMD_POWER_ON)) return false;
    delay(1);
    if (!send_command(CMD_RESET)) return false;
    delay(1);

    // Verify: trigger a test read
    float test = read_lux();
    return test >= 0.0f;
}

float Bh1750Sensor::read_lux() {
    // Start one-time high-resolution measurement
    if (!send_command(CMD_ONE_TIME_H_RES))
        return -1.0f;

    // Wait for measurement (max 180ms for H-resolution)
    delay(180);

    // Read 2 bytes
    Wire.requestFrom(addr_, static_cast<uint8_t>(2));
    if (Wire.available() < 2)
        return -1.0f;

    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    uint16_t raw = (static_cast<uint16_t>(hi) << 8) | lo;

    // Convert: lux = raw / 1.2 (datasheet formula for H-resolution mode)
    return static_cast<float>(raw) / 1.2f;
}

bool Bh1750Sensor::send_command(uint8_t cmd) {
    Wire.beginTransmission(addr_);
    Wire.write(cmd);
    return Wire.endTransmission() == 0;
}

#endif
