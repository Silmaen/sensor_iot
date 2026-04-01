#pragma once

#include "module_registry.h"
#include "payload_builder.h"

#include <stdint.h>

void light_module_register(ModuleRegistry& reg);
void light_module_contribute(PayloadBuilder& pb, uint8_t light_pct);

// Convert raw ADC reading to light percentage (0-100).
// adc_max: maximum ADC value (1023 for 10-bit, 4095 for 12-bit).
uint8_t adc_to_light_pct(uint16_t raw, uint16_t adc_max);
