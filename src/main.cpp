#include "config.h"
#include "credentials.h"
#include "debug.h"

// WIFI_CHANNEL is optional in credentials.h: 0 = auto (scan all channels). Set it
// to the AP's channel to join a HIDDEN SSID — the ESP32 scan can't find an AP that
// doesn't broadcast its name, so it needs the channel to associate directly.
#ifndef WIFI_CHANNEL
#define WIFI_CHANNEL 0
#endif
#include "hw_id.h"
#include "module_registry.h"
#include "mqtt_payload.h"
#include "payload_builder.h"
#include "diagnostics.h"
#include "platform_diag.h"

#ifdef HAS_BME280
#include "modules/bme280_module.h"
#endif

#ifdef HAS_SHT30
#include "modules/sht30_module.h"
#endif

#ifdef HAS_MKR_ENV
#include "modules/mkr_env_module.h"
#endif

#ifdef HAS_BATTERY
#include "modules/battery_module.h"
#endif

#ifdef HAS_BH1750
#include "modules/bh1750_module.h"
#endif

#ifdef HAS_CALIBRATION
#include "modules/calibration_module.h"
#endif

#ifndef NATIVE
#include <Arduino.h>

// --- Device diagnostics counters (persisted across a sleep cycle) ---
// Feed the `diag` topic (see diagnostics.h / docs/diagnostics.md), no longer the
// sensors payload. Now portable across ALL platforms via the DiagCounters struct
// (field meanings in diagnostics.h): ESP32 keeps it in RTC_DATA_ATTR, ESP8266 in
// RTC user memory (explicit save/restore), SAMD/native in plain RAM (SAMD standby
// resumes execution, so RAM survives). txsent/txok are only populated while the
// opt-in uplink-confirm mode is on (set_confirm_uplink; see docs/diagnostics.md).
// Storage persists across a sleep cycle, per platform:
//   ESP32    -> RTC_DATA_ATTR (retained through deep sleep automatically)
//   ESP8266  -> plain RAM, explicitly saved/restored to RTC user memory around
//               deep sleep (diag_counters_restore/persist, defined below)
//   SAMD     -> plain RAM (standby resumes execution — no reset, RAM survives)
//   native   -> plain RAM (single run)
#if defined(ESP32)
static RTC_DATA_ATTR DiagCounters g_dc = {};
#else
static DiagCounters g_dc = {};
#endif
static inline void     note_missed_wake()    { g_dc.miss++; }
static inline uint32_t missed_wakes()        { return g_dc.miss; }
static inline void     clear_missed_wakes()  { g_dc.miss = 0; }
static inline uint32_t next_publish_seq()    { return ++g_dc.seq; }
static inline uint32_t publish_seq()         { return g_dc.seq; }
static inline uint32_t pubfail_count()       { return g_dc.pubfail; }
static inline void     note_publish(bool ok) { if (ok) g_dc.pubfail = 0; else g_dc.pubfail++; }
static inline void     note_boot()           { g_dc.boot++; }
static inline uint32_t boot_count()          { return g_dc.boot; }
static inline bool     confirm_uplink_on()   { return g_dc.confirm != 0; }
static inline void     set_confirm_uplink(bool on) { g_dc.confirm = on ? 1 : 0; }
static inline void     note_tx_sent()        { g_dc.tx_sent++; }
static inline void     note_tx_confirmed()   { g_dc.tx_ok++; }
static inline uint32_t tx_sent_count()       { return g_dc.tx_sent; }
static inline uint32_t tx_ok_count()         { return g_dc.tx_ok; }
// Wake-path instrumentation: cumulative, never reset (see diagnostics.h). They
// pinpoint where a wake dies before it can publish — the gap between boot and
// delivered publishes on the C3.
static inline void     note_wifi_fail()      { g_dc.wifi_fail++; }
static inline void     note_mqtt_fail()      { g_dc.mqtt_fail++; }
static inline void     note_sensor_fail()    { g_dc.sensor_fail++; }
static inline void     note_boot_nds()       { g_dc.boot_nds++; }
static inline uint32_t wifi_fail_count()     { return g_dc.wifi_fail; }
static inline uint32_t mqtt_fail_count()     { return g_dc.mqtt_fail; }
static inline uint32_t sensor_fail_count()   { return g_dc.sensor_fail; }
static inline uint32_t boot_nds_count()      { return g_dc.boot_nds; }

// Set by on_mqtt_message() when a message arrives on our own sensors topic
// (broker loopback), i.e. the just-published sensors payload made it to the
// broker. Reset before each publish; read after the command-wait window.
static volatile bool g_uplink_confirmed = false;

// --- Sync-before-sleep (confirmed delivery) ---
// SYNC_BEFORE_SLEEP (build flag) forces the confirmed-publish path ON: the device
// subscribes to its own sensors topic and, before deep sleep, waits for the broker
// to loop the publish back (= delivery proof) and re-publishes if it doesn't,
// closing the QoS-0 "publish lost in the deep-sleep teardown" hole. Also toggleable
// at runtime via set_confirm_uplink. See docs/diagnostics.md.
#ifdef SYNC_BEFORE_SLEEP
static constexpr bool kSyncBeforeSleep = true;
#else
static constexpr bool kSyncBeforeSleep = false;
#endif
static inline bool sync_publish_on() { return kSyncBeforeSleep || confirm_uplink_on(); }
static constexpr uint32_t SYNC_CONFIRM_TIMEOUT_MS = 900; // wait per attempt for the loopback
static constexpr int      SYNC_MAX_RESENDS        = 4;   // re-publishes if unconfirmed

