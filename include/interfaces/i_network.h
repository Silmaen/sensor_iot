#pragma once

#include <stdint.h>

class INetwork {
public:
    virtual ~INetwork() = default;
    virtual bool connect_wifi() = 0;
    virtual bool connect_mqtt() = 0;
    virtual bool wifi_connected() = 0;
    virtual bool mqtt_connected() = 0;
    virtual int32_t wifi_rssi() = 0;
    virtual const char* wifi_ip() = 0;
    virtual bool publish(const char* topic, const char* payload, bool retained = false) = 0;
    virtual bool subscribe(const char* topic) = 0;
    virtual void loop() = 0;
    virtual void disconnect() = 0;
};
