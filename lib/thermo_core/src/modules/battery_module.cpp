#include "modules/battery_module.h"

void battery_module_register(ModuleRegistry& reg) {
    reg.add_metric("battery_pct", "%");
    reg.add_metric("battery_v", "V");
}

void battery_module_contribute(PayloadBuilder& pb, uint8_t soc, float voltage) {
    pb.add_uint("battery_pct", soc);
    pb.add_float("battery_v", voltage, 2);
}
