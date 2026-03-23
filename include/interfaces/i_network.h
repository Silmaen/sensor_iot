#pragma once

#include <stdint.h>

class INetwork {
public:
    virtual ~INetwork() = default;
    virtual bool connect_wifi() = 0;
    virtual bool connect_mqtt() = 0;
    virtual bool publish(const char* topic, const char* payload, bool retained = false) = 0;
    virtual bool subscribe(const char* topic) = 0;
    virtual void loop() = 0;
    virtual void disconnect() = 0;
};
