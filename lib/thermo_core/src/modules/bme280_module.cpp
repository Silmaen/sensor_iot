#include "modules/bme280_module.h"

void bme280_module_register(ModuleRegistry& reg) {
    reg.add_metric("temp", "\xC2\xB0""C"); // °C in UTF-8
    reg.add_metric("humi", "%");
    reg.add_metric("press", "hPa");
}

void bme280_module_contribute(PayloadBuilder& pb, const SensorData& data) {
    pb.add_float("temp", data.temperature, 1);
    pb.add_float("humi", data.humidity, 1);
    pb.add_float("press", data.pressure, 1);
}
