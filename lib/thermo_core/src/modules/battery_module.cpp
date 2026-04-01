#include "modules/battery_module.h"
#include "battery.h"
#include "mqtt_payload.h"

static BatteryAdcReader adc_reader = nullptr;
static BatteryCalibrationCallback calibration_cb = nullptr;

static bool handle_calibrate_battery(const char* payload) {
    float measured = 0.0f;
    if (!parse_command_float_value(payload, measured))
        return false;
    if (measured < BATTERY_VOLTAGE_EMPTY || measured > 12.0f)
        return false;
    if (!adc_reader)
        return false;
    uint16_t raw = adc_reader();
    float new_ratio = battery_calibrate(measured, raw);
    if (calibration_cb)
        calibration_cb(new_ratio);
    return true;
}

static const CommandParamDef calibrate_params[] = {
    {"value", "number"},
};

void battery_module_register(ModuleRegistry& reg) {
    reg.add_metric("bat", "%");
    reg.add_metric("batv", "V");
    reg.add_command("calibrate_battery", handle_calibrate_battery,
                    calibrate_params, 1);
}

void battery_module_contribute(PayloadBuilder& pb, uint8_t soc, float voltage) {
    pb.add_uint("bat", soc);
    pb.add_float("batv", voltage, 2);
}

void battery_module_set_adc_reader(BatteryAdcReader reader) {
    adc_reader = reader;
}

void battery_module_set_calibration_callback(BatteryCalibrationCallback cb) {
    calibration_cb = cb;
}
