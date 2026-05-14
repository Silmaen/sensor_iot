#if defined(ESP8266) && !defined(NATIVE)

#include "esp8266/esp_network.h"
#include "config.h"
#include "debug.h"

#ifdef HAS_SERIAL_DEBUG
static const char* wifi_status_str(wl_status_t status) {
    switch (status) {
    case WL_IDLE_STATUS:      return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
    case WL_WRONG_PASSWORD:  return "WRONG_PASSWORD";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
    }
}

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

void EspNetwork::configure(const NetworkConfig& cfg) {
    cfg_ = cfg;
    mqtt_client_.setServer(cfg.mqtt_server, cfg.mqtt_port);
}

bool EspNetwork::connect_wifi() {
    WiFi.mode(WIFI_STA);
    DEBUG_PRINTF("[WIFI] connecting to SSID \"%s\"...\n", cfg_.wifi_ssid);
    WiFi.begin(cfg_.wifi_ssid, cfg_.wifi_password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(250);
        attempts++;
    }
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
        DEBUG_PRINTF("[WIFI] connected, IP=%s RSSI=%ddBm\n",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        DEBUG_PRINTF("[WIFI] failed after %d attempts, status=%s (%d)\n",
                     attempts, wifi_status_str(st), st);
    }
    return st == WL_CONNECTED;
}

bool EspNetwork::connect_mqtt() {
    if (!mqtt_client_.connected()) {
        DEBUG_PRINTF("[MQTT] connecting to %s:%d as \"%s\"...\n",
                     cfg_.mqtt_server, cfg_.mqtt_port, cfg_.device_id);
        bool connected;
        if (cfg_.mqtt_user && cfg_.mqtt_password) {
            connected = mqtt_client_.connect(cfg_.device_id, cfg_.mqtt_user, cfg_.mqtt_password);
        } else {
            connected = mqtt_client_.connect(cfg_.device_id);
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

bool EspNetwork::wifi_connected() {
    return WiFi.status() == WL_CONNECTED;
}

bool EspNetwork::mqtt_connected() {
    return mqtt_client_.connected();
}

int32_t EspNetwork::wifi_rssi() {
    return WiFi.RSSI();
}

const char* EspNetwork::wifi_ip() {
    static char ip_buf[16];
    WiFi.localIP().toString().toCharArray(ip_buf, sizeof(ip_buf));
    return ip_buf;
}

bool EspNetwork::publish(const char* topic, const char* payload, bool retained) {
    return mqtt_client_.publish(topic, payload, retained);
}

bool EspNetwork::subscribe(const char* topic) {
    bool ok = mqtt_client_.subscribe(topic);
    DEBUG_PRINTF("[MQTT] subscribe(%s) => %s\n", topic, ok ? "OK" : "FAILED");
    return ok;
}

void EspNetwork::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        if (was_mqtt_connected_) {
            was_mqtt_connected_ = false;
            DEBUG_PRINTF("[WIFI] lost, status=%s (%d)\n",
                         wifi_status_str(WiFi.status()), WiFi.status());
        }
        unsigned long now = millis();
        if (now - last_reconnect_attempt_ >= RECONNECT_INTERVAL_MS) {
            last_reconnect_attempt_ = now;
            DEBUG_PRINTF("[WIFI] reconnecting (status=%s)...\n",
                         wifi_status_str(WiFi.status()));
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
            if (connect_mqtt()) {
                subscribe(MQTT_TOPIC_COMMAND);
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

void EspNetwork::disconnect() {
    mqtt_client_.disconnect();
    WiFi.disconnect(true);
    was_mqtt_connected_ = false;
}

void EspNetwork::set_callback(std::function<void(char*, uint8_t*, unsigned int)> cb) {
    mqtt_client_.setCallback(cb);
}

#endif
