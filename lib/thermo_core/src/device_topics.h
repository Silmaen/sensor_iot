#pragma once

#include <stddef.h>

// Runtime-built MQTT topics. device_id is provisioned at runtime (not compiled),
// so topics can no longer be compile-time macros. device_type stays a build
// constant (per firmware type). See docs/ota-calibration-protocol.md §2, Stage B.
struct DeviceTopics {
    // {device_type}/{device_id}/{suffix}: type <=~16, id <=63, suffix <=13.
    static constexpr size_t TOPIC_LEN = 112;

    char sensors[TOPIC_LEN];
    char status[TOPIC_LEN];
    char command[TOPIC_LEN];
    char capabilities[TOPIC_LEN];
    char ack[TOPIC_LEN];
    char commands[TOPIC_LEN];
    char calibration[TOPIC_LEN];

    // Build all topics from the device type and the runtime device_id.
    void build(const char* device_type, const char* device_id);
};

// A valid device_id matches ^[a-zA-Z0-9_-]+$ and fits the store (<= 63 chars),
// mirroring the server's SAFE_IDENTIFIER_RE.
bool device_id_valid(const char* device_id);
