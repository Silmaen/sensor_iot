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
void test_voltage_to_soc_monotonic(void);
void test_voltage_to_soc_below_linear_on_plateau(void);
void test_voltage_to_soc_reaches_full_at_ceiling(void);
void test_battery_calibrate_set_ratio(void);
void test_battery_calibrate(void);
void test_battery_calibrate_zero_adc(void);

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
void test_format_capabilities_ota_cal_flags_off(void);
void test_format_capabilities_no_units(void);
void test_format_commands_payload(void);
void test_format_commands_payload_no_params(void);
void test_format_ack_payload_ok(void);
void test_format_ack_payload_error(void);
void test_parse_command_action_set_interval(void);
void test_parse_command_action_request_capabilities(void);
void test_parse_command_action_missing(void);
void test_parse_command_value_valid(void);
void test_parse_command_value_with_spaces(void);
void test_parse_command_value_missing(void);
void test_parse_command_value_zero_rejected(void);
void test_parse_command_value_too_large(void);
void test_parse_command_value_max_valid(void);
void test_parse_command_float_value_valid(void);
void test_parse_command_float_value_integer(void);
void test_parse_command_float_value_missing(void);
void test_parse_command_float_value_zero_rejected(void);
void test_parse_command_float_value_negative_rejected(void);
void test_parse_command_string_value_url(void);
void test_parse_command_string_value_other_key(void);
void test_parse_command_string_value_with_spaces(void);
void test_parse_command_string_value_missing(void);
void test_parse_command_string_value_not_a_string(void);
void test_parse_command_string_value_overflow_rejected(void);

// HW identity tests
void test_hw_code_valid_accepts_8_alnum(void);
void test_hw_code_valid_rejects_wrong_length(void);
void test_hw_code_valid_rejects_bad_chars(void);
void test_hw_code_valid_null(void);

// Config store tests
void test_config_store_str_roundtrip(void);
void test_config_store_float_roundtrip(void);
void test_config_store_missing_key(void);
void test_config_store_overwrite(void);
void test_config_store_type_mismatch(void);
void test_config_store_get_str_overflow(void);
void test_config_store_set_str_too_long(void);
void test_config_store_remove(void);
void test_config_store_clear(void);
void test_config_store_full(void);
void test_config_calibration_present_flag(void);

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
void test_sht30_module_register(void);
void test_sht30_module_contribute(void);
void test_sht30_module_with_battery(void);
void test_mkr_env_module_register(void);
void test_mkr_env_module_contribute(void);
void test_mkr_env_module_with_battery(void);
void test_battery_module_register(void);
void test_battery_module_contribute(void);
void test_combined_modules_contribute(void);
void test_bh1750_module_register(void);
void test_bh1750_module_contribute(void);
void test_bh1750_module_contribute_zero(void);
void test_bh1750_with_sht30(void);
void test_calibration_module_register(void);
void test_calibration_apply_zero_offsets(void);
void test_calibration_apply_temp_offset(void);
void test_calibration_apply_all_offsets(void);
void test_calibration_clamp_humidity(void);
void test_calibration_set_offset_command_temp(void);
void test_calibration_set_offset_command_humi(void);
void test_calibration_set_offset_command_press(void);
void test_calibration_set_offset_invalid_metric(void);
void test_calibration_set_offset_extreme_rejected(void);
void test_calibration_request_calibration(void);
void test_calibration_format_response(void);
void test_calibration_set_calibration_bat_divider(void);
void test_calibration_set_calibration_unknown_key(void);
void test_calibration_set_calibration_invalid_ratio(void);
void test_calibration_set_offset_zero(void);
void test_light_module_register(void);
void test_light_module_contribute(void);
void test_adc_to_light_pct_zero(void);
void test_adc_to_light_pct_max(void);
void test_adc_to_light_pct_above_max(void);
void test_adc_to_light_pct_midpoint(void);
void test_adc_to_light_pct_12bit(void);
void test_adc_to_light_pct_zero_max(void);
void test_relay_module_register(void);
void test_relay_module_contribute_initial(void);
void test_relay_toggle_on(void);
void test_relay_toggle_off(void);
void test_relay_toggle_invalid_relay(void);
void test_relay_contact_activates_and_reverts(void);
void test_relay_contact_invalid_delay(void);
void test_relay_toggle_cancels_contact(void);

