#include "modules/battery_module.h"

void battery_module_register(ModuleRegistry& reg) {
    reg.add_metric("bat", "%");
    reg.add_metric("batv", "V");
}

void battery_module_contribute(PayloadBuilder& pb, uint8_t soc, float voltage) {
    pb.add_uint("bat", soc);
    pb.add_float("batv", voltage, 2);
}
