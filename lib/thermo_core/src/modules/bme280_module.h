#pragma once

#include "module_registry.h"
#include "payload_builder.h"
#include "sensor_data.h"

void bme280_module_register(ModuleRegistry& reg);
void bme280_module_contribute(PayloadBuilder& pb, const SensorData& data);
