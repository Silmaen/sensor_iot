#include "modules/sht30_module.h"

void sht30_module_register(ModuleRegistry& reg) {
    reg.add_metric("temperature", "\xC2\xB0""C"); // °C in UTF-8
    reg.add_metric("humidity", "%");
}

void sht30_module_contribute(PayloadBuilder& pb, const SensorData& data) {
    pb.add_float("temperature", data.temperature, 1);
    pb.add_float("humidity", data.humidity, 1);
}
