#include <unity.h>
#include <string.h>
#include "payload_builder.h"

void test_payload_builder_empty(void) {
    char buf[64];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    int len = pb.end();
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_STRING("{}", buf);
}

void test_payload_builder_single_float(void) {
    char buf[64];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    pb.add_float("temp", 22.5f, 1);
    pb.end();
    TEST_ASSERT_EQUAL_STRING("{\"temp\":22.5}", buf);
}

void test_payload_builder_single_uint(void) {
    char buf[64];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    pb.add_uint("bat", 75);
    pb.end();
    TEST_ASSERT_EQUAL_STRING("{\"bat\":75}", buf);
}

void test_payload_builder_multiple_fields(void) {
    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    pb.add_float("temp", 22.5f, 1);
    pb.add_float("humi", 45.2f, 1);
    pb.add_uint("bat", 80);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":22.5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",\"humi\":45.2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",\"bat\":80"));
    // Starts with { and ends with }
    TEST_ASSERT_EQUAL('{', buf[0]);
    TEST_ASSERT_EQUAL('}', buf[strlen(buf) - 1]);
}

void test_payload_builder_float_decimals(void) {
    char buf[64];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    pb.add_float("voltage", 7.8f, 2);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"voltage\":7.80"));
}
