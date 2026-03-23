#include <unity.h>
#include <string.h>
#include "mqtt_payload.h"

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

void test_format_capabilities_payload(void) {
    const char* metrics[] = {"temperature", "humidity", "pressure"};
    const char* commands[] = {"set_interval"};
    char buf[256];
    int len = format_capabilities_payload("ESP-ABC123", 60,
                                          metrics, 3, commands, 1,
                                          buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hardware_id\":\"ESP-ABC123\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"publish_interval\":60"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"metrics\":[\"temperature\",\"humidity\",\"pressure\"]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"commands\":[\"set_interval\"]"));
}

void test_format_capabilities_payload_with_battery_metrics(void) {
    const char* metrics[] = {"temperature", "humidity", "pressure",
                             "battery_pct", "battery_v"};
    const char* commands[] = {"set_interval"};
    char buf[320];
    format_capabilities_payload("ESP-DEADBEEF", 300,
                                metrics, 5, commands, 1,
                                buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_pct\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_v\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"publish_interval\":300"));
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
