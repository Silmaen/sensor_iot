#pragma once

#include "module_registry.h"
#include "payload_builder.h"

void battery_module_register(ModuleRegistry& reg);
void battery_module_contribute(PayloadBuilder& pb, uint8_t soc, float voltage);

// ADC reader callback — platform code sets this so the calibrate command
// can read the current ADC value without linking to hw-specific code.
using BatteryAdcReader = uint16_t (*)();

// Callback invoked after successful calibration with the new ratio.
// Platform code sets this to persist the ratio (e.g. RTC memory).
using BatteryCalibrationCallback = void (*)(float new_ratio);

void battery_module_set_adc_reader(BatteryAdcReader reader);
void battery_module_set_calibration_callback(BatteryCalibrationCallback cb);
