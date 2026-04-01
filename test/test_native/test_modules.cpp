#include <unity.h>
#include <string.h>
#include "modules/bme280_module.h"
#include "modules/sht30_module.h"
#include "modules/battery_module.h"
#include "modules/mkr_env_module.h"

void test_bme280_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    bme280_module_register(reg);
    TEST_ASSERT_EQUAL(3, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("temp", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("humi", reg.metrics[1]);
    TEST_ASSERT_EQUAL_STRING("press", reg.metrics[2]);
    TEST_ASSERT_NOT_NULL(reg.metric_units[0]); // °C
    TEST_ASSERT_EQUAL_STRING("%", reg.metric_units[1]);
    TEST_ASSERT_EQUAL_STRING("hPa", reg.metric_units[2]);
}

void test_bme280_module_contribute(void) {
    SensorData data = {22.5f, 45.2f, 1013.1f, 0.0f, 0.0f, true};
    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    bme280_module_contribute(pb, data);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":22.5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"humi\":45.2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"press\":1013.1"));
}

void test_battery_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    battery_module_register(reg);
    TEST_ASSERT_EQUAL(2, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("bat", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("batv", reg.metrics[1]);
    TEST_ASSERT_EQUAL_STRING("%", reg.metric_units[0]);
    TEST_ASSERT_EQUAL_STRING("V", reg.metric_units[1]);
    TEST_ASSERT_EQUAL(1, reg.num_commands);
    TEST_ASSERT_EQUAL_STRING("calibrate_battery", reg.commands[0]);
}

void test_battery_module_contribute(void) {
    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    battery_module_contribute(pb, 75, 7.8f);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bat\":75"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"batv\":7.80"));
}

void test_sht30_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    sht30_module_register(reg);
    TEST_ASSERT_EQUAL(2, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("temp", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("humi", reg.metrics[1]);
    TEST_ASSERT_NOT_NULL(reg.metric_units[0]); // °C
    TEST_ASSERT_EQUAL_STRING("%", reg.metric_units[1]);
}

void test_sht30_module_contribute(void) {
    SensorData data = {23.7f, 52.3f, 0.0f, 0.0f, 0.0f, true};
    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    sht30_module_contribute(pb, data);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":23.7"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"humi\":52.3"));
    // SHT30 does not contribute pressure
    TEST_ASSERT_NULL(strstr(buf, "\"press\""));
}

void test_sht30_module_with_battery(void) {
    SensorData data = {21.0f, 60.0f, 0.0f, 0.0f, 0.0f, true};
    char buf[256];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    sht30_module_contribute(pb, data);
    battery_module_contribute(pb, 80, 7.9f);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":21.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"humi\":60.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bat\":80"));
    TEST_ASSERT_EQUAL('{', buf[0]);
    TEST_ASSERT_EQUAL('}', buf[strlen(buf) - 1]);
}

void test_mkr_env_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    mkr_env_module_register(reg);
    TEST_ASSERT_EQUAL(5, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("temp", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("humi", reg.metrics[1]);
    TEST_ASSERT_EQUAL_STRING("press", reg.metrics[2]);
    TEST_ASSERT_EQUAL_STRING("lux", reg.metrics[3]);
    TEST_ASSERT_EQUAL_STRING("uv", reg.metrics[4]);
    TEST_ASSERT_NOT_NULL(reg.metric_units[0]); // °C
    TEST_ASSERT_EQUAL_STRING("%", reg.metric_units[1]);
    TEST_ASSERT_EQUAL_STRING("hPa", reg.metric_units[2]);
    TEST_ASSERT_EQUAL_STRING("lux", reg.metric_units[3]);
}

void test_mkr_env_module_contribute(void) {
    SensorData data = {21.3f, 55.8f, 1015.2f, 150.0f, 3.45f, true};
    char buf[256];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    mkr_env_module_contribute(pb, data);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":21.3"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"humi\":55.8"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"press\":1015.2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"lux\":150"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"uv\":3.45"));
}

void test_mkr_env_module_with_battery(void) {
    SensorData data = {20.0f, 65.0f, 1010.0f, 200.0f, 1.50f, true};
    char buf[256];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    mkr_env_module_contribute(pb, data);
    battery_module_contribute(pb, 90, 4.1f);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":20.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"humi\":65.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"press\":1010.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"lux\":200"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"uv\":1.50"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bat\":90"));
    TEST_ASSERT_EQUAL('{', buf[0]);
    TEST_ASSERT_EQUAL('}', buf[strlen(buf) - 1]);
}

void test_combined_modules_contribute(void) {
    SensorData data = {22.0f, 50.0f, 1000.0f, 0.0f, 0.0f, true};
    char buf[256];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    bme280_module_contribute(pb, data);
    battery_module_contribute(pb, 60, 7.4f);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":22.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bat\":60"));
    TEST_ASSERT_EQUAL('{', buf[0]);
    TEST_ASSERT_EQUAL('}', buf[strlen(buf) - 1]);
}
