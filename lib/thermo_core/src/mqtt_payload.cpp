#include "mqtt_payload.h"
#include "module_registry.h"
#include <stdarg.h>
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

// Helper: append to buffer, returns new pos or -1 on overflow
static int buf_append(char* buf, size_t buf_size, int pos, const char* fmt, ...) {
    if (pos < 0 || (size_t)pos >= buf_size) return pos;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + pos, buf_size - pos, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)(pos + n) >= buf_size) return -1;
    return pos + n;
}

int format_capabilities_payload(const char* hardware_id,
                                uint32_t publish_interval,
                                const ModuleRegistry& reg,
                                char* buf, size_t buf_size) {
    int pos = snprintf(buf, buf_size,
        "{\"hardware_id\":\"%s\",\"publish_interval\":%lu,\"metrics\":[",
        hardware_id, (unsigned long)publish_interval);
    if (pos < 0 || (size_t)pos >= buf_size) return pos;

    for (size_t i = 0; i < reg.num_metrics; i++) {
        pos = buf_append(buf, buf_size, pos, "%s\"%s\"",
                         i > 0 ? "," : "", reg.metrics[i]);
    }

    // units (only if at least one metric has a unit)
    bool has_units = false;
    for (size_t i = 0; i < reg.num_metrics; i++) {
        if (reg.metric_units[i]) { has_units = true; break; }
    }
    if (has_units) {
        pos = buf_append(buf, buf_size, pos, "],\"units\":{");
        bool first = true;
        for (size_t i = 0; i < reg.num_metrics; i++) {
            if (!reg.metric_units[i]) continue;
            pos = buf_append(buf, buf_size, pos, "%s\"%s\":\"%s\"",
                             first ? "" : ",", reg.metrics[i], reg.metric_units[i]);
            first = false;
        }
        pos = buf_append(buf, buf_size, pos, "},\"commands\":[");
    } else {
        pos = buf_append(buf, buf_size, pos, "],\"commands\":[");
    }

    for (size_t i = 0; i < reg.num_commands; i++) {
        pos = buf_append(buf, buf_size, pos, "%s\"%s\"",
                         i > 0 ? "," : "", reg.commands[i]);
    }

    // command_params (only if at least one command has params)
    bool has_params = false;
    for (size_t i = 0; i < reg.num_commands; i++) {
        if (reg.command_params[i] && reg.command_param_counts[i] > 0) {
            has_params = true; break;
        }
    }
    if (has_params) {
        pos = buf_append(buf, buf_size, pos, "],\"command_params\":{");
        bool first_cmd = true;
        for (size_t i = 0; i < reg.num_commands; i++) {
            if (!reg.command_params[i] || reg.command_param_counts[i] == 0) continue;
            pos = buf_append(buf, buf_size, pos, "%s\"%s\":[",
                             first_cmd ? "" : ",", reg.commands[i]);
            first_cmd = false;
            for (size_t j = 0; j < reg.command_param_counts[i]; j++) {
                pos = buf_append(buf, buf_size, pos,
                    "%s{\"name\":\"%s\",\"type\":\"%s\"}",
                    j > 0 ? "," : "",
                    reg.command_params[i][j].name,
                    reg.command_params[i][j].type);
            }
            pos = buf_append(buf, buf_size, pos, "]");
        }
        pos = buf_append(buf, buf_size, pos, "}}");
    } else {
        pos = buf_append(buf, buf_size, pos, "]}");
    }

    return pos;
}

int format_ack_payload(const char* action, const char* status,
                       char* buf, size_t buf_size) {
    return snprintf(buf, buf_size,
        "{\"action\":\"%s\",\"status\":\"%s\"}", action, status);
}

// Helper: extract a JSON string value for a given key.
// Looks for "key":"value" and copies value into out_buf.
static bool extract_json_string(const char* json, const char* key,
                                char* out_buf, size_t out_buf_size) {
    // Find "key"
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* pos = strstr(json, search);
    if (!pos) return false;

    pos += strlen(search);
    // Skip whitespace around ':'
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != ':') return false;
    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;
    // Expect opening quote
    if (*pos != '"') return false;
    pos++;

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
