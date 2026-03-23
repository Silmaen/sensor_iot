#include <unity.h>
#include <string.h>
#include "module_registry.h"

static bool handler_called = false;
static bool dummy_handler(const char* payload) {
    (void)payload;
    handler_called = true;
    return true;
}

static bool other_handler(const char* payload) {
    (void)payload;
    return false;
}

void test_registry_init(void) {
    ModuleRegistry reg;
    reg.init();
    TEST_ASSERT_EQUAL(0, reg.num_metrics);
    TEST_ASSERT_EQUAL(0, reg.num_commands);
}

void test_registry_add_metric(void) {
    ModuleRegistry reg;
    reg.init();
    TEST_ASSERT_TRUE(reg.add_metric("temperature"));
    TEST_ASSERT_TRUE(reg.add_metric("humidity"));
    TEST_ASSERT_EQUAL(2, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("temperature", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("humidity", reg.metrics[1]);
}

void test_registry_add_metric_overflow(void) {
    ModuleRegistry reg;
    reg.init();
    for (size_t i = 0; i < MAX_METRICS; i++) {
        TEST_ASSERT_TRUE(reg.add_metric("m"));
    }
    TEST_ASSERT_FALSE(reg.add_metric("overflow"));
    TEST_ASSERT_EQUAL(MAX_METRICS, reg.num_metrics);
}

void test_registry_add_command(void) {
    ModuleRegistry reg;
    reg.init();
    TEST_ASSERT_TRUE(reg.add_command("set_interval", dummy_handler));
    TEST_ASSERT_EQUAL(1, reg.num_commands);
    TEST_ASSERT_EQUAL_STRING("set_interval", reg.commands[0]);
}

void test_registry_dispatch_match(void) {
    ModuleRegistry reg;
    reg.init();
    reg.add_command("set_interval", dummy_handler);
    handler_called = false;
    TEST_ASSERT_TRUE(reg.dispatch("set_interval", "{}"));
    TEST_ASSERT_TRUE(handler_called);
}

void test_registry_dispatch_no_match(void) {
    ModuleRegistry reg;
    reg.init();
    reg.add_command("set_interval", dummy_handler);
    handler_called = false;
    TEST_ASSERT_FALSE(reg.dispatch("unknown_cmd", "{}"));
    TEST_ASSERT_FALSE(handler_called);
}

void test_registry_dispatch_first_match_wins(void) {
    ModuleRegistry reg;
    reg.init();
    reg.add_command("cmd", dummy_handler);
    reg.add_command("cmd", other_handler);
    handler_called = false;
    TEST_ASSERT_TRUE(reg.dispatch("cmd", "{}"));
    TEST_ASSERT_TRUE(handler_called);
}
