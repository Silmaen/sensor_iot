#include <unity.h>
#include <string.h>
#include "modules/bme280_module.h"
#include "modules/sht30_module.h"
#include "modules/battery_module.h"
#include "modules/mkr_env_module.h"
#include "modules/relay_module.h"
#include "modules/light_module.h"
#include "modules/calibration_module.h"

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

// --- Calibration module tests ---

static float persisted_temp = 0, persisted_humi = 0, persisted_press = 0;
static int persist_count = 0;

static void mock_persist(float t, float h, float p) {
    persisted_temp = t;
    persisted_humi = h;
    persisted_press = p;
    persist_count++;
}

void test_calibration_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    calibration_module_register(reg);
    TEST_ASSERT_EQUAL(0, reg.num_metrics);
    TEST_ASSERT_EQUAL(1, reg.num_commands);
    TEST_ASSERT_EQUAL_STRING("set_offset", reg.commands[0]);
}

void test_calibration_apply_zero_offsets(void) {
    calibration_module_reset();
    SensorData data = {22.5f, 45.0f, 1013.0f, 0.0f, 0.0f, true};
    calibration_apply(data);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.5f, data.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, data.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.0f, data.pressure);
}

void test_calibration_apply_temp_offset(void) {
    calibration_module_reset();
    calibration_module_set_offsets(-1.5f, 0.0f, 0.0f);
    SensorData data = {22.5f, 45.0f, 1013.0f, 0.0f, 0.0f, true};
    calibration_apply(data);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.0f, data.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, data.humidity);
}

void test_calibration_apply_all_offsets(void) {
    calibration_module_reset();
    calibration_module_set_offsets(-0.5f, 3.0f, -1.2f);
    SensorData data = {20.0f, 50.0f, 1010.0f, 0.0f, 0.0f, true};
    calibration_apply(data);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 19.5f, data.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 53.0f, data.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1008.8f, data.pressure);
}

void test_calibration_clamp_humidity(void) {
    calibration_module_reset();
    calibration_module_set_offsets(0.0f, -60.0f, 0.0f);
    SensorData data = {20.0f, 30.0f, 1010.0f, 0.0f, 0.0f, true};
    calibration_apply(data);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, data.humidity);
}

void test_calibration_set_offset_command_temp(void) {
    calibration_module_reset();
    persist_count = 0;
    calibration_module_set_persist_callback(mock_persist);
    ModuleRegistry reg;
    reg.init();
    calibration_module_register(reg);

    bool ok = reg.dispatch("set_offset",
        "{\"action\":\"set_offset\",\"metric\":\"temp\",\"value\":-0.5}");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, calibration_get_temp_offset());
    TEST_ASSERT_EQUAL(1, persist_count);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, persisted_temp);
}

void test_calibration_set_offset_command_humi(void) {
    calibration_module_reset();
    ModuleRegistry reg;
    reg.init();
    calibration_module_register(reg);

    bool ok = reg.dispatch("set_offset",
        "{\"action\":\"set_offset\",\"metric\":\"humi\",\"value\":2.3}");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.3f, calibration_get_humi_offset());
}

void test_calibration_set_offset_command_press(void) {
    calibration_module_reset();
    ModuleRegistry reg;
    reg.init();
    calibration_module_register(reg);

    bool ok = reg.dispatch("set_offset",
        "{\"action\":\"set_offset\",\"metric\":\"press\",\"value\":-1.0}");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, calibration_get_press_offset());
}

void test_calibration_set_offset_invalid_metric(void) {
    calibration_module_reset();
    ModuleRegistry reg;
    reg.init();
    calibration_module_register(reg);

    bool ok = reg.dispatch("set_offset",
        "{\"action\":\"set_offset\",\"metric\":\"foo\",\"value\":1.0}");
    TEST_ASSERT_FALSE(ok);
}

void test_calibration_set_offset_extreme_rejected(void) {
    calibration_module_reset();
    ModuleRegistry reg;
    reg.init();
    calibration_module_register(reg);

    bool ok = reg.dispatch("set_offset",
        "{\"action\":\"set_offset\",\"metric\":\"temp\",\"value\":99.0}");
    TEST_ASSERT_FALSE(ok);
}

void test_calibration_set_offset_zero(void) {
    calibration_module_reset();
    calibration_module_set_offsets(1.0f, 0.0f, 0.0f);
    ModuleRegistry reg;
    reg.init();
    calibration_module_register(reg);

    bool ok = reg.dispatch("set_offset",
        "{\"action\":\"set_offset\",\"metric\":\"temp\",\"value\":0}");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, calibration_get_temp_offset());
}

// --- Light module tests ---

void test_light_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    light_module_register(reg);
    TEST_ASSERT_EQUAL(1, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("light", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("%", reg.metric_units[0]);
}

void test_light_module_contribute(void) {
    char buf[64];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    light_module_contribute(pb, 72);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"light\":72"));
}

void test_adc_to_light_pct_zero(void) {
    TEST_ASSERT_EQUAL(0, adc_to_light_pct(0, 1023));
}

void test_adc_to_light_pct_max(void) {
    TEST_ASSERT_EQUAL(100, adc_to_light_pct(1023, 1023));
}

void test_adc_to_light_pct_above_max(void) {
    TEST_ASSERT_EQUAL(100, adc_to_light_pct(1100, 1023));
}

