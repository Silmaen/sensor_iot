#include <unity.h>
#include <string.h>
#include "diagnostics.h"

// A nominal deep-sleep wake: clean reset, good signal, healthy heap, no battery flags.
static DiagData nominal() {
    DiagData d;
    d.reset_cause = 4;   // deep-sleep (normal wake)
    d.rssi = -50;        // good
    d.heap = 100000;
    return d;
}

void test_diag_health_ok(void) {
    DiagData d = nominal();
    const char* msg = "";
    TEST_ASSERT_EQUAL(HEALTH_OK, diag_evaluate(d, &msg));
    TEST_ASSERT_EQUAL_STRING("ok", msg);
}

void test_diag_health_brownout_error(void) {
    DiagData d = nominal();
    d.reset_cause = 5; // brownout
    const char* msg = "";
    TEST_ASSERT_EQUAL(HEALTH_ERROR, diag_evaluate(d, &msg));
    TEST_ASSERT_EQUAL_STRING("reset_brownout", msg);
}

void test_diag_health_critical_battery(void) {
    DiagData d = nominal();
    d.has_battery = true;
    d.bat_critical = true;
    const char* msg = "";
    TEST_ASSERT_EQUAL(HEALTH_ERROR, diag_evaluate(d, &msg));
    TEST_ASSERT_EQUAL_STRING("critical_battery", msg);
}

void test_diag_health_missed_wakes_warning(void) {
    DiagData d = nominal();
    d.miss = 2;
    const char* msg = "";
    TEST_ASSERT_EQUAL(HEALTH_WARNING, diag_evaluate(d, &msg));
    TEST_ASSERT_EQUAL_STRING("missed_wakes", msg);
}

void test_diag_health_weak_signal_warning(void) {
    DiagData d = nominal();
    d.rssi = -85;
    const char* msg = "";
    TEST_ASSERT_EQUAL(HEALTH_WARNING, diag_evaluate(d, &msg));
    TEST_ASSERT_EQUAL_STRING("weak_signal", msg);
}

void test_diag_health_booted_info(void) {
    DiagData d = nominal();
    d.reset_cause = 1; // power-on
    const char* msg = "";
    TEST_ASSERT_EQUAL(HEALTH_INFO, diag_evaluate(d, &msg));
    TEST_ASSERT_EQUAL_STRING("booted", msg);
}

void test_diag_error_beats_warning(void) {
    DiagData d = nominal();
    d.reset_cause = 5; // brownout (error)
    d.miss = 3;        // missed wakes (warning) -> error must win
    const char* msg = "";
    TEST_ASSERT_EQUAL(HEALTH_ERROR, diag_evaluate(d, &msg));
    TEST_ASSERT_EQUAL_STRING("reset_brownout", msg);
}

void test_format_diag_payload(void) {
    DiagData d = nominal();
    d.boot = 12;
    d.seq = 45;
    d.pubfail = 0;
    d.rssi = -42;
    d.heap = 123456;
    d.has_battery = true;
    d.bat_soc = 88;
    char buf[256];
    int len = format_diag_payload(d, "C3-ABC123", "C3BMELUX", 1, "1.0.0", buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"level\":\"ok\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"rst\":4"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"boot\":12"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"seq\":45"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"rssi\":-42"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bat\":88"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hw\":\"C3BMELUX\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"id\":\"C3-ABC123\""));
}
