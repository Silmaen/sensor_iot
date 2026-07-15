#if defined(ESP32) && !defined(NATIVE)

#include "esp32/esp32_network.h"
#include "config.h"
#include "debug.h"

#include <string.h>

#ifdef HAS_SERIAL_DEBUG
static const char* mqtt_state_str(int state) {
    switch (state) {
    case -4: return "CONNECTION_TIMEOUT";
    case -3: return "CONNECTION_LOST";
    case -2: return "CONNECT_FAILED";
    case -1: return "DISCONNECTED";
    case  0: return "CONNECTED";
    case  1: return "CONNECT_BAD_PROTOCOL";
    case  2: return "CONNECT_BAD_CLIENT_ID";
    case  3: return "CONNECT_UNAVAILABLE";
    case  4: return "CONNECT_BAD_CREDENTIALS";
    case  5: return "CONNECT_UNAUTHORIZED";
    default: return "UNKNOWN";
    }
}
#endif

void Esp32Network::configure(const NetworkConfig& cfg) {
    cfg_ = cfg;
    mqtt_client_.setServer(cfg.mqtt_server, cfg.mqtt_port);
}

bool Esp32Network::connect_wifi() {
    WiFi.mode(WIFI_STA);
    // The default ESP32 minimum auth mode is WPA2-PSK, which silently filters out
    // WPA-PSK (WPA1) APs during association and reports them as "no AP found". Our
    // IoT network is WPA_PSK, so lower the bar (the ESP8266 core has no such gate,
    // which is why those nodes connect but the ESP32 did not).
    WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
    DEBUG_PRINTF("[WIFI] connecting to SSID \"%s\" (channel %u)...\n",
                 cfg_.wifi_ssid, cfg_.wifi_channel);
    // Passing a non-zero channel lets the ESP32 associate directly on that channel
    // instead of scanning for the SSID first — required for HIDDEN SSIDs, which do
    // not appear in a scan. channel 0 keeps the default scan-based behaviour.
    WiFi.begin(cfg_.wifi_ssid, cfg_.wifi_password, cfg_.wifi_channel);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(250);
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTF("[WIFI] connected, IP=%s RSSI=%ddBm\n",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    }
    DEBUG_PRINTF("[WIFI] failed after %d attempts, status=%d\n",
                 attempts, WiFi.status());
    return false;
}

bool Esp32Network::connect_mqtt() {
    if (!mqtt_client_.connected()) {
        DEBUG_PRINTF("[MQTT] connecting to %s:%d as \"%s\"...\n",
                     cfg_.mqtt_server, cfg_.mqtt_port, cfg_.device_id);
        bool connected;
        // cleanSession=false: the broker keeps our subscription and queues
        // QoS-1 commands while we deep-sleep, delivering them on reconnect.
        // No LWT (per protocol): willTopic=nullptr, willQos=0, willRetain=false.
        if (cfg_.mqtt_user && cfg_.mqtt_password) {
            connected = mqtt_client_.connect(cfg_.device_id, cfg_.mqtt_user, cfg_.mqtt_password,
                                             nullptr, 0, false, nullptr, false);
        } else {
            connected = mqtt_client_.connect(cfg_.device_id, nullptr, nullptr,
                                             nullptr, 0, false, nullptr, false);
        }
        if (connected) {
            DEBUG_PRINTLN("[MQTT] connected");
        } else {
            DEBUG_PRINTF("[MQTT] failed, state=%s (%d)\n",
                         mqtt_state_str(mqtt_client_.state()), mqtt_client_.state());
        }
        was_mqtt_connected_ = connected;
        return connected;
    }
    return true;
}

bool Esp32Network::wifi_connected() {
    return WiFi.status() == WL_CONNECTED;
}

bool Esp32Network::mqtt_connected() {
    return mqtt_client_.connected();
}

int32_t Esp32Network::wifi_rssi() {
    return WiFi.RSSI();
}

const char* Esp32Network::wifi_ip() {
    static char ip_buf[16];
    WiFi.localIP().toString().toCharArray(ip_buf, sizeof(ip_buf));
    return ip_buf;
}

bool Esp32Network::publish(const char* topic, const char* payload, bool retained) {
    return mqtt_client_.publish(topic, payload, retained);
}

bool Esp32Network::subscribe(const char* topic) {
    strncpy(subscribe_topic_, topic, sizeof(subscribe_topic_) - 1);
    subscribe_topic_[sizeof(subscribe_topic_) - 1] = '\0';
    bool ok = mqtt_client_.subscribe(topic, 1); // QoS 1: broker queues while asleep
    DEBUG_PRINTF("[MQTT] subscribe(%s) => %s\n", topic, ok ? "OK" : "FAILED");
    return ok;
}

void Esp32Network::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        if (was_mqtt_connected_) {
            was_mqtt_connected_ = false;
            DEBUG_PRINTF("[WIFI] lost, status=%d\n", WiFi.status());
        }
        unsigned long now = millis();
        if (now - last_reconnect_attempt_ >= RECONNECT_INTERVAL_MS) {
            last_reconnect_attempt_ = now;
            DEBUG_PRINTLN("[WIFI] reconnecting...");
            WiFi.reconnect();
        }
        return;
    }

    if (!mqtt_client_.connected()) {
        if (was_mqtt_connected_) {
            was_mqtt_connected_ = false;
            DEBUG_PRINTF("[MQTT] connection lost, state=%s (%d)\n",
                         mqtt_state_str(mqtt_client_.state()), mqtt_client_.state());
        }
        unsigned long now = millis();
        if (now - last_reconnect_attempt_ >= RECONNECT_INTERVAL_MS) {
            last_reconnect_attempt_ = now;
            if (connect_mqtt() && subscribe_topic_[0] != '\0') {
                subscribe(subscribe_topic_);
            }
        }
        return;
    }

    if (!was_mqtt_connected_) {
        was_mqtt_connected_ = true;
        DEBUG_PRINTF("[NET] fully connected, IP=%s RSSI=%ddBm\n",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    mqtt_client_.loop();
}

void Esp32Network::disconnect() {
    mqtt_client_.disconnect();
    WiFi.disconnect(true);
    was_mqtt_connected_ = false;
}

void Esp32Network::power_down() {
    disconnect();
    WiFi.mode(WIFI_OFF);
}

void Esp32Network::set_callback(void (*cb)(char*, uint8_t*, unsigned int)) {
    mqtt_client_.setCallback(cb);
}

#endif
