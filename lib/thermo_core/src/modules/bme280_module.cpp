#include "modules/bme280_module.h"

void bme280_module_register(ModuleRegistry& reg) {
    reg.add_metric("temperature", "\xC2\xB0""C"); // °C in UTF-8
    reg.add_metric("humidity", "%");
    reg.add_metric("pressure", "hPa");
}

void bme280_module_contribute(PayloadBuilder& pb, const SensorData& data) {
    pb.add_float("temperature", data.temperature, 1);
    pb.add_float("humidity", data.humidity, 1);
    pb.add_float("pressure", data.pressure, 1);
}
