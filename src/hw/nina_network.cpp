#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)

#include "hw/nina_network.h"
#include "config.h"
#include "credentials.h"
#include "debug.h"

#ifdef HAS_SERIAL_DEBUG
static const char* wifi_status_str(uint8_t status) {
    switch (status) {
    case WL_IDLE_STATUS:      return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
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

NinaNetwork::NinaNetwork() : mqtt_client_(wifi_client_) {
    mqtt_client_.setServer(MQTT_SERVER, MQTT_PORT);
}

bool NinaNetwork::connect_wifi() {
    DEBUG_PRINTF("[WIFI] connecting to SSID \"%s\"...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(250);
        attempts++;
    }
    uint8_t st = WiFi.status();
    if (st == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        DEBUG_PRINTF("[WIFI] connected, IP=%d.%d.%d.%d RSSI=%lddBm\n",
                     ip[0], ip[1], ip[2], ip[3], (long)WiFi.RSSI());
    } else {
        DEBUG_PRINTF("[WIFI] failed after %d attempts, status=%s (%d)\n",
                     attempts, wifi_status_str(st), st);
    }
    return st == WL_CONNECTED;
}

bool NinaNetwork::connect_mqtt() {
    if (!mqtt_client_.connected()) {
        DEBUG_PRINTF("[MQTT] connecting to %s:%d as \"%s\"...\n", MQTT_SERVER, MQTT_PORT, DEVICE_ID);
        bool connected;
#if defined(MQTT_USER) && defined(MQTT_PASSWORD)
        connected = mqtt_client_.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD);
#else
        connected = mqtt_client_.connect(DEVICE_ID);
#endif
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

bool NinaNetwork::wifi_connected() {
    return WiFi.status() == WL_CONNECTED;
}

bool NinaNetwork::mqtt_connected() {
    return mqtt_client_.connected();
}

int32_t NinaNetwork::wifi_rssi() {
    return WiFi.RSSI();
}

const char* NinaNetwork::wifi_ip() {
    static char ip_buf[16];
    IPAddress ip = WiFi.localIP();
    snprintf(ip_buf, sizeof(ip_buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    return ip_buf;
}

bool NinaNetwork::publish(const char* topic, const char* payload, bool retained) {
    return mqtt_client_.publish(topic, payload, retained);
}

bool NinaNetwork::subscribe(const char* topic) {
    bool ok = mqtt_client_.subscribe(topic);
    DEBUG_PRINTF("[MQTT] subscribe(%s) => %s\n", topic, ok ? "OK" : "FAILED");
    return ok;
}

void NinaNetwork::loop() {
    // WiFi lost — reconnect
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
            // WiFiNINA has no reconnect() — call begin() again
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        return;
    }

    // WiFi OK but MQTT lost — non-blocking reconnect
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

    // Both connected
    if (!was_mqtt_connected_) {
        was_mqtt_connected_ = true;
        IPAddress ip = WiFi.localIP();
        DEBUG_PRINTF("[NET] fully connected, IP=%d.%d.%d.%d RSSI=%lddBm\n",
                     ip[0], ip[1], ip[2], ip[3], (long)WiFi.RSSI());
    }
    mqtt_client_.loop();
}

void NinaNetwork::disconnect() {
    mqtt_client_.disconnect();
    WiFi.disconnect();
    was_mqtt_connected_ = false;
}

void NinaNetwork::power_down() {
    disconnect();
    WiFi.end();  // Powers down the NINA-W102 radio module
}

void NinaNetwork::set_callback(MqttCallback cb) {
    mqtt_client_.setCallback(cb);
}

#endif
