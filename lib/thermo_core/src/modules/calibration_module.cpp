#include "modules/calibration_module.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Default offsets — override per-device via -D flags in platformio.ini:
//   -DCALIBRATION_TEMP_OFFSET=-1.5f
//   -DCALIBRATION_HUMI_OFFSET=3.0f
//   -DCALIBRATION_PRESS_OFFSET=-0.3f
#ifndef CALIBRATION_TEMP_OFFSET
#define CALIBRATION_TEMP_OFFSET 0.0f
#endif
#ifndef CALIBRATION_HUMI_OFFSET
#define CALIBRATION_HUMI_OFFSET 0.0f
#endif
#ifndef CALIBRATION_PRESS_OFFSET
#define CALIBRATION_PRESS_OFFSET 0.0f
#endif

static const float default_temp_offset  = CALIBRATION_TEMP_OFFSET;
static const float default_humi_offset  = CALIBRATION_HUMI_OFFSET;
static const float default_press_offset = CALIBRATION_PRESS_OFFSET;

static float temp_offset  = CALIBRATION_TEMP_OFFSET;
static float humi_offset  = CALIBRATION_HUMI_OFFSET;
static float press_offset = CALIBRATION_PRESS_OFFSET;
static CalibrationPersistCallback persist_cb = nullptr;
static CalibrationResponseCallback response_cb = nullptr;

// --- Local JSON helpers ---

// Extract a JSON string value for a given key into out_buf.
static bool parse_string_field(const char* json, const char* key,
                               char* out_buf, size_t out_buf_size) {
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) return false;
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != '"') return false;
    pos++;
    const char* end = strchr(pos, '"');
    if (!end) return false;
    size_t len = end - pos;
    if (len >= out_buf_size) return false;
    memcpy(out_buf, pos, len);
    out_buf[len] = '\0';
    return true;
}

// Parse a float value that may be negative or zero.
static bool parse_signed_float_field(const char* json, const char* key, float& out) {
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) return false;
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    // Accept digits, minus sign, dot
    if ((*pos < '0' || *pos > '9') && *pos != '-' && *pos != '.') return false;
    char* end = nullptr;
    float val = strtof(pos, &end);
    if (end == pos) return false;
    out = val;
    return true;
}

// --- Command handler ---

// Command: {"action":"set_offset","metric":"temp","value":-0.5}
// Supported metrics: "temp", "humi", "press"
static bool handle_set_offset(const char* payload) {
    char metric[16];
    if (!parse_string_field(payload, "metric", metric, sizeof(metric)))
        return false;

    float value = 0.0f;
    if (!parse_signed_float_field(payload, "value", value))
        return false;

    // Sanity bounds: reject extreme offsets
    if (value < -50.0f || value > 50.0f)
        return false;

    if (strcmp(metric, "temp") == 0) {
        temp_offset = value;
    } else if (strcmp(metric, "humi") == 0) {
        humi_offset = value;
    } else if (strcmp(metric, "press") == 0) {
        press_offset = value;
    } else {
        return false;
    }

    if (persist_cb)
        persist_cb(temp_offset, humi_offset, press_offset);

    return true;
}

static bool handle_request_calibration(const char* /*payload*/) {
    if (response_cb)
        response_cb();
    return true;
}

static const CommandParamDef offset_params[] = {
    {"metric", "string"},
    {"value",  "number"},
};

// --- Module interface ---

void calibration_module_register(ModuleRegistry& reg) {
    reg.add_command("set_offset", handle_set_offset, offset_params, 2);
    reg.add_command("request_calibration", handle_request_calibration);
}

void calibration_apply(SensorData& data) {
    data.temperature += temp_offset;
    data.humidity    += humi_offset;
    data.pressure    += press_offset;
    // Clamp humidity to valid range
    if (data.humidity < 0.0f)   data.humidity = 0.0f;
    if (data.humidity > 100.0f) data.humidity = 100.0f;
}

float calibration_get_temp_offset()  { return temp_offset; }
float calibration_get_humi_offset()  { return humi_offset; }
float calibration_get_press_offset() { return press_offset; }

void calibration_module_set_offsets(float temp, float humi, float press) {
    temp_offset  = temp;
    humi_offset  = humi;
    press_offset = press;
}

int calibration_format_response(char* buf, size_t buf_size) {
    return snprintf(buf, buf_size,
        "{\"temp\":%.2f,\"humi\":%.2f,\"press\":%.2f}",
        temp_offset, humi_offset, press_offset);
}

void calibration_module_reset() {
    temp_offset  = default_temp_offset;
    humi_offset  = default_humi_offset;
    press_offset = default_press_offset;
    persist_cb   = nullptr;
    response_cb  = nullptr;
}

void calibration_module_set_persist_callback(CalibrationPersistCallback cb) {
    persist_cb = cb;
}

void calibration_module_set_response_callback(CalibrationResponseCallback cb) {
    response_cb = cb;
}