// --- Platform: network ---
#if defined(ARDUINO_SAMD_MKRWIFI1010)
#include "samd/nina_network.h"
static NinaNetwork network;
#elif defined(ESP32)
#include "esp32/esp32_network.h"
static Esp32Network network;
#else
#include "esp8266/esp_network.h"
static EspNetwork network;
#endif

// --- Drivers: sensors ---
#ifdef HAS_BME280
#include "bme280_sensor.h"
static Bme280Sensor sensor;
#endif

#ifdef HAS_SHT30
#include "sht30_sensor.h"
static Sht30Sensor sensor;
#endif

#ifdef HAS_MKR_ENV
#include "mkr_env_sensor.h"
static MkrEnvSensor sensor;
#endif

#ifdef HAS_DISPLAY
#include "display_encoding.h"
#include "shift_display.h"
static ShiftDisplay display;

#ifdef HAS_BME280
enum class DisplayMode : uint8_t { TEMPERATURE, HUMIDITY, PRESSURE, COUNT };
#else
enum class DisplayMode : uint8_t { TEMPERATURE, HUMIDITY, COUNT };
#endif
static DisplayMode current_mode         = DisplayMode::TEMPERATURE;
static unsigned long last_button_change = 0;
static bool last_button_state           = true;
#endif

#ifdef HAS_BH1750
#include "bh1750_sensor.h"
static Bh1750Sensor light_sensor;
#endif

// --- Platform: battery ADC ---
#ifdef HAS_BATTERY
#include "battery.h"
#include "battery_adc.h"
#endif

// --- Platform: sleep ---
#ifdef HAS_DEEP_SLEEP
#if defined(ESP32)
#include "esp32/esp32_sleep.h"
static Esp32Sleep sleeper;
#else
#include "esp8266/esp_sleep.h"
static EspSleep sleeper;
#endif

// Diag-counter persistence around deep sleep. ESP32 keeps g_dc in RTC_DATA_ATTR
// (nothing to do). ESP8266 must copy g_dc to/from RTC user memory explicitly.
#if defined(ESP8266) && !defined(NATIVE)
static inline void diag_counters_restore() { sleeper.read_diag_counters(g_dc); }
static inline void diag_counters_persist() { sleeper.write_diag_counters(g_dc); }
#else
static inline void diag_counters_restore() {}
static inline void diag_counters_persist() {}
#endif

// Persist the diag counters, then deep sleep. Use everywhere instead of calling
// sleeper.deep_sleep() directly, so no counter update is lost across a wake.
static inline void enter_deep_sleep(uint32_t seconds) {
    diag_counters_persist();
    sleeper.deep_sleep(seconds);
}
#endif

#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(HAS_DISPLAY)
#include "samd/samd_sleep.h"
static SamdSleep samd_sleeper;
#endif

// --- Platform: runtime config store (device_id + calibration) ---
#if defined(ESP8266)
#include "esp8266/littlefs_config_store.h"
static LittleFsConfigStore config_store;
#elif defined(ESP32)
#include "esp32/nvs_config_store.h"
static NvsConfigStore config_store;
#elif defined(ARDUINO_SAMD_MKRWIFI1010)
#include "samd/flash_config_store.h"
static FlashConfigStore config_store;
#endif

// --- OTA module ---
#ifdef HAS_OTA
#include "modules/ota_module.h"
#if defined(ESP32)
#include "esp32/esp32_ota.h"
#else
#include "esp8266/esp_ota.h"
#endif
#endif

#endif // NATIVE

#ifdef NATIVE
#include "memory_config_store.h"
static MemoryConfigStore config_store;
#endif

#include "config_store.h"
#include "device_topics.h"

// --- Shared state ---
static ModuleRegistry registry;
static uint32_t publish_interval_s = DEFAULT_PUBLISH_INTERVAL_S;

#ifndef NATIVE
// device_id is provisioned at runtime (Stage B); MQTT topics are built from it.
static char g_device_id[64] = {};
static DeviceTopics topics;
#endif

#if (defined(HAS_BME280) || defined(HAS_SHT30) || defined(HAS_MKR_ENV)) && !defined(NATIVE)
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

// --- Command handler: set_confirm_uplink ---
// Opt-in uplink-delivery diagnostic. When on (value!=0), the device subscribes to
// its own sensors topic; a broker loopback of the just-published payload confirms
// the publish reached the broker (txok/txsent, reported every wake via diag).
// Off by default; RTC-backed so it survives deep sleep. See docs/diagnostics.md.
static bool handle_set_confirm_uplink(const char* payload) {
    uint32_t value = 0;
    if (!parse_command_value(payload, value))
        return false;
    DEBUG_PRINTF("[CMD] set_confirm_uplink: %lu\n", (unsigned long)value);
    set_confirm_uplink(value != 0);
    return true;
}

// Plausible range for the battery divider ratio. A value outside this — from a
// corrupted store, or a bad calibrate/set_calibration (incl. a garbage value
// laundered through the server mirror) — is rejected so it can never wreck the
// reading. Guards BOTH the boot restore and the runtime set paths.
static constexpr float BAT_DIVIDER_MIN = 0.5f;
static constexpr float BAT_DIVIDER_MAX = 6.0f;

