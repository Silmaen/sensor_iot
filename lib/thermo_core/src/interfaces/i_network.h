#pragma once

#include <stdint.h>

struct NetworkConfig {
    const char* wifi_ssid;
    const char* wifi_password;
    uint8_t     wifi_channel;   // 0 = auto (scan all); set to AP channel for a hidden SSID
    const char* mqtt_server;
    uint16_t    mqtt_port;
    const char* mqtt_user;      // nullptr = no auth
    const char* mqtt_password;  // nullptr = no auth
    const char* device_id;      // MQTT client ID
};

class INetwork {
public:
    virtual ~INetwork() = default;
    virtual void configure(const NetworkConfig& cfg) = 0;
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
    virtual void power_down() { disconnect(); }
};
