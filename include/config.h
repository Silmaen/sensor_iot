#pragma once

// --- Device identity (must be defined via -D build flags in platformio.ini) ---
#ifndef DEVICE_ID
#error "DEVICE_ID must be defined via build flags (-DDEVICE_ID='\"mydevice\"')"
#endif
#ifndef MQTT_DEVICE_TYPE
#error "MQTT_DEVICE_TYPE must be defined via build flags (-DMQTT_DEVICE_TYPE='\"thermo\"')"
#endif

// --- MQTT topics ---
#define MQTT_TOPIC_SENSORS      MQTT_DEVICE_TYPE "/" DEVICE_ID "/sensors"
#define MQTT_TOPIC_STATUS       MQTT_DEVICE_TYPE "/" DEVICE_ID "/status"
#define MQTT_TOPIC_COMMAND      MQTT_DEVICE_TYPE "/" DEVICE_ID "/command"
#define MQTT_TOPIC_CAPABILITIES MQTT_DEVICE_TYPE "/" DEVICE_ID "/capabilities"
#define MQTT_TOPIC_ACK          MQTT_DEVICE_TYPE "/" DEVICE_ID "/ack"

// --- MQTT settings ---
#define MQTT_PORT 1883

// --- Platform-specific pins ---
#if defined(ESP8266)

  // I2C pins (BME/BMP280)
  #define PIN_I2C_SCL 5   // D1
  #define PIN_I2C_SDA 4   // D2

  // Shift register pins (display)
  #define PIN_SR_DATA  13  // D7
  #define PIN_SR_CLOCK 14  // D5
  #define PIN_SR_LATCH 15  // D8

  // Button pin (display)
  #define PIN_BUTTON 12    // D6, active LOW

  // Battery ADC
  #define PIN_BATTERY_ADC A0

#elif defined(ARDUINO_SAMD_MKRWIFI1010)

  // I2C: default Wire pins (SDA/SCL defined by board variant)
  // Battery: built-in ADC pin with voltage divider
  #define PIN_BATTERY_ADC ADC_BATTERY

#endif

// --- Timing ---
#define DEFAULT_PUBLISH_INTERVAL_S 10   // Default publish interval (seconds)
#define BUTTON_DEBOUNCE_MS         50   // Button debounce time
#define DEFAULT_SLEEP_INTERVAL_S   300  // Default deep sleep interval (seconds)
#define MQTT_COMMAND_WAIT_MS       2000 // Wait time for retained commands
#define RECONNECT_INTERVAL_MS      5000 // Non-blocking reconnect retry interval

// --- Battery ---
#define BATTERY_LOW_THRESHOLD 15 // SoC percentage to trigger low_battery alert

// --- RTC memory (ESP8266 only) ---
#ifdef ESP8266
#define RTC_MAGIC 0xDEADBEEF
#endif
