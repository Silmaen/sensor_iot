#include "config.h"
#include "debug.h"
#include "module_registry.h"
#include "mqtt_payload.h"
#include "payload_builder.h"

#ifdef HAS_BME280
#include "modules/bme280_module.h"
#endif

#ifdef HAS_BATTERY
#include "modules/battery_module.h"
#endif

#ifndef NATIVE
#include <Arduino.h>

#if defined(ARDUINO_SAMD_MKRWIFI1010)
#include "hw/nina_network.h"
static NinaNetwork network;
#else
#include "hw/esp_network.h"
static EspNetwork network;
#endif

#ifdef HAS_BME280
#include "hw/bme280_sensor.h"
static Bme280Sensor sensor;
#endif

#ifdef HAS_DISPLAY
#include "display_encoding.h"
#include "hw/shift_display.h"
static ShiftDisplay display;

enum class DisplayMode : uint8_t { TEMPERATURE, HUMIDITY, PRESSURE, COUNT };
static DisplayMode current_mode         = DisplayMode::TEMPERATURE;
static unsigned long last_button_change = 0;
static bool last_button_state           = true;
#endif

#ifdef HAS_BATTERY
#include "battery.h"
#if defined(ARDUINO_SAMD_MKRWIFI1010)
#include "hw/samd_battery_adc.h"
#else
#include "hw/battery_adc.h"
#endif
#endif

#ifdef HAS_DEEP_SLEEP
#include "hw/esp_sleep.h"
static EspSleep sleeper;
#endif

#endif // NATIVE

// --- Shared state ---
static ModuleRegistry registry;
static uint32_t publish_interval_s = DEFAULT_PUBLISH_INTERVAL_S;

#if defined(HAS_BME280) && !defined(NATIVE)
static SensorData last_data = {};
#endif

// --- Command handler: set_interval ---
static bool handle_set_interval(const char* payload) {
    uint32_t value = 0;
    if (!parse_command_value(payload, value))
        return false;
    DEBUG_PRINTF("[CMD] set_interval: %lu -> %lu\n", (unsigned long)publish_interval_s, (unsigned long)value);
    publish_interval_s = value;
#if defined(HAS_DEEP_SLEEP) && !defined(NATIVE)
    sleeper.write_rtc_interval(value);
#endif
    return true;
}

static const CommandParamDef set_interval_params[] = {
    {"value", "number"},
};

// --- Module registration ---
static void register_modules() {
    registry.init();
    registry.add_command("set_interval", handle_set_interval,
                         set_interval_params, 1);
    registry.add_metric("wifi_rssi", "dBm");
#ifdef HAS_BME280
    bme280_module_register(registry);
#endif
#ifdef HAS_BATTERY
    battery_module_register(registry);
#endif
}

#ifndef NATIVE

// --- Build and publish sensor payload ---
static void publish_sensor_data() {
    char buf[256];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
#ifdef HAS_BME280
    bme280_module_contribute(pb, last_data);
#endif
#ifdef HAS_BATTERY
    uint16_t adc_raw = read_battery_adc();
    float voltage    = adc_to_voltage(adc_raw);
    uint8_t soc      = voltage_to_soc(voltage);
    battery_module_contribute(pb, soc, voltage);
#endif
    pb.add_int("wifi_rssi", network.wifi_rssi());
    pb.end();
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", MQTT_TOPIC_SENSORS, buf);
    network.publish(MQTT_TOPIC_SENSORS, buf);

#ifdef HAS_BATTERY
    if (soc <= BATTERY_LOW_THRESHOLD) {
        char status_buf[64];
        format_status_payload("warning", "low_battery", status_buf, sizeof(status_buf));
        DEBUG_PRINTF("[MQTT] publish %s: %s\n", MQTT_TOPIC_STATUS, status_buf);
        network.publish(MQTT_TOPIC_STATUS, status_buf);
    }
#endif
}

// --- Platform-specific hardware ID ---
static void get_hw_id(char* buf, size_t len) {
#if defined(ESP8266)
    snprintf(buf, len, "ESP-%06X", static_cast<unsigned int>(ESP.getChipId()));
#elif defined(ARDUINO_SAMD_MKRWIFI1010)
    // SAMD21 unique ID: 4 x 32-bit words at 0x0080A00C
    volatile uint32_t* uid = reinterpret_cast<volatile uint32_t*>(0x0080A00C);
    snprintf(buf, len, "MKR-%08lX", static_cast<unsigned long>(uid[0] ^ uid[1] ^ uid[2] ^ uid[3]));
#else
    snprintf(buf, len, "UNKNOWN");
#endif
}

// --- Publish capabilities from registry ---
static void publish_capabilities() {
    char hw_id[20];
    get_hw_id(hw_id, sizeof(hw_id));

    char buf[512];
    format_capabilities_payload(hw_id,
                                publish_interval_s,
                                registry,
                                buf,
                                sizeof(buf));
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", MQTT_TOPIC_CAPABILITIES, buf);
    network.publish(MQTT_TOPIC_CAPABILITIES, buf);
}

// --- MQTT message callback ---
static void on_mqtt_message(char* topic, uint8_t* payload, unsigned int length) {
    DEBUG_PRINTF("[MQTT] << topic=%s len=%u\n", topic, length);
    char buf[128];
    unsigned int copy_len = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
    memcpy(buf, payload, copy_len);
    buf[copy_len] = '\0';
    DEBUG_PRINTF("[MQTT] << payload: %s\n", buf);

    char action[32];
    if (!parse_command_action(buf, action, sizeof(action))) {
        DEBUG_PRINTF("[MQTT] parse_command_action failed for: %s\n", buf);
        return;
    }

    DEBUG_PRINTF("[MQTT] command received: action=%s\n", action);

    if (strcmp(action, "request_capabilities") == 0) {
        DEBUG_PRINTLN("[MQTT] publishing capabilities (requested)");
        publish_capabilities();
        return;
    }

    bool ok = registry.dispatch(action, buf);
    if (!ok) {
        DEBUG_PRINTF("[MQTT] unknown command: %s\n", action);
    }

    // Acknowledge the command
    char ack_buf[96];
    format_ack_payload(action, ok ? "ok" : "error", ack_buf, sizeof(ack_buf));
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", MQTT_TOPIC_ACK, ack_buf);
    network.publish(MQTT_TOPIC_ACK, ack_buf);
}

