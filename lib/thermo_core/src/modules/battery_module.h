#pragma once

#include "module_registry.h"
#include "payload_builder.h"

void battery_module_register(ModuleRegistry& reg);
void battery_module_contribute(PayloadBuilder& pb, uint8_t soc, float voltage);
