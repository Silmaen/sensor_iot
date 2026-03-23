#include <unity.h>

// Battery tests
void test_adc_to_voltage_zero(void);
void test_adc_to_voltage_max(void);
void test_adc_to_voltage_midpoint(void);
void test_voltage_to_soc_full(void);
void test_voltage_to_soc_above_full(void);
void test_voltage_to_soc_empty(void);
void test_voltage_to_soc_below_empty(void);
void test_voltage_to_soc_midpoint(void);
void test_voltage_to_soc_quarter(void);

// Display encoding tests
void test_encode_display_zeros(void);
void test_encode_display_digits_only(void);
void test_encode_display_with_dp1(void);
void test_encode_display_all_dp(void);
void test_encode_display_example_from_spec(void);
void test_encode_temperature_23_5(void);
void test_encode_temperature_0(void);
void test_encode_temperature_99_9(void);
void test_encode_temperature_clamped_high(void);
void test_encode_humidity_65_2(void);
void test_encode_pressure_1013(void);
void test_encode_pressure_987(void);

// MQTT payload tests (legacy format functions)
void test_format_sensor_payload(void);
void test_format_sensor_payload_with_battery(void);
void test_format_status_payload_warning(void);
void test_format_status_payload_ok_no_message(void);
void test_format_status_payload_error(void);
void test_format_capabilities_payload(void);
void test_format_capabilities_payload_with_battery_metrics(void);
void test_parse_command_action_set_interval(void);
void test_parse_command_action_request_capabilities(void);
void test_parse_command_action_missing(void);
void test_parse_command_value_valid(void);
void test_parse_command_value_with_spaces(void);
void test_parse_command_value_missing(void);
void test_parse_command_value_zero_rejected(void);
void test_parse_command_value_too_large(void);
void test_parse_command_value_max_valid(void);

// Module registry tests
void test_registry_init(void);
void test_registry_add_metric(void);
void test_registry_add_metric_overflow(void);
void test_registry_add_command(void);
void test_registry_dispatch_match(void);
void test_registry_dispatch_no_match(void);
void test_registry_dispatch_first_match_wins(void);

// Payload builder tests
void test_payload_builder_empty(void);
void test_payload_builder_single_float(void);
void test_payload_builder_single_uint(void);
void test_payload_builder_multiple_fields(void);
void test_payload_builder_float_decimals(void);

// Module tests
void test_bme280_module_register(void);
void test_bme280_module_contribute(void);
void test_battery_module_register(void);
void test_battery_module_contribute(void);
void test_combined_modules_contribute(void);

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    // Battery
    RUN_TEST(test_adc_to_voltage_zero);
    RUN_TEST(test_adc_to_voltage_max);
    RUN_TEST(test_adc_to_voltage_midpoint);
    RUN_TEST(test_voltage_to_soc_full);
    RUN_TEST(test_voltage_to_soc_above_full);
    RUN_TEST(test_voltage_to_soc_empty);
    RUN_TEST(test_voltage_to_soc_below_empty);
    RUN_TEST(test_voltage_to_soc_midpoint);
    RUN_TEST(test_voltage_to_soc_quarter);

    // Display encoding
    RUN_TEST(test_encode_display_zeros);
    RUN_TEST(test_encode_display_digits_only);
    RUN_TEST(test_encode_display_with_dp1);
    RUN_TEST(test_encode_display_all_dp);
    RUN_TEST(test_encode_display_example_from_spec);
    RUN_TEST(test_encode_temperature_23_5);
    RUN_TEST(test_encode_temperature_0);
    RUN_TEST(test_encode_temperature_99_9);
    RUN_TEST(test_encode_temperature_clamped_high);
    RUN_TEST(test_encode_humidity_65_2);
    RUN_TEST(test_encode_pressure_1013);
    RUN_TEST(test_encode_pressure_987);

    // MQTT payload (legacy)
    RUN_TEST(test_format_sensor_payload);
    RUN_TEST(test_format_sensor_payload_with_battery);
    RUN_TEST(test_format_status_payload_warning);
    RUN_TEST(test_format_status_payload_ok_no_message);
    RUN_TEST(test_format_status_payload_error);
    RUN_TEST(test_format_capabilities_payload);
    RUN_TEST(test_format_capabilities_payload_with_battery_metrics);
    RUN_TEST(test_parse_command_action_set_interval);
    RUN_TEST(test_parse_command_action_request_capabilities);
    RUN_TEST(test_parse_command_action_missing);
    RUN_TEST(test_parse_command_value_valid);
    RUN_TEST(test_parse_command_value_with_spaces);
    RUN_TEST(test_parse_command_value_missing);
    RUN_TEST(test_parse_command_value_zero_rejected);
    RUN_TEST(test_parse_command_value_too_large);
    RUN_TEST(test_parse_command_value_max_valid);

    // Module registry
    RUN_TEST(test_registry_init);
    RUN_TEST(test_registry_add_metric);
    RUN_TEST(test_registry_add_metric_overflow);
    RUN_TEST(test_registry_add_command);
    RUN_TEST(test_registry_dispatch_match);
    RUN_TEST(test_registry_dispatch_no_match);
    RUN_TEST(test_registry_dispatch_first_match_wins);

    // Payload builder
    RUN_TEST(test_payload_builder_empty);
    RUN_TEST(test_payload_builder_single_float);
    RUN_TEST(test_payload_builder_single_uint);
    RUN_TEST(test_payload_builder_multiple_fields);
    RUN_TEST(test_payload_builder_float_decimals);

    // Modules
    RUN_TEST(test_bme280_module_register);
    RUN_TEST(test_bme280_module_contribute);
    RUN_TEST(test_battery_module_register);
    RUN_TEST(test_battery_module_contribute);
    RUN_TEST(test_combined_modules_contribute);

    return UNITY_END();
}