#if defined(HAS_BATTERY) && !defined(NATIVE)
// Apply + persist the battery divider ratio (config store survives power loss,
// unlike RTC). Rejects an implausible ratio. Returns true if applied.
static bool persist_bat_divider(float new_ratio) {
    if (new_ratio < BAT_DIVIDER_MIN || new_ratio > BAT_DIVIDER_MAX) {
        DEBUG_PRINTF("[BAT] rejecting implausible ratio=%.4f\n", new_ratio);
        return false;
    }
    battery_calibrate_set_ratio(new_ratio);
    config_store.set_float(config_keys::bat_divider, new_ratio);
    config_store.commit();
#if defined(HAS_CALIBRATION)
    // Keep the calibration report (calibration_format_response) in sync with the
    // real ratio, so the server mirror gets the actual value, not a stale 0.
    calibration_module_set_bat_divider(new_ratio);
#endif
    return true;
}
static void on_battery_calibrated(float new_ratio) {
    persist_bat_divider(new_ratio);
}
#endif

#if defined(HAS_CALIBRATION) && !defined(NATIVE)
// Persist sensor offsets to the config store after a set_offset command.
static void on_calibration_persist(float temp, float humi, float press) {
    config_store.set_float(config_keys::cal_temp, temp);
    config_store.set_float(config_keys::cal_humi, humi);
    config_store.set_float(config_keys::cal_press, press);
    config_store.commit();
}
// Apply + persist a generic calibration value (set_calibration). Returns false
// for an unsupported key so the command is nacked.
static bool on_calibration_value(const char* key, float value) {
    if (strcmp(key, config_keys::bat_divider) == 0) {
#if defined(HAS_BATTERY)
        return persist_bat_divider(value);  // nacks an implausible ratio
#else
        return false;
#endif
    }
    return false;
}
// Publish the current calibration on the calibration topic (request_calibration).
static void on_calibration_requested() {
    char buf[96];
    calibration_format_response(buf, sizeof(buf));
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.calibration, buf);
    network.publish(topics.calibration, buf);
}
#endif

#if defined(HAS_OTA) && !defined(NATIVE)
// Publish an OTA ack: {"action":"ota_update","status":..,"message":..}.
static void on_ota_ack(const char* status, const char* message) {
    char buf[96];
    if (message && message[0] != '\0')
        snprintf(buf, sizeof(buf),
                 "{\"action\":\"ota_update\",\"status\":\"%s\",\"message\":\"%s\"}",
                 status, message);
    else
        snprintf(buf, sizeof(buf),
                 "{\"action\":\"ota_update\",\"status\":\"%s\"}", status);
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.ack, buf);
    network.publish(topics.ack, buf);
}
#if defined(HAS_BATTERY)
// Battery percentage for the OTA low-battery guard.
static int on_ota_battery() {
    return (int)voltage_to_soc(adc_to_voltage(read_battery_adc()));
}
#endif
#endif

static const CommandParamDef set_interval_params[] = {
    {"value", "number"},
};

// --- Module registration ---
static void register_modules() {
    registry.init();
    registry.add_command("set_interval", handle_set_interval,
                         set_interval_params, 1);
    // Note: get_status/get_diag/set_confirm_uplink are NOT registered here — they
    // are core diagnostics commands, inferred server-side from the "diag":1
    // capability flag and dispatched by strcmp in process_pending_commands().
    // Likewise ota_update is inferred from "ota":1. Keeps the `commands` message
    // small (MQTT_MAX_PACKET_SIZE) and MAX_COMMANDS headroom for real modules.
    registry.add_metric("rssi", "dBm");
#ifdef HAS_BME280
    bme280_module_register(registry);
#endif
#ifdef HAS_SHT30
    sht30_module_register(registry);
#endif
#ifdef HAS_MKR_ENV
    mkr_env_module_register(registry);
#endif
#ifdef HAS_BATTERY
    battery_module_register(registry);
#endif
#ifdef HAS_BH1750
    bh1750_module_register(registry);
#endif
#ifdef HAS_CALIBRATION
    calibration_module_register(registry);
#endif
#ifdef HAS_OTA
    ota_module_register(registry);
#endif
}

#ifndef NATIVE

// --- Serial provisioning (Stage B) ---
// When the store holds no device_id, the device stays in this mode (no MQTT)
// until an operator provisions it over serial. See docs §3.1.
static void reboot_device() {
#if defined(ESP8266) || defined(ESP32)
    ESP.restart();
#elif defined(ARDUINO_SAMD_MKRWIFI1010)
    NVIC_SystemReset();
#endif
}

static void handle_provision_line(const char* line) {
    if (strncmp(line, "provision ", 10) == 0) {
        const char* id = line + 10;
        while (*id == ' ') id++;
        if (device_id_valid(id)) {
            config_store.set_str(config_keys::device_id, id);
            config_store.commit();
            Serial.print("[PROV] provisioned device_id=");
            Serial.print(id);
            Serial.println(", rebooting");
            delay(200);
            reboot_device();
        } else {
            Serial.println("[PROV] invalid device_id (allowed [A-Za-z0-9_-], 1-63 chars)");
        }
    } else if (strcmp(line, "factory_reset") == 0) {
        config_store.clear();
        Serial.println("[PROV] store cleared, rebooting");
        delay(200);
        reboot_device();
    } else if (line[0] != '\0') {
        Serial.println("[PROV] commands: provision <id> | factory_reset");
    }
}

static void run_provisioning() {
    Serial.println("[PROV] no device_id provisioned.");
    Serial.println("[PROV] send: provision <id>   (or: factory_reset)");
    char line[96];
    size_t len = 0;
    for (;;) {
        while (Serial.available()) {
            char c = (char)Serial.read();
            // Accept CR, LF or CRLF as line terminator (terminal-agnostic);
            // a blank line (e.g. the LF of a CRLF) is ignored.
            if (c == '\r' || c == '\n') {
                if (len > 0) {
                    line[len] = '\0';
                    handle_provision_line(line);
                    len = 0;
                }
            } else if (len < sizeof(line) - 1) {
                line[len++] = c;
            }
        }
        delay(10);
    }
}

