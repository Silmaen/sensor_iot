#pragma once

#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)

#include "interfaces/i_network.h"
#include <PubSubClient.h>
#include <WiFiNINA.h>
using MqttCallback = void (*)(char*, uint8_t*, unsigned int);

class NinaNetwork : public INetwork {
public:
    void configure(const NetworkConfig& cfg) override;
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

    void set_callback(MqttCallback cb);

private:
    WiFiClient wifi_client_;
    PubSubClient mqtt_client_{wifi_client_};
    NetworkConfig cfg_ = {};
    unsigned long last_reconnect_attempt_ = 0;
    bool was_mqtt_connected_ = false;
    char subscribe_topic_[112] = {}; // remembered for auto-resubscribe on reconnect
};

#endif
