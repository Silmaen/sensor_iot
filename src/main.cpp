#include "config.h"
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
#include "hw/esp_network.h"

#include <Arduino.h>

static EspNetwork network;

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
#include "hw/battery_adc.h"
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
    publish_interval_s = value;
#if defined(HAS_DEEP_SLEEP) && !defined(NATIVE)
    sleeper.write_rtc_interval(value);
#endif
    return true;
}

// --- Module registration ---
static void register_modules() {
    registry.init();
    registry.add_command("set_interval", handle_set_interval);
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
    pb.end();
    network.publish(MQTT_TOPIC_SENSORS, buf);

#ifdef HAS_BATTERY
    if (soc <= BATTERY_LOW_THRESHOLD) {
        char status_buf[64];
        format_status_payload("warning", "low_battery", status_buf, sizeof(status_buf));
        network.publish(MQTT_TOPIC_STATUS, status_buf);
    }
#endif
}

// --- Publish capabilities from registry ---
static void publish_capabilities() {
    char hw_id[20];
    snprintf(hw_id, sizeof(hw_id), "ESP-%06X", static_cast<unsigned int>(ESP.getChipId()));

    char buf[320];
    format_capabilities_payload(hw_id,
                                publish_interval_s,
                                registry.metrics,
                                registry.num_metrics,
                                registry.commands,
                                registry.num_commands,
                                buf,
                                sizeof(buf));
    network.publish(MQTT_TOPIC_CAPABILITIES, buf);
}

// --- MQTT message callback ---
static void on_mqtt_message(char* topic, uint8_t* payload, unsigned int length) {
    (void)topic;
    char buf[128];
    unsigned int copy_len = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
    memcpy(buf, payload, copy_len);
    buf[copy_len] = '\0';

    char action[32];
    if (!parse_command_action(buf, action, sizeof(action)))
        return;

    if (strcmp(action, "request_capabilities") == 0) {
        publish_capabilities();
        return;
    }
    registry.dispatch(action, buf);
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

    if (!network.connect_wifi()) {
        Serial.println("WiFi connection failed!");
#ifdef HAS_DEEP_SLEEP
        sleeper.deep_sleep(publish_interval_s);
        return;
#endif
    }
    if (!network.connect_mqtt()) {
        Serial.println("MQTT connection failed!");
#ifdef HAS_DEEP_SLEEP
        sleeper.deep_sleep(publish_interval_s);
        return;
#endif
    }

    network.set_callback(on_mqtt_message);
    network.subscribe(MQTT_TOPIC_COMMAND);

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
            return;
        }
#ifdef HAS_DISPLAY
        update_display();
#endif
#endif
        publish_sensor_data();
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