// --- Build and publish sensor payload ---
static void publish_sensor_data() {
    char buf[256];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
#if defined(HAS_CALIBRATION) && (defined(HAS_BME280) || defined(HAS_SHT30) || defined(HAS_MKR_ENV))
    calibration_apply(last_data);
#endif
#ifdef HAS_BME280
    bme280_module_contribute(pb, last_data);
#endif
#ifdef HAS_SHT30
    sht30_module_contribute(pb, last_data);
#endif
#ifdef HAS_MKR_ENV
    mkr_env_module_contribute(pb, last_data);
#endif
#ifdef HAS_BH1750
    {
        float lux = light_sensor.read_lux();
        if (lux >= 0.0f) {
            DEBUG_PRINTF("[SENSOR] BH1750 lux=%.1f\n", lux);
            bh1750_module_contribute(pb, lux);
        } else {
            DEBUG_PRINTLN("[SENSOR] BH1750 read failed");
        }
    }
#endif
#ifdef HAS_BATTERY
    uint16_t adc_raw = read_battery_adc();
    float voltage    = adc_to_voltage(adc_raw);
    uint8_t soc      = voltage_to_soc(voltage);
    DEBUG_PRINTF("[BAT] adc=%u ratio=%.3f v=%.2f (ADC_MAX=%d REF=%.2f)\n",
                 adc_raw, battery_get_divider_ratio(), voltage,
                 (int)ADC_MAX_VALUE, (float)ADC_REF_VOLTAGE);
    battery_module_contribute(pb, soc, voltage);
#endif
    pb.add_int("rssi", network.wifi_rssi());
    pb.end();
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.sensors, buf);
    g_uplink_confirmed = false; // reset before send; set by the loopback callback
    const bool pub_ok = network.publish(topics.sensors, buf);
    next_publish_seq();   // advance publish sequence (reported via the diag topic)
    note_publish(pub_ok); // track send failures for the diag topic
    if (sync_publish_on()) note_tx_sent(); // count attempts under sync/confirm mode
    DEBUG_PRINTF("[MQTT] publish -> %s\n", pub_ok ? "ok" : "FAILED");

#ifdef HAS_BATTERY
    if (soc <= BATTERY_CRITICAL_THRESHOLD) {
        char status_buf[64];
        format_status_payload("error", "critical_battery", status_buf, sizeof(status_buf));
        DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.status, status_buf);
        network.publish(topics.status, status_buf);
    } else if (soc <= BATTERY_WARN_THRESHOLD) {
        char status_buf[64];
        format_status_payload("warning", "low_battery", status_buf, sizeof(status_buf));
        DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.status, status_buf);
        network.publish(topics.status, status_buf);
    }
#endif
}

// --- Diagnostics: health summary (status topic) + technical snapshot (diag topic) ---
static void build_diag_data(DiagData& d) {
    d.reset_cause = platform_reset_cause();
    d.boot    = boot_count();
    d.miss    = missed_wakes();
    d.wake_ms = millis();
    d.seq     = publish_seq();
    d.pubfail = pubfail_count();
    d.tx_sent = tx_sent_count();
    d.tx_ok   = tx_ok_count();
    d.wifi_fail   = wifi_fail_count();
    d.mqtt_fail   = mqtt_fail_count();
    d.sensor_fail = sensor_fail_count();
    d.boot_nds    = boot_nds_count();
    d.rssi    = network.wifi_connected() ? network.wifi_rssi() : 0;
    d.heap    = platform_free_heap();
#ifdef HAS_BATTERY
    const uint8_t soc = voltage_to_soc(adc_to_voltage(read_battery_adc()));
    d.has_battery  = true;
    d.bat_soc      = soc;
    d.bat_critical = soc <= BATTERY_CRITICAL_THRESHOLD;
    d.bat_low      = soc <= BATTERY_WARN_THRESHOLD;
#endif
}

// Full technical snapshot on the diag topic (get_diag response, or auto on warning+).
static void publish_diag(const DiagData& d) {
    char hw_id[20];
    get_hw_id(hw_id, sizeof(hw_id));
    char buf[320];  // headroom for the wake-path instrumentation fields (wf/mf/sf/bx)
    format_diag_payload(d, hw_id, HW_CODE, HW_REV, FIRMWARE_VERSION, buf, sizeof(buf));
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.diag, buf);
    network.publish(topics.diag, buf);
}

// Health summary {level,message} on the status topic (get_status response).
static void publish_health_status(const DiagData& d) {
    const char* msg = "ok";
    const HealthLevel level = diag_evaluate(d, &msg);
    char buf[96];
    format_status_payload(health_level_str(level), msg, buf, sizeof(buf));
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.status, buf);
    network.publish(topics.status, buf);
}

// Called each wake after the sensor publish: publish diag when health >= warning,
// or on every wake while the uplink-confirm diagnostic is on (so the txok/txsent
// counters are observed even on nominal wakes — see docs/diagnostics.md).
static void maybe_publish_diag() {
    DiagData d;
    build_diag_data(d);
    const char* msg = "ok";
    if (confirm_uplink_on() || diag_evaluate(d, &msg) >= HEALTH_WARNING) {
        DEBUG_PRINTF("[DIAG] %s -> diag\n", confirm_uplink_on() ? "confirm-mode" : msg);
        publish_diag(d);
    }
}

