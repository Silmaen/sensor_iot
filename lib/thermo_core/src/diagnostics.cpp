#include "diagnostics.h"

#include "payload_builder.h"

// RSSI / heap thresholds for the health ladder.
static constexpr int32_t  RSSI_WEAK = -80;   // < this  -> warning
static constexpr int32_t  RSSI_FAIR = -70;   // < this  -> info (between FAIR and WEAK)
static constexpr uint32_t HEAP_LOW  = 20000; // < this  -> warning (bytes)

const char* health_level_str(HealthLevel level) {
    switch (level) {
    case HEALTH_ERROR:   return "error";
    case HEALTH_WARNING: return "warning";
    case HEALTH_INFO:    return "info";
    default:             return "ok";
    }
}

HealthLevel diag_evaluate(const DiagData& d, const char** out_message) {
    // error (highest priority first)
    if (d.reset_cause == 5) { *out_message = "reset_brownout";  return HEALTH_ERROR; }
    if (d.reset_cause == 6) { *out_message = "reset_panic";     return HEALTH_ERROR; }
    if (d.reset_cause == 7) { *out_message = "reset_wdt";       return HEALTH_ERROR; }
    if (d.has_battery && d.bat_critical) { *out_message = "critical_battery"; return HEALTH_ERROR; }
    // warning
    if (d.has_battery && d.bat_low)      { *out_message = "low_battery";  return HEALTH_WARNING; }
    if (d.miss > 0)                      { *out_message = "missed_wakes"; return HEALTH_WARNING; }
    if (d.rssi != 0 && d.rssi < RSSI_WEAK) { *out_message = "weak_signal"; return HEALTH_WARNING; }
    if (d.heap != 0 && d.heap < HEAP_LOW)  { *out_message = "low_memory";  return HEALTH_WARNING; }
    // info
    if (d.reset_cause == 1 || d.reset_cause == 2 || d.reset_cause == 3) {
        *out_message = "booted"; return HEALTH_INFO;
    }
    if (d.rssi != 0 && d.rssi < RSSI_FAIR) { *out_message = "fair_signal"; return HEALTH_INFO; }
    // ok
    *out_message = "ok";
    return HEALTH_OK;
}

int format_diag_payload(const DiagData& d, const char* hw_id, const char* hw_code,
                        int hw_rev, const char* fw, char* buf, size_t buf_size) {
    const char* msg = "ok";
    const HealthLevel level = diag_evaluate(d, &msg);

    PayloadBuilder pb{buf, buf_size, 0, true};
    pb.begin();
    pb.add_string("level", health_level_str(level));
    pb.add_string("message", msg);
    pb.add_uint("rst", d.reset_cause);
    pb.add_uint("boot", d.boot);
    pb.add_uint("miss", d.miss);
    pb.add_uint("wake_ms", d.wake_ms);
    pb.add_uint("seq", d.seq);
    pb.add_uint("pubfail", d.pubfail);
    // Uplink-confirmation counters: only present while the (opt-in) confirm mode
    // is active, so nominal diag payloads are unchanged. Server derives the
    // confirmed-delivery rate as tx_ok / tx_sent. See docs/diagnostics.md.
    if (d.tx_sent > 0) {
        pb.add_uint("txsent", d.tx_sent);
        pb.add_uint("txok", d.tx_ok);
    }
    // Wake-path instrumentation (cumulative). Emitted only when non-zero, so a
    // healthy node's payload is unchanged; short keys keep it within budget.
    // wf=WiFi-connect fails, mf=MQTT-connect fails, sf=invalid-sensor skips,
    // bx=non-deep-sleep boots (>1 => spurious double-boot). See docs/diagnostics.md.
    if (d.wifi_fail > 0)   pb.add_uint("wf", d.wifi_fail);
    if (d.mqtt_fail > 0)   pb.add_uint("mf", d.mqtt_fail);
    if (d.sensor_fail > 0) pb.add_uint("sf", d.sensor_fail);
    if (d.boot_nds > 0)    pb.add_uint("bx", d.boot_nds);
    pb.add_int("rssi", d.rssi);
    pb.add_uint("heap", d.heap);
    if (d.has_battery) pb.add_uint("bat", d.bat_soc);
    pb.add_string("fw", fw);
    pb.add_string("hw", hw_code);
    pb.add_int("hwrev", hw_rev);
    pb.add_string("id", hw_id);
    return pb.end();
}
