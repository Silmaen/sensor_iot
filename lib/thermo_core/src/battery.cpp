#include "battery.h"

static float divider_ratio = BATTERY_DIVIDER_RATIO;

float adc_to_voltage(uint16_t adc_value) {
    float adc_voltage = (static_cast<float>(adc_value) / ADC_MAX_VALUE) * ADC_REF_VOLTAGE;
    return adc_voltage * divider_ratio;
}

// Empirical Li-ion/LiPo discharge curve: open-circuit-ish voltage *per cell*
// mapped to remaining charge. Derived from real 17-18 day discharge traces of
// the 2S field devices (SoC ~= 100% - fraction of runtime elapsed at roughly
// constant load). It captures the flat mid plateau and the steep end-of-life
// cliff that a linear model completely misses. Descending by voltage.
struct SocPoint {
    float v_cell;
    float soc;
};
static const SocPoint LIPO_CURVE[] = {
    {4.08f, 100.0f}, {4.03f, 94.0f}, {3.98f, 88.0f}, {3.93f, 82.0f},
    {3.89f, 76.0f},  {3.84f, 70.0f}, {3.81f, 63.0f}, {3.79f, 57.0f},
    {3.77f, 51.0f},  {3.76f, 45.0f}, {3.74f, 39.0f}, {3.71f, 33.0f},
    {3.66f, 27.0f},  {3.57f, 22.0f}, {3.47f, 18.0f}, {3.37f, 14.0f},
    {3.27f, 10.0f},  {3.17f, 5.0f},  {3.10f, 2.0f},  {3.00f, 0.0f},
};

// Interpolate the per-cell curve for a given cell voltage (0-100).
static float curve_soc(float v_cell) {
    const int n = sizeof(LIPO_CURVE) / sizeof(LIPO_CURVE[0]);
    if (v_cell >= LIPO_CURVE[0].v_cell) return 100.0f;
    if (v_cell <= LIPO_CURVE[n - 1].v_cell) return 0.0f;
    for (int i = 0; i < n - 1; ++i) {
        float v_hi = LIPO_CURVE[i].v_cell;
        float v_lo = LIPO_CURVE[i + 1].v_cell;
        if (v_cell <= v_hi && v_cell >= v_lo) {
            float t = (v_cell - v_lo) / (v_hi - v_lo);
            return LIPO_CURVE[i + 1].soc + t * (LIPO_CURVE[i].soc - LIPO_CURVE[i + 1].soc);
        }
    }
    return 0.0f;
}

uint8_t voltage_to_soc(float voltage) {
    if (voltage >= BATTERY_VOLTAGE_FULL) return 100;
    if (voltage <= BATTERY_VOLTAGE_EMPTY) return 0;

    // Map through the non-linear curve, then rescale so that the configured
    // empty/full pack voltages read exactly 0% / 100% on this hardware.
    float raw = curve_soc(voltage / BATTERY_CELLS);
    float lo = curve_soc(BATTERY_VOLTAGE_EMPTY / BATTERY_CELLS);
    float hi = curve_soc(BATTERY_VOLTAGE_FULL / BATTERY_CELLS);
    if (hi <= lo) return 0; // guard against misconfigured bounds

    float pct = (raw - lo) / (hi - lo) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return static_cast<uint8_t>(pct + 0.5f);
}

float battery_calibrate(float measured_voltage, uint16_t adc_value) {
    if (adc_value == 0) return divider_ratio;
    float pin_voltage = (static_cast<float>(adc_value) / ADC_MAX_VALUE) * ADC_REF_VOLTAGE;
    divider_ratio = measured_voltage / pin_voltage;
    return divider_ratio;
}

void battery_calibrate_set_ratio(float ratio) {
    if (ratio > 0.0f)
        divider_ratio = ratio;
}

float battery_get_divider_ratio() {
    return divider_ratio;
}
