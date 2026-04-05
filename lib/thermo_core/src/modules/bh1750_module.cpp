#include "modules/bh1750_module.h"

void bh1750_module_register(ModuleRegistry& reg) {
    reg.add_metric("lux", "lx");
}

void bh1750_module_contribute(PayloadBuilder& pb, float lux) {
    pb.add_float("lux", lux, 0);
}