// --- Publish capabilities from registry ---
static void publish_capabilities() {
    char hw_id[20];
    get_hw_id(hw_id, sizeof(hw_id));

#ifdef HAS_OTA
    constexpr bool ota_capable = true;
#else
    constexpr bool ota_capable = false;
#endif

    char buf[512];
    format_capabilities_payload(hw_id,
                                HW_CODE,
                                HW_REV,
                                FIRMWARE_VERSION,
                                ota_capable,
                                config_calibration_present(config_store),
                                publish_interval_s,
                                registry,
                                buf,
                                sizeof(buf));
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.capabilities, buf);
    network.publish(topics.capabilities, buf);
}

// --- Publish command list from registry (response to request_commands) ---
static void publish_commands() {
    char buf[512];
    format_commands_payload(registry, buf, sizeof(buf));
    DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.commands, buf);
    network.publish(topics.commands, buf);
}

// --- Incoming command queue ---
// The PubSubClient message callback runs *inside* mqtt_client_.loop() and reuses
// the client's TX buffer. Publishing from it (ack, or capabilities with its 768B
// stack buffer nested deep in the call chain) is re-entrant and stack-heavy and
// has wedged deep-sleep nodes. So the callback only *copies* each payload into
// this queue; process_pending_commands() runs the handlers — and their
// publishes — from the main loop, outside the callback.
static constexpr uint8_t CMD_QUEUE_SIZE = 4;
static constexpr size_t CMD_PAYLOAD_MAX = 128;
static char cmd_queue[CMD_QUEUE_SIZE][CMD_PAYLOAD_MAX];
static uint8_t cmd_queue_count = 0;

// --- MQTT message callback (enqueue only — never publishes) ---
static void on_mqtt_message(char* topic, uint8_t* payload, unsigned int length) {
    // Uplink confirmation: a message on our own sensors topic is the broker
    // looping back the payload we just published — proof it reached the broker
    // (TCP is ordered, so the loopback implies the PUBLISH was received). Not a
    // command; consume it here without enqueuing.
    if (topic != nullptr && strcmp(topic, topics.sensors) == 0) {
        g_uplink_confirmed = true;
        return;
    }
    if (cmd_queue_count >= CMD_QUEUE_SIZE) {
        DEBUG_PRINTLN("[MQTT] << queue full, dropping command");
        return;
    }
    size_t const n = length < CMD_PAYLOAD_MAX - 1 ? length : CMD_PAYLOAD_MAX - 1;
    memcpy(cmd_queue[cmd_queue_count], payload, n);
    cmd_queue[cmd_queue_count][n] = '\0';
    DEBUG_PRINTF("[MQTT] << queued (%u): %s\n",
                 static_cast<unsigned>(cmd_queue_count + 1), cmd_queue[cmd_queue_count]);
    cmd_queue_count++;
}

// --- Process queued commands (called from the main loop, NOT the callback) ---
// Safe to publish here: we are outside mqtt_client_.loop(), so the client buffer
// and stack are ours. Single-threaded — the callback only appends during
// network.loop(), which we do not call from within this function.
static void process_pending_commands() {
    for (uint8_t i = 0; i < cmd_queue_count; i++) {
        const char* buf = cmd_queue[i];

        char action[32];
        if (!parse_command_action(buf, action, sizeof(action))) {
            DEBUG_PRINTF("[MQTT] parse_command_action failed for: %s\n", buf);
            continue;
        }
        DEBUG_PRINTF("[MQTT] command: action=%s\n", action);

        if (strcmp(action, "request_capabilities") == 0) {
            DEBUG_PRINTLN("[MQTT] publishing capabilities (requested)");
            publish_capabilities();
            continue;
        }

        if (strcmp(action, "request_commands") == 0) {
            DEBUG_PRINTLN("[MQTT] publishing commands (requested)");
            publish_commands();
            continue;
        }

        // get_status: quick health summary on the status topic. get_diag: full
        // technical snapshot on the diag topic. (Both built-in, see docs/diagnostics.md.)
        if (strcmp(action, "get_status") == 0) {
            DiagData d; build_diag_data(d);
            publish_health_status(d);
            continue;
        }
        if (strcmp(action, "get_diag") == 0) {
            DiagData d; build_diag_data(d);
            publish_diag(d);
            continue;
        }

        // set_confirm_uplink: toggle the uplink-delivery diagnostic. Core command,
        // inferred server-side from the "diag":1 flag (not in the command list).
        if (strcmp(action, "set_confirm_uplink") == 0) {
            bool const ok = handle_set_confirm_uplink(buf);
            char ack_buf[96];
            format_ack_payload(action, ok ? "ok" : "error", ack_buf, sizeof(ack_buf));
            DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.ack, ack_buf);
            network.publish(topics.ack, ack_buf);
            continue;
        }

#ifdef HAS_OTA
        // ota_update: inferred server-side from the "ota":1 flag (not in the
        // command list). The OTA module emits its own detailed ack.
        if (strcmp(action, "ota_update") == 0) {
            ota_module_handle(buf);
            continue;
        }
#endif

        // Remote factory reset: wipe the store (device_id + calibration) and
        // reboot into serial provisioning. Ack first (the reboot cuts the link),
        // since after this the device is unprovisioned and needs `provision`.
        if (strcmp(action, "factory_reset") == 0) {
            DEBUG_PRINTLN("[MQTT] factory_reset: clearing store and rebooting");
            network.publish(topics.ack, "{\"action\":\"factory_reset\",\"status\":\"ok\"}");
            delay(200);
            config_store.clear();
            reboot_device();
        }

        bool const ok = registry.dispatch(action, buf);
        if (!ok) {
            DEBUG_PRINTF("[MQTT] unknown command: %s\n", action);
        }

        // Acknowledge the command
        char ack_buf[96];
        format_ack_payload(action, ok ? "ok" : "error", ack_buf, sizeof(ack_buf));
        DEBUG_PRINTF("[MQTT] publish %s: %s\n", topics.ack, ack_buf);
        network.publish(topics.ack, ack_buf);
    }
    cmd_queue_count = 0;
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
#ifdef HAS_BME280
    case DisplayMode::PRESSURE:
        encoded = encode_pressure(last_data.pressure);
        break;