// OTA module tests
void test_ota_module_register(void);
void test_ota_happy_path(void);
void test_ota_hw_code_mismatch(void);
void test_ota_hw_rev_mismatch(void);
void test_ota_same_version_rejected(void);
void test_ota_low_battery_rejected(void);
void test_ota_battery_ok_proceeds(void);
void test_ota_no_battery_provider_skips_guard(void);
void test_ota_performer_failure_acked(void);
void test_ota_bad_request(void);

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
    RUN_TEST(test_voltage_to_soc_monotonic);
    RUN_TEST(test_voltage_to_soc_below_linear_on_plateau);
    RUN_TEST(test_voltage_to_soc_reaches_full_at_ceiling);
    RUN_TEST(test_battery_calibrate_set_ratio);
    RUN_TEST(test_battery_calibrate);
    RUN_TEST(test_battery_calibrate_zero_adc);

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
    RUN_TEST(test_format_capabilities_ota_cal_flags_off);
    RUN_TEST(test_format_capabilities_no_units);
    RUN_TEST(test_format_commands_payload);
    RUN_TEST(test_format_commands_payload_no_params);
    RUN_TEST(test_format_ack_payload_ok);
    RUN_TEST(test_format_ack_payload_error);
    RUN_TEST(test_parse_command_action_set_interval);
    RUN_TEST(test_parse_command_action_request_capabilities);
    RUN_TEST(test_parse_command_action_missing);
    RUN_TEST(test_parse_command_value_valid);
    RUN_TEST(test_parse_command_value_with_spaces);
    RUN_TEST(test_parse_command_value_missing);
    RUN_TEST(test_parse_command_value_zero_rejected);
    RUN_TEST(test_parse_command_value_too_large);
    RUN_TEST(test_parse_command_value_max_valid);
    RUN_TEST(test_parse_command_float_value_valid);
    RUN_TEST(test_parse_command_float_value_integer);
    RUN_TEST(test_parse_command_float_value_missing);
    RUN_TEST(test_parse_command_float_value_zero_rejected);
    RUN_TEST(test_parse_command_float_value_negative_rejected);
    RUN_TEST(test_parse_command_string_value_url);
    RUN_TEST(test_parse_command_string_value_other_key);
    RUN_TEST(test_parse_command_string_value_with_spaces);
    RUN_TEST(test_parse_command_string_value_missing);
    RUN_TEST(test_parse_command_string_value_not_a_string);
    RUN_TEST(test_parse_command_string_value_overflow_rejected);

    // HW identity
    RUN_TEST(test_hw_code_valid_accepts_8_alnum);
    RUN_TEST(test_hw_code_valid_rejects_wrong_length);
    RUN_TEST(test_hw_code_valid_rejects_bad_chars);
    RUN_TEST(test_hw_code_valid_null);

    // Config store
    RUN_TEST(test_config_store_str_roundtrip);
    RUN_TEST(test_config_store_float_roundtrip);
    RUN_TEST(test_config_store_missing_key);
    RUN_TEST(test_config_store_overwrite);
    RUN_TEST(test_config_store_type_mismatch);
    RUN_TEST(test_config_store_get_str_overflow);
    RUN_TEST(test_config_store_set_str_too_long);
    RUN_TEST(test_config_store_remove);
    RUN_TEST(test_config_store_clear);
    RUN_TEST(test_config_store_full);
    RUN_TEST(test_config_calibration_present_flag);

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
    RUN_TEST(test_sht30_module_register);
    RUN_TEST(test_sht30_module_contribute);
    RUN_TEST(test_sht30_module_with_battery);
    RUN_TEST(test_mkr_env_module_register);
    RUN_TEST(test_mkr_env_module_contribute);
    RUN_TEST(test_mkr_env_module_with_battery);
    RUN_TEST(test_battery_module_register);
    RUN_TEST(test_battery_module_contribute);
    RUN_TEST(test_combined_modules_contribute);

    // BH1750 module
    RUN_TEST(test_bh1750_module_register);
    RUN_TEST(test_bh1750_module_contribute);
    RUN_TEST(test_bh1750_module_contribute_zero);
    RUN_TEST(test_bh1750_with_sht30);

    // Calibration module
    RUN_TEST(test_calibration_module_register);
    RUN_TEST(test_calibration_apply_zero_offsets);
    RUN_TEST(test_calibration_apply_temp_offset);
    RUN_TEST(test_calibration_apply_all_offsets);
    RUN_TEST(test_calibration_clamp_humidity);
    RUN_TEST(test_calibration_set_offset_command_temp);
    RUN_TEST(test_calibration_set_offset_command_humi);
    RUN_TEST(test_calibration_set_offset_command_press);
    RUN_TEST(test_calibration_set_offset_invalid_metric);
    RUN_TEST(test_calibration_set_offset_extreme_rejected);
    RUN_TEST(test_calibration_request_calibration);
    RUN_TEST(test_calibration_format_response);
    RUN_TEST(test_calibration_set_calibration_bat_divider);
    RUN_TEST(test_calibration_set_calibration_unknown_key);
    RUN_TEST(test_calibration_set_calibration_invalid_ratio);
    RUN_TEST(test_calibration_set_offset_zero);

    // Light module
    RUN_TEST(test_light_module_register);
    RUN_TEST(test_light_module_contribute);
    RUN_TEST(test_adc_to_light_pct_zero);
    RUN_TEST(test_adc_to_light_pct_max);
    RUN_TEST(test_adc_to_light_pct_above_max);
    RUN_TEST(test_adc_to_light_pct_midpoint);
    RUN_TEST(test_adc_to_light_pct_12bit);
    RUN_TEST(test_adc_to_light_pct_zero_max);

    // Relay module
    RUN_TEST(test_relay_module_register);
    RUN_TEST(test_relay_module_contribute_initial);
    RUN_TEST(test_relay_toggle_on);
    RUN_TEST(test_relay_toggle_off);
    RUN_TEST(test_relay_toggle_invalid_relay);
    RUN_TEST(test_relay_contact_activates_and_reverts);
    RUN_TEST(test_relay_contact_invalid_delay);
    RUN_TEST(test_relay_toggle_cancels_contact);

    // OTA module
    RUN_TEST(test_ota_module_register);
    RUN_TEST(test_ota_happy_path);
    RUN_TEST(test_ota_hw_code_mismatch);
    RUN_TEST(test_ota_hw_rev_mismatch);
    RUN_TEST(test_ota_same_version_rejected);
    RUN_TEST(test_ota_low_battery_rejected);
    RUN_TEST(test_ota_battery_ok_proceeds);
    RUN_TEST(test_ota_no_battery_provider_skips_guard);
    RUN_TEST(test_ota_performer_failure_acked);
    RUN_TEST(test_ota_bad_request);

    return UNITY_END();
}
