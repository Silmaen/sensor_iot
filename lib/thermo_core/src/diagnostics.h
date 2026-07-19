#pragma once

#include <stddef.h>
#include <stdint.h>

// Portable device health / diagnostics — shared by all device types.
// See docs/diagnostics.md. Kept platform-agnostic: main.cpp gathers the runtime
// values (reset cause via platform_diag.*, RTC counters, RSSI, heap, battery) into
// a DiagData and calls these; the diag topic is published only when health >= warning
// (or on the get_diag command).

enum HealthLevel : uint8_t { HEALTH_OK = 0, HEALTH_INFO = 1, HEALTH_WARNING = 2, HEALTH_ERROR = 3 };

// "ok" | "info" | "warning" | "error"
const char* health_level_str(HealthLevel level);

// Runtime snapshot gathered by the orchestrator. reset_cause mirrors
// platform_diag.h ResetCause (1=power-on 2=ext 3=sw 4=deep-sleep 5=brownout 6=panic 7=wdt).
struct DiagData {
    uint8_t  reset_cause = 0;
    uint32_t boot        = 0;   // boots since cold start
    uint32_t miss        = 0;   // consecutive connect failures since last publish
    uint32_t wake_ms     = 0;   // wake -> publish duration
    uint32_t seq         = 0;   // monotonic publish counter (server-side loss detection)
    uint32_t pubfail     = 0;   // publish() failures since last publish
    int32_t  rssi        = 0;   // dBm at connect (0 = n/a)
    uint32_t heap        = 0;   // free heap bytes (0 = n/a)
    bool     has_battery = false;
    bool     bat_critical = false; // soc <= critical threshold (computed by main.cpp)
    bool     bat_low     = false;  // soc <= warn threshold
    uint8_t  bat_soc     = 0;   // 0-100
};

// Evaluate the health level + dominant message (*out_message -> static string).
HealthLevel diag_evaluate(const DiagData& d, const char** out_message);

// Format the diag payload as JSON. Returns end() (bytes written) — fits MQTT 512 B.
int format_diag_payload(const DiagData& d, const char* hw_id, const char* hw_code,
                        int hw_rev, const char* fw, char* buf, size_t buf_size);
