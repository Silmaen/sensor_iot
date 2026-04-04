#pragma once

#include "module_registry.h"
#include "sensor_data.h"

#include <stdint.h>

// Register the set_offset command. No metrics — this module only adjusts
// readings from other sensor modules.
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

// Format current calibration offsets as JSON payload.
// Output: {"temp":<f>,"humi":<f>,"press":<f>}
// Returns number of chars written (excluding null terminator).
int calibration_format_response(char* buf, size_t buf_size);

// Callback invoked after a successful set_offset command with all three
// current offsets, so platform code can persist them (RTC, EEPROM, etc.).
using CalibrationPersistCallback = void (*)(float temp, float humi, float press);
void calibration_module_set_persist_callback(CalibrationPersistCallback cb);

// Callback invoked when the device should publish calibration data
// (triggered by request_calibration command). Platform code formats
// the response with calibration_format_response() and publishes it.
using CalibrationResponseCallback = void (*)();
void calibration_module_set_response_callback(CalibrationResponseCallback cb);