#ifdef HAS_DISPLAY
static void update_display() {
    uint16_t encoded = 0;
    switch (current_mode) {
    case DisplayMode::TEMPERATURE:
        encoded = encode_temperature(last_data.temperature);
        break;
    case DisplayMode::HUMIDITY:
        encoded = encode_humidity(last_data.humidity);
        break;
    case DisplayMode::PRESSURE:
        encoded = encode_pressure(last_data.pressure);
        break;
    default:
        break;
    }
    display.show(encoded);
}

static void handle_button() {
    bool state = digitalRead(PIN_BUTTON);
    if (state != last_button_state) {
        unsigned long now = millis();
        if (now - last_button_change >= BUTTON_DEBOUNCE_MS) {
            last_button_change = now;
            last_button_state  = state;
            if (!state) {
                uint8_t mode = static_cast<uint8_t>(current_mode);
                mode         = (mode + 1) % static_cast<uint8_t>(DisplayMode::COUNT);
                current_mode = static_cast<DisplayMode>(mode);
                update_display();
            }
        }
    }
}
#endif // HAS_DISPLAY

#endif // NATIVE

// ============================================================
// Arduino entry points
// ============================================================

void setup() {
    register_modules();

#ifndef NATIVE
    Serial.begin(115200);
    Serial.println("Thermo starting...");
    DEBUG_PRINTF("[INIT] device=%s type=%s\n", DEVICE_ID, MQTT_DEVICE_TYPE);
    DEBUG_PRINTF("[INIT] publish_interval=%lus\n", (unsigned long)publish_interval_s);

#ifdef HAS_DEEP_SLEEP
    if (!sleeper.read_rtc_interval(publish_interval_s)) {
        publish_interval_s = DEFAULT_SLEEP_INTERVAL_S;
    }
#endif

#ifdef HAS_BME280
    if (!sensor.begin()) {
        Serial.println("BME280 init failed!");
#ifdef HAS_DEEP_SLEEP
        sleeper.deep_sleep(publish_interval_s);
        return;
#endif
    }
#endif

#ifdef HAS_DISPLAY
    display.begin();
    pinMode(PIN_BUTTON, INPUT_PULLUP);
#endif

    network.set_callback(on_mqtt_message);

    DEBUG_PRINTLN("[NET] connecting WiFi...");
    if (!network.connect_wifi()) {
        Serial.println("WiFi connection failed!");
#ifdef HAS_DEEP_SLEEP
        sleeper.deep_sleep(publish_interval_s);
        return;
#else
        DEBUG_PRINTLN("[NET] will retry in loop");
#endif
    } else {
        DEBUG_PRINTLN("[NET] WiFi connected");
        DEBUG_PRINTLN("[NET] connecting MQTT...");
        if (!network.connect_mqtt()) {
            Serial.println("MQTT connection failed!");
#ifdef HAS_DEEP_SLEEP
            sleeper.deep_sleep(publish_interval_s);
            return;
#else
            DEBUG_PRINTLN("[NET] will retry in loop");
#endif
        } else {
            DEBUG_PRINTLN("[NET] MQTT connected");
            network.subscribe(MQTT_TOPIC_COMMAND);
            DEBUG_PRINTF("[MQTT] subscribed to %s\n", MQTT_TOPIC_COMMAND);
        }
    }

#ifdef HAS_DEEP_SLEEP
    // One-shot mode: wait for retained commands, read, publish, sleep

    volatile bool cmd_received = false;
    unsigned long start        = millis();
    while (!cmd_received && millis() - start < MQTT_COMMAND_WAIT_MS) {
        network.loop();
        delay(10);
    }

#ifdef HAS_BME280
    last_data = sensor.read();
    if (last_data.valid) {
        publish_sensor_data();
    }
#else
    publish_sensor_data();
#endif

    network.disconnect();
    sleeper.deep_sleep(publish_interval_s);
#endif // HAS_DEEP_SLEEP

#endif // NATIVE
}

static unsigned long last_sensor_read = 0;

void loop() {
#ifdef HAS_DEEP_SLEEP
    // Deep sleep resets to setup(), loop() should never run
    return;
#endif

#ifndef NATIVE
    network.loop();

#ifdef HAS_DISPLAY
    handle_button();
#endif

    if (unsigned long const now = millis();
        now - last_sensor_read >= static_cast<unsigned long>(publish_interval_s) * 1000UL) {
        last_sensor_read = now;

#ifdef HAS_BME280
        last_data = sensor.read();
        if (!last_data.valid) {
            DEBUG_PRINTLN("[SENSOR] BME280 read failed");
            return;
        }
        DEBUG_PRINTF("[SENSOR] T=%.1f H=%.1f P=%.1f\n", last_data.temperature, last_data.humidity, last_data.pressure);
#ifdef HAS_DISPLAY
        update_display();
#endif
#endif
        if (!network.mqtt_connected()) {
            DEBUG_PRINTLN("[MQTT] skipping publish, not connected");
        } else {
            publish_sensor_data();
        }
    }
#endif // NATIVE
}

#ifdef NATIVE
int main() {
    setup();
    loop();
    return 0;
}
#endif
