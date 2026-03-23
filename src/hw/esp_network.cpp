#ifndef NATIVE

#include "hw/esp_network.h"
#include "config.h"
#include "credentials.h"

EspNetwork::EspNetwork() : mqtt_client_(wifi_client_) {
    mqtt_client_.setServer(MQTT_SERVER, MQTT_PORT);
}

bool EspNetwork::connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(250);
        attempts++;
    }
    return WiFi.status() == WL_CONNECTED;
}

bool EspNetwork::connect_mqtt() {
    if (!mqtt_client_.connected()) {
        bool connected;
#if defined(MQTT_USER) && defined(MQTT_PASSWORD)
        connected = mqtt_client_.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD);
#else
        connected = mqtt_client_.connect(DEVICE_ID);
#endif
        return connected;
    }
    return true;
}

bool EspNetwork::publish(const char* topic, const char* payload, bool retained) {
    return mqtt_client_.publish(topic, payload, retained);
}

bool EspNetwork::subscribe(const char* topic) {
    return mqtt_client_.subscribe(topic);
}

void EspNetwork::loop() {
    if (!mqtt_client_.connected()) {
        connect_mqtt();
    }
    mqtt_client_.loop();
}

void EspNetwork::disconnect() {
    mqtt_client_.disconnect();
    WiFi.disconnect(true);
}

void EspNetwork::set_callback(std::function<void(char*, uint8_t*, unsigned int)> cb) {
    mqtt_client_.setCallback(cb);
}

#endif
