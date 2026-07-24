#include <unity.h>
#include <string.h>
#include "modules/ota_module.h"
#include "module_registry.h"

// --- Mocks ---
static char mock_ack_status[16];
static char mock_ack_message[32];
static int  mock_ack_count;

static bool mock_perform_called;
static char mock_perform_url[160];
static char mock_perform_md5[40];
static OtaResult mock_perform_result;

static int  mock_battery_pct;

static void mock_ack(const char* status, const char* message) {
    mock_ack_count++;
    strncpy(mock_ack_status, status ? status : "", sizeof(mock_ack_status) - 1);
    mock_ack_status[sizeof(mock_ack_status) - 1] = '\0';
    strncpy(mock_ack_message, message ? message : "", sizeof(mock_ack_message) - 1);
    mock_ack_message[sizeof(mock_ack_message) - 1] = '\0';
}

static OtaResult mock_perform(const char* url, const char* md5) {
    mock_perform_called = true;
    strncpy(mock_perform_url, url, sizeof(mock_perform_url) - 1);
    mock_perform_url[sizeof(mock_perform_url) - 1] = '\0';
    strncpy(mock_perform_md5, md5, sizeof(mock_perform_md5) - 1);
    mock_perform_md5[sizeof(mock_perform_md5) - 1] = '\0';
    return mock_perform_result;
}

static int mock_battery(void) { return mock_battery_pct; }

static void ota_test_setup(void) {
    ota_module_reset();
    memset(mock_ack_status, 0, sizeof(mock_ack_status));
    memset(mock_ack_message, 0, sizeof(mock_ack_message));
    mock_ack_count = 0;
    mock_perform_called = false;
    memset(mock_perform_url, 0, sizeof(mock_perform_url));
    memset(mock_perform_md5, 0, sizeof(mock_perform_md5));
    mock_perform_result = {true, nullptr}; // simulate "would reboot on success"
    mock_battery_pct = 100;
    ota_module_set_identity("E8BMEBAT", 1, "1.0.0");
    ota_module_set_performer(mock_perform);
    ota_module_set_ack(mock_ack);
}

static const char* VALID =
    "{\"action\":\"ota_update\",\"value\":\"http://s/fw/E8BMEBAT/1/1.1.0.bin\","
    "\"md5\":\"d41d8cd98f00b204e9800998ecf8427e\",\"ver\":\"1.1.0\","
    "\"hw\":\"E8BMEBAT\",\"hwrev\":1}";

void test_ota_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    ota_module_register(reg);
    // ota_update is inferred server-side from the "ota":1 capability flag, so the
    // module registers no command (kept out of the size-limited `commands` list).
    // Handling still goes through ota_module_handle() (see the other OTA tests).
    TEST_ASSERT_EQUAL_UINT(0, reg.num_commands);
}

void test_ota_happy_path(void) {
    ota_test_setup();
    bool handled = ota_module_handle(VALID);
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_TRUE(mock_perform_called);
    TEST_ASSERT_EQUAL_STRING("http://s/fw/E8BMEBAT/1/1.1.0.bin", mock_perform_url);
    TEST_ASSERT_EQUAL_STRING("d41d8cd98f00b204e9800998ecf8427e", mock_perform_md5);
    // "start" ack, no error (success reboots in reality)
    TEST_ASSERT_EQUAL_STRING("start", mock_ack_status);
}

void test_ota_hw_code_mismatch(void) {
    ota_test_setup();
    ota_module_set_identity("C3BMELUX", 1, "1.0.0"); // device is a different type
    ota_module_handle(VALID);
    TEST_ASSERT_FALSE(mock_perform_called);
    TEST_ASSERT_EQUAL_STRING("error", mock_ack_status);
    TEST_ASSERT_EQUAL_STRING("hw_mismatch", mock_ack_message);
}

void test_ota_hw_rev_mismatch(void) {
    ota_test_setup();
    ota_module_set_identity("E8BMEBAT", 2, "1.0.0"); // rev 2, image is rev 1
    ota_module_handle(VALID);
    TEST_ASSERT_FALSE(mock_perform_called);
    TEST_ASSERT_EQUAL_STRING("hw_mismatch", mock_ack_message);
}

void test_ota_same_version_rejected(void) {
    ota_test_setup();
    ota_module_set_identity("E8BMEBAT", 1, "1.1.0"); // already running 1.1.0
    ota_module_handle(VALID);
    TEST_ASSERT_FALSE(mock_perform_called);
    TEST_ASSERT_EQUAL_STRING("same_version", mock_ack_message);
}

void test_ota_low_battery_rejected(void) {
    ota_test_setup();
    ota_module_set_battery_provider(mock_battery);
    mock_battery_pct = 20; // below OTA_MIN_BATTERY_PCT (40)
    ota_module_handle(VALID);
    TEST_ASSERT_FALSE(mock_perform_called);
    TEST_ASSERT_EQUAL_STRING("low_battery", mock_ack_message);
}

void test_ota_battery_ok_proceeds(void) {
    ota_test_setup();
    ota_module_set_battery_provider(mock_battery);
    mock_battery_pct = 80;
    ota_module_handle(VALID);
    TEST_ASSERT_TRUE(mock_perform_called);
    TEST_ASSERT_EQUAL_STRING("start", mock_ack_status);
}

void test_ota_no_battery_provider_skips_guard(void) {
    ota_test_setup(); // no battery provider injected
    ota_module_handle(VALID);
    TEST_ASSERT_TRUE(mock_perform_called);
}

void test_ota_performer_failure_acked(void) {
    ota_test_setup();
    mock_perform_result = {false, "download_failed"};
    ota_module_handle(VALID);
    TEST_ASSERT_TRUE(mock_perform_called);
    TEST_ASSERT_EQUAL_STRING("error", mock_ack_status);
    TEST_ASSERT_EQUAL_STRING("download_failed", mock_ack_message);
}

void test_ota_bad_request(void) {
    ota_test_setup();
    // Missing md5 / ver / hw / hwrev
    ota_module_handle("{\"action\":\"ota_update\",\"value\":\"http://s/x.bin\"}");
    TEST_ASSERT_FALSE(mock_perform_called);
    TEST_ASSERT_EQUAL_STRING("bad_request", mock_ack_message);
}
