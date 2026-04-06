#pragma once

#ifndef NATIVE

#include "interfaces/i_network.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <functional>

class EspNetwork : public INetwork {
public:
    EspNetwork();
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

    void set_callback(std::function<void(char*, uint8_t*, unsigned int)> cb);

private:
    WiFiClient wifi_client_;
    PubSubClient mqtt_client_;
    unsigned long last_reconnect_attempt_ = 0;
    bool was_mqtt_connected_ = false;
};

#endif
