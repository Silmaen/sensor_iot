#pragma once

#include "module_registry.h"
#include "payload_builder.h"

#include <stdint.h>

static constexpr uint8_t RELAY_COUNT = 2;

void relay_module_register(ModuleRegistry& reg);
void relay_module_contribute(PayloadBuilder& pb);

// Call from loop() with current millis() to handle contact auto-revert.
// Returns true if any relay state changed (caller may want to publish).
bool relay_module_tick(unsigned long now_ms);

// Reset all relay state (for testing)
void relay_module_reset();

// Platform code provides this callback to drive the relay hardware.
// relay_num: 1 or 2.  active: true = energized (NO closed, NC open).
using RelayWriter = void (*)(uint8_t relay_num, bool active);
void relay_module_set_writer(RelayWriter writer);
