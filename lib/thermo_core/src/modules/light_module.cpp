#include "modules/light_module.h"

void light_module_register(ModuleRegistry& reg) {
    reg.add_metric("light", "%");
}

void light_module_contribute(PayloadBuilder& pb, uint8_t light_pct) {
    pb.add_uint("light", light_pct);
}

uint8_t adc_to_light_pct(uint16_t raw, uint16_t adc_max) {
    if (adc_max == 0) return 0;
    if (raw >= adc_max) return 100;
    return static_cast<uint8_t>((static_cast<uint32_t>(raw) * 100) / adc_max);
}
