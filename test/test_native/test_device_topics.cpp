#include <unity.h>
#include <string.h>
#include "device_topics.h"

void test_device_topics_build(void) {
    DeviceTopics t;
    t.build("thermo", "thermo_1");
    TEST_ASSERT_EQUAL_STRING("thermo/thermo_1/sensors", t.sensors);
    TEST_ASSERT_EQUAL_STRING("thermo/thermo_1/status", t.status);
    TEST_ASSERT_EQUAL_STRING("thermo/thermo_1/command", t.command);
    TEST_ASSERT_EQUAL_STRING("thermo/thermo_1/capabilities", t.capabilities);
    TEST_ASSERT_EQUAL_STRING("thermo/thermo_1/ack", t.ack);
    TEST_ASSERT_EQUAL_STRING("thermo/thermo_1/commands", t.commands);
    TEST_ASSERT_EQUAL_STRING("thermo/thermo_1/calibration", t.calibration);
}

void test_device_id_valid_accepts(void) {
    TEST_ASSERT_TRUE(device_id_valid("thermo_1"));
    TEST_ASSERT_TRUE(device_id_valid("thermo-mkr"));
    TEST_ASSERT_TRUE(device_id_valid("ABC123"));
}

void test_device_id_valid_rejects(void) {
    TEST_ASSERT_FALSE(device_id_valid(""));            // empty
    TEST_ASSERT_FALSE(device_id_valid("a b"));         // space
    TEST_ASSERT_FALSE(device_id_valid("a/b"));         // slash (MQTT wildcard risk)
    TEST_ASSERT_FALSE(device_id_valid("a+b"));         // plus
    TEST_ASSERT_FALSE(device_id_valid(nullptr));
}
