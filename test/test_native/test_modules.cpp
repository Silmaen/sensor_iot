#include <unity.h>
#include <string.h>
#include "modules/bme280_module.h"
#include "modules/battery_module.h"

void test_bme280_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    bme280_module_register(reg);
    TEST_ASSERT_EQUAL(3, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("temperature", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("humidity", reg.metrics[1]);
    TEST_ASSERT_EQUAL_STRING("pressure", reg.metrics[2]);
    TEST_ASSERT_NOT_NULL(reg.metric_units[0]); // °C
    TEST_ASSERT_EQUAL_STRING("%", reg.metric_units[1]);
    TEST_ASSERT_EQUAL_STRING("hPa", reg.metric_units[2]);
}

void test_bme280_module_contribute(void) {
    SensorData data = {22.5f, 45.2f, 1013.1f, true};
    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    bme280_module_contribute(pb, data);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temperature\":22.5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"humidity\":45.2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pressure\":1013.1"));
}

void test_battery_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    battery_module_register(reg);
    TEST_ASSERT_EQUAL(2, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("battery_pct", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("battery_v", reg.metrics[1]);
    TEST_ASSERT_EQUAL_STRING("%", reg.metric_units[0]);
    TEST_ASSERT_EQUAL_STRING("V", reg.metric_units[1]);
}

void test_battery_module_contribute(void) {
    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    battery_module_contribute(pb, 75, 7.8f);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_pct\":75"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_v\":7.80"));
}

void test_combined_modules_contribute(void) {
    SensorData data = {22.0f, 50.0f, 1000.0f, true};
    char buf[256];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    bme280_module_contribute(pb, data);
    battery_module_contribute(pb, 60, 7.4f);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temperature\":22.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery_pct\":60"));
    TEST_ASSERT_EQUAL('{', buf[0]);
    TEST_ASSERT_EQUAL('}', buf[strlen(buf) - 1]);
}
