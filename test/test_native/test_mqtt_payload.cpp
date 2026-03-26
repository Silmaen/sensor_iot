#include <unity.h>
#include <string.h>
#include "mqtt_payload.h"
#include "module_registry.h"

// --- Sensor payload tests ---

void test_format_sensor_payload(void) {
    SensorData data = {23.5f, 65.2f, 1013.1f, true};
    char buf[128];
    int len = format_sensor_payload(data, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temperature\":23.5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"humidity\":65.2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pressure\":1013.1"));
}

void test_format_sensor_payload_with_battery(void) {
    SensorData data = {22.0f, 50.0f, 1000.0f, true};
    char buf[192];
    int len = format_sensor_payload_with_battery(data, 75, 7.8f, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_pct\":75"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_v\":7.80"));
}

// --- Status payload tests ---

void test_format_status_payload_warning(void) {
    char buf[128];
    int len = format_status_payload("warning", "low_battery", buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"level\":\"warning\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"message\":\"low_battery\""));
}

void test_format_status_payload_ok_no_message(void) {
    char buf[128];
    int len = format_status_payload("ok", nullptr, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"level\":\"ok\""));
    TEST_ASSERT_NULL(strstr(buf, "\"message\""));
}

void test_format_status_payload_error(void) {
    char buf[128];
    format_status_payload("error", "sensor_fault", buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"level\":\"error\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"message\":\"sensor_fault\""));
}

// --- Capabilities payload tests ---

static bool dummy_cmd_handler(const char*) { return true; }

void test_format_capabilities_payload(void) {
    static const CommandParamDef si_params[] = {{"value", "number"}};
    ModuleRegistry reg;
    reg.init();
    reg.add_metric("temperature", "\xC2\xB0""C");
    reg.add_metric("humidity", "%");
    reg.add_metric("pressure", "hPa");
    reg.add_command("set_interval", dummy_cmd_handler, si_params, 1);

    char buf[512];
    int len = format_capabilities_payload("ESP-ABC123", 60,
                                          reg, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hardware_id\":\"ESP-ABC123\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"publish_interval\":60"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"metrics\":[\"temperature\",\"humidity\",\"pressure\"]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"units\":{"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"humidity\":\"%\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pressure\":\"hPa\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"commands\":[\"set_interval\"]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"command_params\":{\"set_interval\":[{\"name\":\"value\",\"type\":\"number\"}]}"));
}

void test_format_capabilities_payload_with_battery_metrics(void) {
    ModuleRegistry reg;
    reg.init();
    reg.add_metric("temperature", "\xC2\xB0""C");
    reg.add_metric("humidity", "%");
    reg.add_metric("pressure", "hPa");
    reg.add_metric("battery_pct", "%");
    reg.add_metric("battery_v", "V");
    reg.add_command("set_interval", dummy_cmd_handler);

    char buf[512];
    format_capabilities_payload("ESP-DEADBEEF", 300,
                                reg, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_pct\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_v\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"publish_interval\":300"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_v\":\"V\""));
}

void test_format_capabilities_no_units_no_params(void) {
    ModuleRegistry reg;
    reg.init();
    reg.add_metric("wifi_rssi");
    reg.add_command("set_interval", dummy_cmd_handler);

    char buf[256];
    int len = format_capabilities_payload("ESP-000000", 10,
                                          reg, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NULL(strstr(buf, "\"units\""));
    TEST_ASSERT_NULL(strstr(buf, "\"command_params\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"commands\":[\"set_interval\"]"));
}

// --- Ack payload tests ---

void test_format_ack_payload_ok(void) {
    char buf[96];
    int len = format_ack_payload("set_interval", "ok", buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"action\":\"set_interval\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"status\":\"ok\""));
}

void test_format_ack_payload_error(void) {
    char buf[96];
    int len = format_ack_payload("unknown_cmd", "error", buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"action\":\"unknown_cmd\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"status\":\"error\""));
}

// --- Command parsing tests ---

void test_parse_command_action_set_interval(void) {
    char action[32];
    bool ok = parse_command_action("{\"action\":\"set_interval\",\"value\":30}", action, sizeof(action));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("set_interval", action);
}

void test_parse_command_action_request_capabilities(void) {
    char action[32];
    bool ok = parse_command_action("{\"action\":\"request_capabilities\"}", action, sizeof(action));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("request_capabilities", action);
}

void test_parse_command_action_missing(void) {
    char action[32];
    bool ok = parse_command_action("{\"other\":42}", action, sizeof(action));
    TEST_ASSERT_FALSE(ok);
}

void test_parse_command_value_valid(void) {
    uint32_t value = 0;
    bool ok = parse_command_value("{\"action\":\"set_interval\",\"value\":600}", value);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(600, value);
}

void test_parse_command_value_with_spaces(void) {
    uint32_t value = 0;
    bool ok = parse_command_value("{\"action\":\"set_interval\",\"value\": 120}", value);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(120, value);
}

void test_parse_command_value_missing(void) {
    uint32_t value = 0;
    bool ok = parse_command_value("{\"action\":\"request_capabilities\"}", value);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_command_value_zero_rejected(void) {
    uint32_t value = 0;
    bool ok = parse_command_value("{\"value\":0}", value);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_command_value_too_large(void) {
    uint32_t value = 0;
    bool ok = parse_command_value("{\"value\":100000}", value);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_command_value_max_valid(void) {
    uint32_t value = 0;
    bool ok = parse_command_value("{\"value\":86400}", value);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(86400, value);
}
