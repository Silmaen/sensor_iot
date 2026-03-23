#include "mqtt_payload.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int format_sensor_payload(const SensorData& data, char* buf, size_t buf_size) {
    return snprintf(buf, buf_size,
        "{\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f}",
        data.temperature, data.humidity, data.pressure);
}

int format_sensor_payload_with_battery(const SensorData& data,
                                       uint8_t battery_pct, float battery_v,
                                       char* buf, size_t buf_size) {
    return snprintf(buf, buf_size,
        "{\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f,"
        "\"battery_pct\":%u,\"battery_v\":%.2f}",
        data.temperature, data.humidity, data.pressure,
        battery_pct, battery_v);
}

int format_status_payload(const char* level, const char* message,
                          char* buf, size_t buf_size) {
    if (message && message[0] != '\0') {
        return snprintf(buf, buf_size,
            "{\"level\":\"%s\",\"message\":\"%s\"}", level, message);
    }
    return snprintf(buf, buf_size, "{\"level\":\"%s\"}", level);
}

int format_capabilities_payload(const char* hardware_id,
                                uint32_t publish_interval,
                                const char* const* metrics, size_t num_metrics,
                                const char* const* commands, size_t num_commands,
                                char* buf, size_t buf_size) {
    int pos = snprintf(buf, buf_size,
        "{\"hardware_id\":\"%s\",\"publish_interval\":%lu,\"metrics\":[",
        hardware_id, (unsigned long)publish_interval);
    if (pos < 0 || (size_t)pos >= buf_size) return pos;

    for (size_t i = 0; i < num_metrics; i++) {
        int n = snprintf(buf + pos, buf_size - pos,
            "%s\"%s\"", i > 0 ? "," : "", metrics[i]);
        if (n < 0 || (size_t)(pos + n) >= buf_size) return pos + n;
        pos += n;
    }

    int n = snprintf(buf + pos, buf_size - pos, "],\"commands\":[");
    if (n < 0 || (size_t)(pos + n) >= buf_size) return pos + n;
    pos += n;

    for (size_t i = 0; i < num_commands; i++) {
        n = snprintf(buf + pos, buf_size - pos,
            "%s\"%s\"", i > 0 ? "," : "", commands[i]);
        if (n < 0 || (size_t)(pos + n) >= buf_size) return pos + n;
        pos += n;
    }

    n = snprintf(buf + pos, buf_size - pos, "]}");
    pos += n;
    return pos;
}

// Helper: extract a JSON string value for a given key.
// Looks for "key":"value" and copies value into out_buf.
static bool extract_json_string(const char* json, const char* key,
                                char* out_buf, size_t out_buf_size) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char* pos = strstr(json, search);
    if (!pos) return false;

    pos += strlen(search);
    const char* end = strchr(pos, '"');
    if (!end) return false;

    size_t len = end - pos;
    if (len >= out_buf_size) len = out_buf_size - 1;
    memcpy(out_buf, pos, len);
    out_buf[len] = '\0';
    return true;
}

bool parse_command_action(const char* json, char* action_buf, size_t action_buf_size) {
    return extract_json_string(json, "action", action_buf, action_buf_size);
}

bool parse_command_value(const char* json, uint32_t& out_value) {
    const char* key = "\"value\":";
    const char* pos = strstr(json, key);
    if (!pos) return false;

    pos += strlen(key);
    while (*pos == ' ' || *pos == '\t') pos++;

    if (*pos < '0' || *pos > '9') return false;

    unsigned long val = strtoul(pos, nullptr, 10);
    if (val == 0 || val > 86400) return false;

    out_value = static_cast<uint32_t>(val);
    return true;
}