#endif
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
#ifdef HAS_DEEP_SLEEP
    diag_counters_restore();  // ESP8266: load counters from RTC user memory (no-op on ESP32)
#endif
    note_boot();  // count this boot (diag)
#ifndef NATIVE
    // Instrumentation: a deep-sleep timer wake has reset cause 4. Any other cause
    // here is the cold start, a brownout, or a spurious double-boot — counting it
    // exposes whether the boot>>publish gap comes from extra (non-wake) boots.
    if (platform_reset_cause() != 4 /* deep-sleep timer */) note_boot_nds();
#endif

#ifndef NATIVE
    Serial.begin(115200);
#if defined(ARDUINO_SAMD_MKRWIFI1010) && defined(HAS_SERIAL_DEBUG)
    // Wait up to 3s for USB CDC serial to be ready (non-blocking if no monitor)
    unsigned long serial_start = millis();
    while (!Serial && millis() - serial_start < 3000) {
        delay(10);
    }
#endif
    Serial.println("Thermo starting...");
    DEBUG_PRINTF("[INIT] type=%s\n", MQTT_DEVICE_TYPE);
    DEBUG_PRINTF("[INIT] publish_interval=%lus\n", (unsigned long)publish_interval_s);

    // --- Runtime config store: identity + calibration (survives OTA/power loss) ---
    if (!config_store.begin())
        DEBUG_PRINTLN("[CFG] store begin failed");
#ifdef DEVICE_ID
    // Dev/seed (D12): seed device_id from the build flag when the store is empty.
    if (!config_store.has(config_keys::device_id)) {
        config_store.set_str(config_keys::device_id, DEVICE_ID);
        config_store.commit();
    }
#endif

#if defined(FORCE_DEVICE_ID) && defined(DEVICE_ID)
    // Recovery build: use the compile-time DEVICE_ID directly, bypassing the config
    // store entirely. For units whose NVS store is unusable (e.g. after a full
    // esptool erase_flash left it unwritable) so seed + serial provisioning both
    // fail — this guarantees a valid device_id and reaches MQTT regardless of NVS.
    strncpy(g_device_id, DEVICE_ID, sizeof(g_device_id) - 1);
    g_device_id[sizeof(g_device_id) - 1] = '\0';
    DEBUG_PRINTF("[INIT] FORCED device_id=%s (store bypassed)\n", g_device_id);
#else
    // Resolve the runtime device_id. Without a valid one, enter serial
    // provisioning and never reach MQTT (topics can't be built).
    if (!config_store.get_str(config_keys::device_id, g_device_id, sizeof(g_device_id)) ||
        !device_id_valid(g_device_id)) {
        run_provisioning(); // blocks; reboots once provisioned
        return;             // unreachable
    }
    DEBUG_PRINTF("[INIT] device_id=%s\n", g_device_id);
#endif
    topics.build(MQTT_DEVICE_TYPE, g_device_id);

#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(HAS_DISPLAY)
    samd_sleeper.begin();
    DEBUG_PRINTLN("[INIT] SAMD standby sleep enabled");
#endif

#ifdef HAS_DEEP_SLEEP
    if (!sleeper.read_rtc_interval(publish_interval_s)) {
        publish_interval_s = DEFAULT_SLEEP_INTERVAL_S;
    }
#endif

#if defined(HAS_CALIBRATION)
    {
        // Restore offsets from the store (default 0 when absent).
        float t = 0.0f, h = 0.0f, p = 0.0f;
        config_store.get_float(config_keys::cal_temp, t);
        config_store.get_float(config_keys::cal_humi, h);
        config_store.get_float(config_keys::cal_press, p);
        calibration_module_set_offsets(t, h, p);
        calibration_module_set_response_callback(on_calibration_requested);
        calibration_module_set_persist_callback(on_calibration_persist);
        calibration_module_set_value_callback(on_calibration_value);
    }
#endif

#if defined(HAS_BATTERY)
    battery_module_set_adc_reader(read_battery_adc);
#if defined(PIN_BATTERY_SWITCH)
    battery_adc_begin();
#endif
    {
        // Restore the divider ratio from the store, but only if it is within a
        // plausible range (BAT_DIVIDER_MIN..MAX) — a corrupted/stale store (e.g.
        // SAMD FlashStorage not erased on flash) must not apply a garbage ratio.
        // Out of range => keep the compiled nominal default.
        float ratio = 0.0f;
        if (config_store.get_float(config_keys::bat_divider, ratio)) {
            if (ratio >= BAT_DIVIDER_MIN && ratio <= BAT_DIVIDER_MAX) {
                battery_calibrate_set_ratio(ratio);
                calibration_module_set_bat_divider(ratio);
                DEBUG_PRINTF("[INIT] restored battery ratio=%.4f from store\n", ratio);
            } else {
                DEBUG_PRINTF("[INIT] ignoring implausible stored battery ratio=%.4f "
                             "(using default)\n", ratio);
            }
        }
    }
    battery_module_set_calibration_callback(on_battery_calibrated);
#endif

#ifdef HAS_OTA
    ota_module_set_identity(HW_CODE, HW_REV, FIRMWARE_VERSION);
    ota_module_set_performer(platform_ota_perform);
    ota_module_set_ack(on_ota_ack);
