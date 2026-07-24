#pragma once

#include <stdint.h>
#include <stddef.h>
#include "sensor_data.h"

// Format sensor data as JSON payload (Config A)
// Returns number of chars written (excluding null terminator)
int format_sensor_payload(const SensorData& data, char* buf, size_t buf_size);

// Format sensor data + battery info as JSON payload (Config B)
int format_sensor_payload_with_battery(const SensorData& data,
                                       uint8_t battery_pct, float battery_v,
                                       char* buf, size_t buf_size);

// Format status alert as JSON payload
// level: "ok", "warning", or "error". message: optional detail (may be nullptr).
int format_status_payload(const char* level, const char* message,
                          char* buf, size_t buf_size);

struct ModuleRegistry;

// Format the capabilities response (identity + metrics only — the command list
// lives in a separate `commands` message, see format_commands_payload). Kept
// small to fit MQTT_MAX_PACKET_SIZE (512 B).
//   hardware_id:          unique per-chip serial (e.g. "ESP-A1B2C3")
//   hw_code:              fixed 8-char hardware code
//   hw_rev:               hardware revision
//   fw_version:           firmware version (semver)
//   ota_capable:          true if the device can perform OTA updates (-> "ota":1)
//   calibration_present:  true if the calibration store holds any value (-> "cal":1)
// Always emits "diag":1 — diagnostics is always-on; the server infers the diag
// commands (get_status/get_diag/set_confirm_uplink) from this flag, not `commands`.
// Output: {"id","hw","hwrev","fw","ota","cal","diag","intrvl","metrics":{name:unit,...}}
int format_capabilities_payload(const char* hardware_id,
                                const char* hw_code,
                                uint16_t hw_rev,
                                const char* fw_version,
                                bool ota_capable,
                                bool calibration_present,
                                uint32_t publish_interval,
                                const ModuleRegistry& registry,
                                char* buf, size_t buf_size);

// Format the command list as a standalone message (response to
// request_commands). Output: {"commands":[..],"command_params":{name:[{n,t}],..}}
// Only commands that declare parameters appear in command_params.
int format_commands_payload(const ModuleRegistry& registry,
                            char* buf, size_t buf_size);

// Format command acknowledgement as JSON payload.
// action: the command action being acknowledged. status: "ok" or "error".
int format_ack_payload(const char* action, const char* status,
                       char* buf, size_t buf_size);

// Parse the "action" field from a command payload.
// Copies the action string into action_buf. Returns true if found.
bool parse_command_action(const char* json, char* action_buf, size_t action_buf_size);

// Parse the numeric "value" field from a command payload.
// Returns true if found and valid.
bool parse_command_value(const char* json, uint32_t& out_value);

// Parse the numeric "value" field as a float from a command payload.
// Returns true if found and valid (positive, finite).
bool parse_command_float_value(const char* json, float& out_value);

// Parse an arbitrary string field for a given key from a JSON payload.
// Copies the value into out_buf (null-terminated, truncated if too long).
// Returns true if the key is found with a string value.
bool parse_command_string_value(const char* json, const char* key,
                                char* out_buf, size_t out_buf_size);
