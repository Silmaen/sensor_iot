#include "modules/relay_module.h"
#include "mqtt_payload.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static RelayWriter relay_writer = nullptr;
static bool relay_state[RELAY_COUNT] = {false, false};

// Contact timer: two-phase — pending_delay is set by the command handler,
// then resolved to an absolute deadline on the next tick().
static uint32_t contact_pending_delay[RELAY_COUNT] = {0, 0};
static unsigned long contact_deadline[RELAY_COUNT] = {0, 0};

// --- JSON field helper (local to this module) ---

// Extract unsigned integer for a given JSON key. Returns true if found.
static bool parse_uint_field(const char* json, const char* key, uint32_t& out) {
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) return false;
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos < '0' || *pos > '9') return false;
    out = static_cast<uint32_t>(strtoul(pos, nullptr, 10));
    return true;
}

static bool valid_relay_num(uint32_t num) {
    return num >= 1 && num <= RELAY_COUNT;
}

static void set_relay(uint8_t relay_num, bool active) {
    relay_state[relay_num - 1] = active;
    if (relay_writer)
        relay_writer(relay_num, active);
}

// --- Command handlers ---

static bool handle_relay_toggle(const char* payload) {
    uint32_t relay_num = 0;
    if (!parse_command_value(payload, relay_num))
        return false;
    if (!valid_relay_num(relay_num))
        return false;
    uint8_t idx = static_cast<uint8_t>(relay_num);
    // Cancel any pending contact timer on this relay
    contact_pending_delay[idx - 1] = 0;
    contact_deadline[idx - 1] = 0;
    set_relay(idx, !relay_state[idx - 1]);
    return true;
}

// Max contact delay: 5 minutes (300 000 ms)
static constexpr uint32_t MAX_CONTACT_DELAY_MS = 300000;

static bool handle_relay_contact(const char* payload) {
    uint32_t relay_num = 0;
    if (!parse_uint_field(payload, "relay", relay_num))
        return false;
    if (!valid_relay_num(relay_num))
        return false;
    uint32_t delay_ms = 0;
    if (!parse_uint_field(payload, "value", delay_ms))
        return false;
    if (delay_ms == 0 || delay_ms > MAX_CONTACT_DELAY_MS)
        return false;
    uint8_t idx = static_cast<uint8_t>(relay_num);
    set_relay(idx, true);
    // Store delay — will be resolved to absolute deadline on next tick()
    contact_pending_delay[idx - 1] = delay_ms;
    contact_deadline[idx - 1] = 0;
    return true;
}

// --- Command parameter definitions ---

static const CommandParamDef toggle_params[] = {
    {"value", "number"},
};

static const CommandParamDef contact_params[] = {
    {"relay", "number"},
    {"value", "number"},
};

// --- Module interface ---

void relay_module_register(ModuleRegistry& reg) {
    reg.add_metric("relay1", "");
    reg.add_metric("relay2", "");
    reg.add_command("relay_toggle", handle_relay_toggle,
                    toggle_params, 1);
    reg.add_command("relay_contact", handle_relay_contact,
                    contact_params, 2);
}

void relay_module_contribute(PayloadBuilder& pb) {
    pb.add_uint("relay1", relay_state[0] ? 1 : 0);
    pb.add_uint("relay2", relay_state[1] ? 1 : 0);
}

bool relay_module_tick(unsigned long now_ms) {
    bool changed = false;
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        // Phase 1: resolve pending delay to absolute deadline
        if (contact_pending_delay[i] > 0) {
            contact_deadline[i] = now_ms + contact_pending_delay[i];
            contact_pending_delay[i] = 0;
        }
        // Phase 2: check if deadline has expired
        if (contact_deadline[i] > 0 && now_ms >= contact_deadline[i]) {
            contact_deadline[i] = 0;
            set_relay(i + 1, false);
            changed = true;
        }
    }
    return changed;
}

void relay_module_reset() {
    relay_writer = nullptr;
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        relay_state[i] = false;
        contact_pending_delay[i] = 0;
        contact_deadline[i] = 0;
    }
}

void relay_module_set_writer(RelayWriter writer) {
    relay_writer = writer;
}
