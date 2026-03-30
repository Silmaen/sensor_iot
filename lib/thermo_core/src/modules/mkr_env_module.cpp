#include "modules/mkr_env_module.h"

void mkr_env_module_register(ModuleRegistry& reg) {
    reg.add_metric("temp", "\xC2\xB0""C"); // °C in UTF-8
    reg.add_metric("humi", "%");
    reg.add_metric("press", "hPa");
    reg.add_metric("lux", "lux");
    reg.add_metric("uv", "");
}

void mkr_env_module_contribute(PayloadBuilder& pb, const SensorData& data) {
    pb.add_float("temp", data.temperature, 1);
    pb.add_float("humi", data.humidity, 1);
    pb.add_float("press", data.pressure, 1);
    pb.add_float("lux", data.light_lux, 0);
    pb.add_float("uv", data.uv_index, 2);
}
