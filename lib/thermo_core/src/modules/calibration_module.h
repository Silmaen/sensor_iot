#pragma once

#include "module_registry.h"
#include "sensor_data.h"

#include <stdint.h>

// Register the set_offset, set_calibration and request_calibration commands.
// No metrics — this module only adjusts readings from other sensor modules.
void calibration_module_register(ModuleRegistry& reg);

// Apply stored offsets to sensor data in-place. Call this between the
// sensor read and the module contribute step.
void calibration_apply(SensorData& data);

// Get current offsets (for persistence / reporting)
float calibration_get_temp_offset();
float calibration_get_humi_offset();
float calibration_get_press_offset();

// Restore offsets from persistent storage (call at startup)
void calibration_module_set_offsets(float temp, float humi, float press);

// Reset all offsets to 0 (for testing)
void calibration_module_reset();

// Format current calibration as the JSON payload published on the calibration
// topic (see docs/ota-calibration-protocol.md §4.2).
// Output: {"cal_temp":<f>,"cal_humi":<f>,"cal_press":<f>,"bat_divider":<f>}
// Returns number of chars written (excluding null terminator).
int calibration_format_response(char* buf, size_t buf_size);

// Battery voltage-divider ratio mirror, carried in the calibration report and
// set via set_calibration / restored from the store at boot (0 = unset).
void calibration_module_set_bat_divider(float ratio);
float calibration_get_bat_divider();

// Callback invoked by set_calibration for a generic key/value (e.g.
// "bat_divider"): platform code applies and persists it. Return false to
// reject an unsupported key.
using CalibrationValueCallback = bool (*)(const char* key, float value);
void calibration_module_set_value_callback(CalibrationValueCallback cb);

// Callback invoked after a successful set_offset command with all three
// current offsets, so platform code can persist them (RTC, EEPROM, etc.).
using CalibrationPersistCallback = void (*)(float temp, float humi, float press);
void calibration_module_set_persist_callback(CalibrationPersistCallback cb);

// Callback invoked when the device should publish calibration data
// (triggered by request_calibration command). Platform code formats
// the response with calibration_format_response() and publishes it.
using CalibrationResponseCallback = void (*)();
void calibration_module_set_response_callback(CalibrationResponseCallback cb);
