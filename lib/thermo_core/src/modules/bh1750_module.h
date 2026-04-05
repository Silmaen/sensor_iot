#pragma once

#include "module_registry.h"
#include "payload_builder.h"

void bh1750_module_register(ModuleRegistry& reg);
void bh1750_module_contribute(PayloadBuilder& pb, float lux);