#if defined(HAS_BATTERY)
    ota_module_set_battery_provider(on_ota_battery);
#endif
#endif

#ifdef HAS_BME280
    if (!sensor.begin()) {
        Serial.println("BME280 init failed!");
#ifdef HAS_DEEP_SLEEP
        enter_deep_sleep(publish_interval_s);
        return;
#endif
    }
#endif

#ifdef HAS_SHT30
    if (!sensor.begin()) {
        Serial.println("SHT30 init failed!");
#ifdef HAS_DEEP_SLEEP
        enter_deep_sleep(publish_interval_s);
        return;
#endif
    }
#endif

#ifdef HAS_MKR_ENV
    if (!sensor.begin()) {
        Serial.println("MKR ENV init failed!");
#ifdef HAS_DEEP_SLEEP
        enter_deep_sleep(publish_interval_s);
        return;
#endif
    }
#endif

#ifdef HAS_BH1750
    if (!light_sensor.begin()) {
        Serial.println("BH1750 init failed!");
    }
#endif

#ifdef HAS_DISPLAY
    display.begin();
    pinMode(PIN_BUTTON, INPUT_PULLUP);
#endif

    network.configure({
        WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL,
        MQTT_SERVER, MQTT_PORT,
#if defined(MQTT_USER) && defined(MQTT_PASSWORD)
        MQTT_USER, MQTT_PASSWORD,
#else
        nullptr, nullptr,
#endif
        g_device_id  // MQTT client id = runtime device_id
    });
    network.set_callback(on_mqtt_message);

#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(HAS_DISPLAY) && !defined(HAS_DEEP_SLEEP)
    // Duty-cycle mode: WiFi will be brought up on first publish in loop()
    DEBUG_PRINTLN("[NET] duty-cycle mode, WiFi deferred to first publish");
#else
    DEBUG_PRINTLN("[NET] connecting WiFi...");
    if (!network.connect_wifi()) {
        Serial.println("WiFi connection failed!");
        note_wifi_fail();  // instrumentation: wake died at WiFi connect
#ifdef HAS_DEEP_SLEEP
        note_missed_wake();
        enter_deep_sleep(publish_interval_s);
        return;
#else
        DEBUG_PRINTLN("[NET] will retry in loop");
#endif
    } else {
        DEBUG_PRINTLN("[NET] WiFi connected");
        DEBUG_PRINTLN("[NET] connecting MQTT...");
        if (!network.connect_mqtt()) {
            Serial.println("MQTT connection failed!");
            note_mqtt_fail();  // instrumentation: WiFi up but MQTT connect failed
#ifdef HAS_DEEP_SLEEP
            note_missed_wake();
            enter_deep_sleep(publish_interval_s);
            return;
#else
            DEBUG_PRINTLN("[NET] will retry in loop");
#endif
        } else {
            DEBUG_PRINTLN("[NET] MQTT connected");
            network.subscribe(topics.command);
            DEBUG_PRINTF("[MQTT] subscribed to %s\n", topics.command);
            if (sync_publish_on()) {
                network.subscribe(topics.sensors); // broker loopback = delivery proof
                DEBUG_PRINTF("[MQTT] subscribed to %s (sync/confirm)\n", topics.sensors);
            }
        }
    }
#endif

#ifdef HAS_DEEP_SLEEP
    // One-shot mode: publish sensors (triggers server command flush),
    // wait for commands, then sleep. Capabilities are only sent when
    // the server requests them (request_capabilities command).

#if defined(HAS_BME280) || defined(HAS_SHT30) || defined(HAS_MKR_ENV)
    last_data = sensor.read();
    if (last_data.valid) {
        publish_sensor_data();
    } else {
        note_sensor_fail();  // instrumentation: wake reached network but sensor read invalid
    }
#else
    publish_sensor_data();
#endif

    // Report a technical snapshot on the diag topic only if something is wrong
    // (brownout, missed wakes, weak signal, low battery/heap); then reset the miss
    // counter now that it has been reported.
    maybe_publish_diag();
    clear_missed_wakes();

    // Wait for server to flush pending commands (may be multiple).
    // If request_capabilities arrives, it is handled in on_mqtt_message.
    // The sensors-topic loopback (uplink confirm) also arrives in this window.
    unsigned long start = millis();
    while (millis() - start < MQTT_COMMAND_WAIT_MS) {
        network.loop();
        process_pending_commands(); // handle/ack outside the MQTT callback
        delay(10);
    }

    // Sync before sleep: don't deep-sleep on an undelivered QoS-0 frame. The
    // command-wait loop above already gave the first publish up to
    // MQTT_COMMAND_WAIT_MS for the broker loopback to confirm delivery. If still
    // unconfirmed, re-publish and wait, up to SYNC_MAX_RESENDS — closing the
    // "publish lost in the deep-sleep teardown" hole. (Loopback received => the
    // broker got our publish, since TCP is ordered.)
    if (sync_publish_on()) {
        int resends = 0;
        while (!g_uplink_confirmed && resends < SYNC_MAX_RESENDS) {
            resends++;
            DEBUG_PRINTF("[SYNC] unconfirmed, resend %d/%d\n", resends, SYNC_MAX_RESENDS);
            publish_sensor_data();  // resets g_uplink_confirmed, re-sends, advances seq
            unsigned long t0 = millis();
            while (!g_uplink_confirmed && millis() - t0 < SYNC_CONFIRM_TIMEOUT_MS) {
                network.loop();
                delay(5);
            }
        }
        if (g_uplink_confirmed) note_tx_confirmed();
        DEBUG_PRINTF("[SYNC] %s (%d resend%s)\n",
                     g_uplink_confirmed ? "confirmed" : "GAVE UP",
                     resends, resends == 1 ? "" : "s");
    }

    network.disconnect();
    enter_deep_sleep(publish_interval_s);