void test_adc_to_light_pct_midpoint(void) {
    TEST_ASSERT_EQUAL(50, adc_to_light_pct(512, 1023));
}

void test_adc_to_light_pct_12bit(void) {
    TEST_ASSERT_EQUAL(50, adc_to_light_pct(2048, 4095));
}

void test_adc_to_light_pct_zero_max(void) {
    TEST_ASSERT_EQUAL(0, adc_to_light_pct(500, 0));
}

// --- Relay module tests ---

// Track relay writes for testing
static uint8_t last_relay_num = 0;
static bool last_relay_active = false;
static int relay_write_count = 0;

static void mock_relay_writer(uint8_t relay_num, bool active) {
    last_relay_num = relay_num;
    last_relay_active = active;
    relay_write_count++;
}

static void reset_relay_mocks() {
    last_relay_num = 0;
    last_relay_active = false;
    relay_write_count = 0;
    relay_module_reset();
    relay_module_set_writer(mock_relay_writer);
}

void test_relay_module_register(void) {
    ModuleRegistry reg;
    reg.init();
    relay_module_register(reg);
    TEST_ASSERT_EQUAL(2, reg.num_metrics);
    TEST_ASSERT_EQUAL_STRING("relay1", reg.metrics[0]);
    TEST_ASSERT_EQUAL_STRING("relay2", reg.metrics[1]);
    TEST_ASSERT_EQUAL(2, reg.num_commands);
    TEST_ASSERT_EQUAL_STRING("relay_toggle", reg.commands[0]);
    TEST_ASSERT_EQUAL_STRING("relay_contact", reg.commands[1]);
}

void test_relay_module_contribute_initial(void) {
    reset_relay_mocks();
    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    relay_module_contribute(pb);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"relay1\":0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"relay2\":0"));
}

void test_relay_toggle_on(void) {
    reset_relay_mocks();
    ModuleRegistry reg;
    reg.init();
    relay_module_register(reg);
    bool ok = reg.dispatch("relay_toggle", "{\"action\":\"relay_toggle\",\"value\":1}");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(1, last_relay_num);
    TEST_ASSERT_TRUE(last_relay_active);

    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    relay_module_contribute(pb);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"relay1\":1"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"relay2\":0"));
}

void test_relay_toggle_off(void) {
    reset_relay_mocks();
    ModuleRegistry reg;
    reg.init();
    relay_module_register(reg);
    // Toggle on then off
    reg.dispatch("relay_toggle", "{\"action\":\"relay_toggle\",\"value\":2}");
    reg.dispatch("relay_toggle", "{\"action\":\"relay_toggle\",\"value\":2}");
    TEST_ASSERT_EQUAL(2, last_relay_num);
    TEST_ASSERT_FALSE(last_relay_active);
}

void test_relay_toggle_invalid_relay(void) {
    reset_relay_mocks();
    ModuleRegistry reg;
    reg.init();
    relay_module_register(reg);
    bool ok = reg.dispatch("relay_toggle", "{\"action\":\"relay_toggle\",\"value\":3}");
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(0, relay_write_count);
}

void test_relay_contact_activates_and_reverts(void) {
    reset_relay_mocks();
    ModuleRegistry reg;
    reg.init();
    relay_module_register(reg);

    bool ok = reg.dispatch("relay_contact",
        "{\"action\":\"relay_contact\",\"relay\":1,\"value\":1000}");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(1, last_relay_num);
    TEST_ASSERT_TRUE(last_relay_active);

    // Tick at t=500ms — relay should stay on
    relay_write_count = 0;
    bool changed = relay_module_tick(500);  // resolves delay to deadline=1500
    TEST_ASSERT_FALSE(changed);

    // Tick at t=1400ms — still on
    changed = relay_module_tick(1400);
    TEST_ASSERT_FALSE(changed);

    // Tick at t=1500ms — deadline reached, should revert
    changed = relay_module_tick(1500);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(1, last_relay_num);
    TEST_ASSERT_FALSE(last_relay_active);

    // Verify payload
    char buf[128];
    PayloadBuilder pb{buf, sizeof(buf), 0, true};
    pb.begin();
    relay_module_contribute(pb);
    pb.end();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"relay1\":0"));
}

void test_relay_contact_invalid_delay(void) {
    reset_relay_mocks();
    ModuleRegistry reg;
    reg.init();
    relay_module_register(reg);

    // delay > 300000 ms
    bool ok = reg.dispatch("relay_contact",
        "{\"action\":\"relay_contact\",\"relay\":1,\"value\":999999}");
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(0, relay_write_count);
}

void test_relay_toggle_cancels_contact(void) {
    reset_relay_mocks();
    ModuleRegistry reg;
    reg.init();
    relay_module_register(reg);

    // Start a contact
    reg.dispatch("relay_contact",
        "{\"action\":\"relay_contact\",\"relay\":1,\"value\":5000}");
    TEST_ASSERT_TRUE(last_relay_active);

    // Toggle should cancel the contact timer and turn off
    reg.dispatch("relay_toggle", "{\"action\":\"relay_toggle\",\"value\":1}");
    TEST_ASSERT_FALSE(last_relay_active);

    // Tick past the original deadline — should NOT re-trigger
    relay_write_count = 0;
    bool changed = relay_module_tick(10000);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL(0, relay_write_count);
}
