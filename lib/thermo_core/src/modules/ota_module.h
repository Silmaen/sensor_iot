#pragma once

#include "module_registry.h"

#include <stdint.h>

// OTA update module (portable core). Registers the `ota_update` command and
// runs the guards (hw/rev compatibility, version idempotence, battery). The
// actual download+flash is delegated to a platform-injected performer; ack
// publishing is delegated to an injected callback (thermo_core has no network).
// See docs/ota-calibration-protocol.md §6.

// Minimum battery percentage required to start an OTA. Overridable per build.
#ifndef OTA_MIN_BATTERY_PCT
#define OTA_MIN_BATTERY_PCT 40
#endif

// Platform performer: download `url`, verify `md5`, flash, reboot. On success
// the device reboots and this never returns; a return therefore always means
// failure, carrying a short reason in `error`.
struct OtaResult {
    bool ok;
    const char* error; // reason when !ok (e.g. "download_failed", "md5_mismatch")
};
using OtaPerformer = OtaResult (*)(const char* url, const char* md5);

// Publish an ack: status is "start" or "error"; message is a reason or nullptr.
using OtaAckFn = void (*)(const char* status, const char* message);

// Return current battery percentage, or a negative value if unknown / no
// battery (skips the low-battery guard).
using OtaBatteryFn = int (*)();

// Register the ota_update command (no params advertised; support is signalled
// by its presence and by capabilities "ota":1).
void ota_module_register(ModuleRegistry& reg);

// Inject the device's own identity so the compat guard can reject a mismatched
// image. fw_version drives the same-version idempotence guard.
void ota_module_set_identity(const char* hw_code, uint16_t hw_rev,
                             const char* fw_version);

void ota_module_set_performer(OtaPerformer fn);
void ota_module_set_ack(OtaAckFn fn);
void ota_module_set_battery_provider(OtaBatteryFn fn); // optional

// Handle an ota_update command payload. Returns true once the command has been
// processed (its own detailed ack is emitted via the ack callback, so the
// generic dispatch ack must be suppressed for this action).
bool ota_module_handle(const char* payload);

// Reset all injected state (for tests).
void ota_module_reset();
