#pragma once

#if defined(ESP32) && !defined(NATIVE)

#include "interfaces/i_network.h"
#include <PubSubClient.h>
#include <WiFi.h>

class Esp32Network : public INetwork {
public:
    Esp32Network();
    bool connect_wifi() override;
    bool connect_mqtt() override;
    bool wifi_connected() override;
    bool mqtt_connected() override;
    int32_t wifi_rssi() override;
    const char* wifi_ip() override;
    bool publish(const char* topic, const char* payload, bool retained = false) override;
    bool subscribe(const char* topic) override;
    void loop() override;
    void disconnect() override;
    void power_down() override;

    void set_callback(void (*cb)(char*, uint8_t*, unsigned int));

private:
    WiFiClient wifi_client_;
    PubSubClient mqtt_client_;
    unsigned long last_reconnect_attempt_ = 0;
    bool was_mqtt_connected_ = false;
};

#endif
