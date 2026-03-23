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

// Format capabilities response as JSON payload
// metrics and commands are null-terminated string arrays.
int format_capabilities_payload(const char* hardware_id,
                                uint32_t publish_interval,
                                const char* const* metrics, size_t num_metrics,
                                const char* const* commands, size_t num_commands,
                                char* buf, size_t buf_size);

// Parse the "action" field from a command payload.
// Copies the action string into action_buf. Returns true if found.
bool parse_command_action(const char* json, char* action_buf, size_t action_buf_size);

// Parse the numeric "value" field from a command payload.
// Returns true if found and valid.
bool parse_command_value(const char* json, uint32_t& out_value);
