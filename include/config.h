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

  // Battery divider power switch (optional, needs N-MOSFET on this pin).
  // When defined, the divider is only powered during ADC reads (~1ms),
  // saving ~247µA quiescent current during deep sleep.
  // #define PIN_BATTERY_SWITCH 0  // D3 (GPIO0) — uncomment when MOSFET is wired

#elif defined(ARDUINO_SAMD_MKRWIFI1010)

  // I2C: default Wire pins (SDA/SCL defined by board variant)
  // Battery: built-in ADC pin with voltage divider
  #define PIN_BATTERY_ADC ADC_BATTERY

#endif

// --- Timing ---
#ifdef HAS_SERIAL_DEBUG
  #define DEFAULT_PUBLISH_INTERVAL_S 10   // Fast feedback for debugging (seconds)
#else
  #define DEFAULT_PUBLISH_INTERVAL_S 300  // Production: 5 minutes
#endif
#define BUTTON_DEBOUNCE_MS         50   // Button debounce time
#define DEFAULT_SLEEP_INTERVAL_S   300  // Default deep sleep interval (seconds)
#define MQTT_COMMAND_WAIT_MS       2000 // Wait window for server command flush after publish
#define RECONNECT_INTERVAL_MS      5000 // Non-blocking reconnect retry interval

// --- Battery alerts ---
// Two-level alerts: warning (plan to recharge) and critical (imminent shutdown)
#if defined(ARDUINO_SAMD_MKRWIFI1010)
  // 1S LiPo: 3.0–4.2V range, narrow margin before BQ24195L cutoff
  #define BATTERY_WARN_THRESHOLD     20  // SoC% → warning "low_battery"
  #define BATTERY_CRITICAL_THRESHOLD  5  // SoC% → error "critical_battery"
#else
  // 2S LiPo: 6.0–8.4V range, wider margin
  #define BATTERY_WARN_THRESHOLD     15  // SoC% → warning "low_battery"
  #define BATTERY_CRITICAL_THRESHOLD  5  // SoC% → error "critical_battery"
#endif

// --- RTC memory (ESP8266 only) ---
#ifdef ESP8266
#define RTC_MAGIC 0xDEADBEEF
#endif
