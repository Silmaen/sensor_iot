#pragma once

#include <stdint.h>

// Portable device diagnostics primitives. Native reset codes and heap APIs
// differ per MCU, so this normalizes them behind two free functions (same
// #if-per-platform pattern as hw_id.* / battery_adc.*). Used by the core
// diagnostics module, which stays platform-agnostic.

// Normalized reset cause of the current boot (see docs/mqtt-protocol.md `diag`).
enum ResetCause : uint8_t {
    RESET_UNKNOWN    = 0,
    RESET_POWER_ON   = 1,  // cold power-up
    RESET_EXTERNAL   = 2,  // RST pin / external reset
    RESET_SOFTWARE   = 3,  // reboot() / software restart
    RESET_DEEP_SLEEP = 4,  // normal timer wake from deep sleep
    RESET_BROWNOUT   = 5,  // supply sag (e.g. WiFi TX peak vs weak LDO)
    RESET_PANIC      = 6,  // exception / crash
    RESET_WATCHDOG   = 7,  // watchdog timeout
};

// Normalized reset cause for this boot.
uint8_t platform_reset_cause();

// Free heap in bytes (0 if the platform exposes no simple metric, e.g. SAMD).
uint32_t platform_free_heap();
