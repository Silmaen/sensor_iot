#include "modules/ota_module.h"
#include "mqtt_payload.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static OtaPerformer  perform_cb    = nullptr;
static OtaAckFn      ack_cb        = nullptr;
static OtaBatteryFn  battery_cb    = nullptr;
static const char*   dev_hw_code   = "";
static uint16_t      dev_hw_rev    = 0;
static const char*   dev_fw_version = "";

// Extract an unsigned integer field ("key":<digits>) from a JSON payload.
static bool parse_uint_field(const char* json, const char* key, uint32_t& out) {
    char search[24];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) return false;
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos < '0' || *pos > '9') return false;
    out = (uint32_t)strtoul(pos, nullptr, 10);
    return true;
}

static void ack(const char* status, const char* message) {
    if (ack_cb) ack_cb(status, message);
}

bool ota_module_handle(const char* payload) {
    char url[160];
    char md5[40];
    char ver[24];
    char hw[16];
    uint32_t hwrev = 0;

    // A malformed command is reported so the server can mark it failed.
    if (!parse_command_string_value(payload, "value", url, sizeof(url)) ||
        !parse_command_string_value(payload, "md5", md5, sizeof(md5)) ||
        !parse_command_string_value(payload, "ver", ver, sizeof(ver)) ||
        !parse_command_string_value(payload, "hw", hw, sizeof(hw)) ||
        !parse_uint_field(payload, "hwrev", hwrev)) {
        ack("error", "bad_request");
        return true;
    }

    // Guard 1: the image must match this device's hardware exactly.
    if (strcmp(hw, dev_hw_code) != 0 || hwrev != dev_hw_rev) {
        ack("error", "hw_mismatch");
        return true;
    }

    // Guard 2: refuse to re-flash the version already running (idempotence).
    if (strcmp(ver, dev_fw_version) == 0) {
        ack("error", "same_version");
        return true;
    }

    // Guard 3: refuse on low battery (only when a provider reports a level).
    if (battery_cb) {
        int const pct = battery_cb();
        if (pct >= 0 && pct < OTA_MIN_BATTERY_PCT) {
            ack("error", "low_battery");
            return true;
        }
    }

    if (!perform_cb) {
        ack("error", "no_performer");
        return true;
    }

    // Announce, then flash. On success the performer reboots and never returns.
    ack("start", nullptr);
    OtaResult const r = perform_cb(url, md5);
    if (!r.ok)
        ack("error", r.error ? r.error : "update_failed");
    return true;
}

void ota_module_register(ModuleRegistry& reg) {
    // ota_update is NOT added to the command registry: it is inferred by the
    // server from the "ota":1 capability flag (keeps it out of the size-limited
    // `commands` message). main.cpp dispatches it via ota_module_handle().
    (void)reg;
}

void ota_module_set_identity(const char* hw_code, uint16_t hw_rev,
                             const char* fw_version) {
    dev_hw_code    = hw_code ? hw_code : "";
    dev_hw_rev     = hw_rev;
    dev_fw_version = fw_version ? fw_version : "";
}

void ota_module_set_performer(OtaPerformer fn)      { perform_cb = fn; }
void ota_module_set_ack(OtaAckFn fn)                { ack_cb = fn; }
void ota_module_set_battery_provider(OtaBatteryFn fn) { battery_cb = fn; }

void ota_module_reset() {
    perform_cb     = nullptr;
    ack_cb         = nullptr;
    battery_cb     = nullptr;
    dev_hw_code    = "";
    dev_hw_rev     = 0;
    dev_fw_version = "";
}
