#include "mqtt_payload.h"
#include "module_registry.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int format_sensor_payload(const SensorData& data, char* buf, size_t buf_size) {
    return snprintf(buf, buf_size,
        "{\"temp\":%.1f,\"humi\":%.1f,\"press\":%.1f}",
        data.temperature, data.humidity, data.pressure);
}

int format_sensor_payload_with_battery(const SensorData& data,
                                       uint8_t battery_pct, float battery_v,
                                       char* buf, size_t buf_size) {
    return snprintf(buf, buf_size,
        "{\"temp\":%.1f,\"humi\":%.1f,\"press\":%.1f,"
        "\"bat\":%u,\"batv\":%.2f}",
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
                                const char* hw_code,
                                uint16_t hw_rev,
                                const char* fw_version,
                                bool ota_capable,
                                bool calibration_present,
                                uint32_t publish_interval,
                                const ModuleRegistry& reg,
                                char* buf, size_t buf_size) {
    // Identity + metrics only (commands moved to format_commands_payload so
    // each message fits MQTT_MAX_PACKET_SIZE). Keys: id (chip serial),
    // hw (8-char code), hwrev, fw, ota (0/1), cal (0/1), intrvl,
    // metrics (name→unit).
    // "diag":1 is hardcoded: the diagnostics layer is always-on on every device
    // (no build flag). The server derives the diag commands (get_status, get_diag,
    // set_confirm_uplink) from this flag, so they are NOT listed in `commands`.
    int pos = snprintf(buf, buf_size,
        "{\"id\":\"%s\",\"hw\":\"%s\",\"hwrev\":%u,\"fw\":\"%s\","
        "\"ota\":%d,\"cal\":%d,\"diag\":1,\"intrvl\":%lu,\"metrics\":{",
        hardware_id, hw_code, (unsigned)hw_rev, fw_version,
        ota_capable ? 1 : 0, calibration_present ? 1 : 0,
        (unsigned long)publish_interval);
    if (pos < 0 || (size_t)pos >= buf_size) return pos;

    // metrics: {"name":"unit", ...} — empty string if no unit
    for (size_t i = 0; i < reg.num_metrics; i++) {
        const char* unit = reg.metric_units[i] ? reg.metric_units[i] : "";
        pos = buf_append(buf, buf_size, pos, "%s\"%s\":\"%s\"",
                         i > 0 ? "," : "", reg.metrics[i], unit);
    }
    pos = buf_append(buf, buf_size, pos, "}}");

    return pos;
}

int format_commands_payload(const ModuleRegistry& reg,
                            char* buf, size_t buf_size) {
    // {"commands":["a",..],"command_params":{"a":[{"n":..,"t":..}],..}}
    // Compact param keys (n/t) to stay within MQTT_MAX_PACKET_SIZE; only
    // commands with parameters appear in command_params.
    int pos = snprintf(buf, buf_size, "{\"commands\":[");
    if (pos < 0 || (size_t)pos >= buf_size) return pos;

    for (size_t i = 0; i < reg.num_commands; i++) {
        pos = buf_append(buf, buf_size, pos, "%s\"%s\"",
                         i > 0 ? "," : "", reg.commands[i]);
    }

    pos = buf_append(buf, buf_size, pos, "],\"command_params\":{");
    bool first = true;
    for (size_t i = 0; i < reg.num_commands; i++) {
        if (!reg.command_params[i] || reg.command_param_counts[i] == 0)
            continue;
        pos = buf_append(buf, buf_size, pos, "%s\"%s\":[",
                         first ? "" : ",", reg.commands[i]);
        first = false;
        for (size_t j = 0; j < reg.command_param_counts[i]; j++) {
            pos = buf_append(buf, buf_size, pos,
                "%s{\"n\":\"%s\",\"t\":\"%s\"}",
                j > 0 ? "," : "",
                reg.command_params[i][j].name,
                reg.command_params[i][j].type);
        }
        pos = buf_append(buf, buf_size, pos, "]");
    }
    pos = buf_append(buf, buf_size, pos, "}}");

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

bool parse_command_float_value(const char* json, float& out_value) {
    const char* key = "\"value\":";
    const char* pos = strstr(json, key);
    if (!pos) return false;

    pos += strlen(key);
    while (*pos == ' ' || *pos == '\t') pos++;

    if ((*pos < '0' || *pos > '9') && *pos != '.') return false;

    char* end = nullptr;
    float val = strtof(pos, &end);
    if (end == pos || val <= 0.0f) return false;

    out_value = val;
    return true;
}

bool parse_command_string_value(const char* json, const char* key,
                                char* out_buf, size_t out_buf_size) {
    if (!json || !key || !out_buf || out_buf_size == 0) return false;

    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* pos = strstr(json, search);
    if (!pos) return false;

    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != ':') return false;
    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != '"') return false;
    pos++;

    const char* end = strchr(pos, '"');
    if (!end) return false;

    size_t len = (size_t)(end - pos);
    // Reject (rather than silently truncate): a truncated value such as an OTA
    // URL would be worse than a clean failure.
    if (len >= out_buf_size) return false;

    memcpy(out_buf, pos, len);
    out_buf[len] = '\0';
    return true;
}