#endif // HAS_DEEP_SLEEP

#endif // NATIVE
}

// Initialized so that (millis() - last_sensor_read >= interval) is true on
// the very first loop() iteration, triggering an immediate first publish.
static unsigned long last_sensor_read = -static_cast<unsigned long>(DEFAULT_PUBLISH_INTERVAL_S) * 1000UL;

// --- Read all sensors, return true if data is valid ---
#if (defined(HAS_BME280) || defined(HAS_SHT30) || defined(HAS_MKR_ENV)) && !defined(NATIVE)
static bool read_sensors() {
    last_data = sensor.read();
    if (!last_data.valid) {
#ifdef HAS_BME280
        DEBUG_PRINTLN("[SENSOR] BME280 read failed");
#elif defined(HAS_SHT30)
        DEBUG_PRINTLN("[SENSOR] SHT30 read failed");
#elif defined(HAS_MKR_ENV)
        DEBUG_PRINTLN("[SENSOR] MKR ENV read failed");
#endif
        return false;
    }
#ifdef HAS_BME280
    DEBUG_PRINTF("[SENSOR] T=%.1f H=%.1f P=%.1f\n", last_data.temperature, last_data.humidity, last_data.pressure);
#ifdef HAS_DISPLAY
    update_display();
#endif
#elif defined(HAS_SHT30)
    DEBUG_PRINTF("[SENSOR] T=%.1f H=%.1f\n", last_data.temperature, last_data.humidity);
#elif defined(HAS_MKR_ENV)
    DEBUG_PRINTF("[SENSOR] T=%.1f H=%.1f P=%.1f L=%.0f UV=%.2f\n", last_data.temperature, last_data.humidity, last_data.pressure, last_data.light_lux, last_data.uv_index);
#endif
    return true;
}
#endif

// --- Connect, publish, disconnect (duty-cycle) ---
#ifndef NATIVE
static void duty_cycle_publish() {
    DEBUG_PRINTLN("[NET] connecting WiFi...");
    if (!network.connect_wifi()) {
        DEBUG_PRINTLN("[NET] WiFi failed, skipping cycle");
        note_wifi_fail();    // instrumentation: wake died at WiFi connect
        note_missed_wake();  // connect failure (diag)
        return;
    }
    if (!network.connect_mqtt()) {
        DEBUG_PRINTLN("[NET] MQTT failed, skipping cycle");
        note_mqtt_fail();    // instrumentation: WiFi up but MQTT connect failed
        note_missed_wake();  // connect failure (diag)
        network.power_down();
        return;
    }
    network.subscribe(topics.command);
    if (confirm_uplink_on()) {
        network.subscribe(topics.sensors); // broker loopback = uplink proof
        DEBUG_PRINTF("[MQTT] subscribed to %s (uplink confirm)\n", topics.sensors);
    }

    // Publish sensors first — triggers server-side command flush
    publish_sensor_data();

    // Report a diag snapshot when health >= warning (or every cycle in confirm mode),
    // then reset the miss counter now that it has been reported.
    maybe_publish_diag();
    clear_missed_wakes();

    // Wait for server to flush pending commands (may be multiple). The sensors-topic
    // loopback (uplink confirm) also arrives in this window.
    unsigned long cmd_start = millis();
    while (millis() - cmd_start < MQTT_COMMAND_WAIT_MS) {
        network.loop();
        process_pending_commands(); // handle/ack outside the MQTT callback
        delay(10);
    }

    if (confirm_uplink_on() && g_uplink_confirmed) note_tx_confirmed();

    network.power_down();
    DEBUG_PRINTLN("[NET] radio powered down");
}
#endif

void loop() {
#ifdef HAS_DEEP_SLEEP
    // Deep sleep resets to setup(), loop() should never run
    return;
#endif

#ifndef NATIVE

#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(HAS_DISPLAY)
    // --- SAMD standby duty-cycle mode ---
    // Read sensors, publish over WiFi, then enter SAMD21 standby (~5µA)
    // until the next cycle. Standby resumes execution here — no reboot.

#if defined(HAS_BME280) || defined(HAS_SHT30) || defined(HAS_MKR_ENV)
    if (!read_sensors()) {
        note_sensor_fail();  // instrumentation: cycle skipped on invalid sensor read
        samd_sleeper.standby(publish_interval_s);
        return;
    }
#endif
    duty_cycle_publish();
    samd_sleeper.standby(publish_interval_s);

#else
    // --- Continuous mode (ESP8266, or any config with display) ---
    // WiFi stays connected for responsive display + low-latency commands.
    network.loop();
    process_pending_commands(); // handle/ack outside the MQTT callback

#ifdef HAS_DISPLAY
    handle_button();
#endif

    if (unsigned long const now = millis();
        now - last_sensor_read >= static_cast<unsigned long>(publish_interval_s) * 1000UL) {
        last_sensor_read = now;

#if defined(HAS_BME280) || defined(HAS_SHT30) || defined(HAS_MKR_ENV)
        if (!read_sensors()) return;
#endif
        if (!network.mqtt_connected()) {
            DEBUG_PRINTLN("[MQTT] skipping publish, not connected");
        } else {
            publish_sensor_data();
        }
    }
#endif // SAMD duty-cycle vs continuous

#endif // NATIVE
}

#ifdef NATIVE
int main() {
    setup();
    loop();
    return 0;
}
#endif
