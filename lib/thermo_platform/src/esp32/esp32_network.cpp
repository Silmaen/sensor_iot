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

static volatile int s_last_wifi_reason = 0;   // raw esp-idf disconnect reason (diagnostic)

static bool wait_wl_connected() {
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(250);
        attempts++;
    }
    return WiFi.status() == WL_CONNECTED;
}

bool Esp32Network::connect_wifi() {
    WiFi.mode(WIFI_STA);
    // Disable WiFi modem-sleep. By default the ESP32 dozes between beacons, which
    // delays/drops our brief QoS-0 publish right before deep sleep -> the server saw
    // publishes lost in bursts (seq gaps) despite a strong RSSI and a clean connect.
    // Negligible battery cost here: the radio is only up for the ~4 s active window
    // (busy TX/RX anyway), then deep sleep turns it fully off. (ESP8266/MKR unaffected.)
    WiFi.setSleep(false);
    // Lower the minimum auth mode: the network is WPA_PSK (WPA1), which the ESP32
    // default (WPA2) filters out as "no AP found". The ESP8266 core has no such gate,
    // which is why those nodes connect out of the box but the C3 did not.
    WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        s_last_wifi_reason = info.wifi_sta_disconnected.reason;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // Fresh scan every wake, connect to the STRONGEST matching AP — no BSSID cache /
    // pinning. The RTC BSSID cache was removed: pinning stuck the node to one AP whose
    // backhaul intermittently dropped our QoS-0 publishes despite a strong RSSI (server
    // saw ~1 publish in 2-5 wakes, unique to this node). See docs/diagnostics.md. The
    // channel also lets the scan find the HIDDEN SSID (no SSID in its beacons).
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    DEBUG_PRINTF("[WIFI] connecting to SSID \"%s\" (channel %u)...\n",
                 cfg_.wifi_ssid, cfg_.wifi_channel);
    WiFi.begin(cfg_.wifi_ssid, cfg_.wifi_password, cfg_.wifi_channel);
    if (wait_wl_connected()) {
        DEBUG_PRINTF("[WIFI] connected, IP=%s RSSI=%ddBm ch %u\n",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.channel());
        return true;
    }
    DEBUG_PRINTF("[WIFI] failed, status=%d reason=%d\n", WiFi.status(), s_last_wifi_reason);
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
            // Disable Nagle: our single small QoS-0 publish must go out immediately,
            // not sit coalescing in the TCP buffer until the connection is torn down.
            wifi_client_.setNoDelay(true);
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
    // Deep-sleep teardown race: WiFi.disconnect(true) powers the radio off at once,
    // which can cut the just-published QoS-0 packet mid-flight (the server saw ~1/3 of
    // this node's publishes lost in transit). Drain TX and give lwip time to send it
    // out, then tear down gracefully.
    wifi_client_.flush();
    delay(40);
    mqtt_client_.disconnect();
    delay(60);
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
