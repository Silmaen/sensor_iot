#pragma once

#include "interfaces/i_config_store.h"

// Canonical keys stored in the runtime config store (see
// docs/ota-calibration-protocol.md §3). Kept short: NVS keys are limited to
// 15 chars. Lowercase identifiers avoid clashing with the -DDEVICE_ID build
// macro used by the native test env.
namespace config_keys {
constexpr const char* device_id   = "device_id";
constexpr const char* cal_temp    = "cal_temp";
constexpr const char* cal_humi    = "cal_humi";
constexpr const char* cal_press   = "cal_press";
constexpr const char* bat_divider = "bat_divider";
} // namespace config_keys

// The `cal` flag reported in capabilities: true once at least one calibration
// key has been written (independent of device_id). A fresh / factory-reset
// store reports false so the server knows to re-push the mirror (§5.1).
inline bool config_calibration_present(IConfigStore& store) {
    return store.has(config_keys::cal_temp) ||
           store.has(config_keys::cal_humi) ||
           store.has(config_keys::cal_press) ||
           store.has(config_keys::bat_divider);
}
