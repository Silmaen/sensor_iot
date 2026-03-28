// Cell Tester Firmware for Arduino MKR WiFi 1010
//
// Tests reclaimed 18650 cells using the MKR's built-in:
//   - BQ24195L PMIC (charges via USB, pre-conditions deeply discharged cells)
//   - ADC_BATTERY pin (reads battery voltage through on-board voltage divider)
//
// Workflow:
//   1. Insert cell into JST connector
//   2. Firmware reads OCV and classifies the cell
//   3. With USB connected, PMIC charges automatically — firmware monitors progress
//   4. Once charged, connect external load resistor and start discharge test
//   5. Firmware integrates current over time and reports capacity + grade
//
// Serial commands:
//   N  - New cell (reset for next cell)
//   D  - Start discharge test (load resistor must be connected)
//   S  - Stop discharge test
//   R  - Set load resistance in ohms (default 10)
//   H  - Help
//
// No WiFi or MQTT — serial console only.
//
// Only compiled when CELL_TESTER is defined (env:cell_tester in platformio.ini).

#ifdef CELL_TESTER

#include <Arduino.h>
#include <Wire.h>

// --- BQ24195L I2C registers ---
static constexpr uint8_t BQ24195_ADDR = 0x6B;
static constexpr uint8_t REG_PON_CFG  = 0x01; // Power-On Configuration
static constexpr uint8_t REG_CHARGE_I = 0x02; // Charge Current Control
static constexpr uint8_t REG_PCHARGE  = 0x03; // Pre-Charge / Termination Current
static constexpr uint8_t REG_CHARGE_V = 0x04; // Charge Voltage Control
static constexpr uint8_t REG_TIMER    = 0x05; // Charge Termination / Timer Control
static constexpr uint8_t REG_STATUS   = 0x08; // System Status Register
static constexpr uint8_t REG_FAULT    = 0x09; // Fault Register

// --- ADC_BATTERY voltage divider ---
// On-board divider: R_top=330, R_bottom=1200 → ratio = 1200/(1200+330) = 0.784
// V_battery = V_adc / 0.784 = V_adc * 1.276
static constexpr float ADC_DIVIDER_FACTOR = (1200.0f + 330.0f) / 1200.0f; // ~1.275
static constexpr int ADC_MAX_VAL          = 4095;
static constexpr float ADC_REF_V          = 3.3f;

// --- Cell voltage thresholds ---
static constexpr float V_NO_CELL      = 0.5f;  // Below this: no cell detected
static constexpr float V_DEAD         = 1.0f;  // Below this: dead cell
static constexpr float V_DEEP_DISCH   = 2.5f;  // Below this: deep discharged
static constexpr float V_DISCHARGED   = 3.6f;  // Below this: discharged but OK
static constexpr float V_FULL         = 4.15f; // Above this: fully charged
static constexpr float V_CUTOFF       = 2.8f;  // Discharge test stop voltage
static constexpr float V_DROP_REMOVAL = 0.5f;  // Sudden drop threshold → cell removed

// --- Timing ---
static constexpr unsigned long PRINT_INTERVAL_MS   = 2000;
static constexpr unsigned long DISCHARGE_SAMPLE_MS = 5000;
static constexpr unsigned long OCV_SETTLE_MS       = 3000;
static constexpr unsigned long CHARGE_CHECK_MS     = 60000; // Charge health check every 60s

// --- State machine ---
enum class State : uint8_t {
    IDLE,           // No cell or waiting
    OCV_MEASURE,    // Cell detected, settling OCV
    MONITORING,     // Displaying voltage, charging if USB connected
    DISCHARGE_TEST, // Discharge in progress, integrating mAh
    TEST_COMPLETE,  // Results displayed
};

static State state               = State::IDLE;
static float load_resistance_ohm = 7.3f;

// OCV measurement
static unsigned long ocv_start_ms = 0;
static float ocv_voltage          = 0.0f;

// Discharge test
static unsigned long discharge_start_ms = 0;
static unsigned long last_sample_ms     = 0;
static float last_sample_voltage        = 0.0f;
static float accumulated_mah            = 0.0f;
static float start_voltage              = 0.0f;

// Charge monitoring
static unsigned long charge_start_ms      = 0;
static float charge_start_voltage         = 0.0f;
static float charge_last_check_voltage    = 0.0f;
static unsigned long charge_last_check_ms = 0;
static float charge_max_voltage           = 0.0f;
static bool charge_warned                 = false;
static bool charge_complete_seen          = false; // Hysteresis: true once charge complete

// Last known voltage (for removal detection)
static float last_known_voltage = 0.0f;

// Cell counter
static uint8_t cell_number = 1;

// --- Read battery voltage (EMA-smoothed) ---
// Exponential moving average with alpha ~0.1 (smooth over ~10 samples).
// At 2s print interval, this averages over ~20 seconds.
static float ema_voltage         = 0.0f;
static bool ema_initialized      = false;
static constexpr float EMA_ALPHA = 0.1f;

static float read_battery_voltage_raw() {
    analogReadResolution(12);
    uint16_t raw = analogRead(ADC_BATTERY);
    float v_adc  = (static_cast<float>(raw) / ADC_MAX_VAL) * ADC_REF_V;
    return v_adc * ADC_DIVIDER_FACTOR;
}

static float read_battery_voltage() {
    float raw = read_battery_voltage_raw();
    if (!ema_initialized) {
        ema_voltage     = raw;
        ema_initialized = true;
    } else {
        ema_voltage += EMA_ALPHA * (raw - ema_voltage);
    }
    return ema_voltage;
}

static void reset_ema() {
    ema_initialized = false;
    ema_voltage     = 0.0f;
}

// --- BQ24195 I2C helpers ---
static uint8_t bq_read_reg(uint8_t reg) {
    Wire.beginTransmission(BQ24195_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(BQ24195_ADDR, static_cast<uint8_t>(1));
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF;
}

static bool bq_write_reg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(BQ24195_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

// Enable or disable charging via REG01 CHG_CONFIG bits
static void bq_set_charging(bool enable) {
    // REG01 bit5-4: CHG_CONFIG = 01 (charge) or 00 (disable)
    // Keep other bits: no reset, sys_min=3.5V
    const uint8_t val = enable ? 0b00011010 : 0b00001010;
    bq_write_reg(REG_PON_CFG, val);
}

// Configure BQ24195 for cell recovery / testing:
// - Enable charging
// - Set low charge current (512mA) for safety with reclaimed cells
// - Set pre-charge current to minimum (128mA)
// - Set charge voltage to 4.208V (4.2V + 8mV for VREG[5:0])
// - Disable safety timer (allows slow pre-charge of deeply discharged cells)
// - Disable watchdog (so charging doesn't stop after 40s without I2C poke)
static void bq_configure_charger() {
    // REG01: Power-On Config
    //   bit7: 0 = no register reset
    //   bit6: 0 = no watchdog reset
    //   bit5-4: CHG_CONFIG = 01 = charge enable
    //   bit3-1: SYS_MIN = 101 = 3.5V minimum system voltage
    //   bit0: 0 = reserved
    bq_write_reg(REG_PON_CFG, 0b00011010);

    // REG02: Charge Current Control
    //   bit7-2: ICHG = 000000 → 512mA (offset=512mA, step=64mA)
    //   bit1-0: reserved
    bq_write_reg(REG_CHARGE_I, 0b00000000);

    // REG03: Pre-Charge / Termination Current
    //   bit7-4: IPRECHG = 0001 → 256mA (offset=128mA, step=128mA)
    //   bit3-0: ITERM = 0001 → 256mA (offset=128mA, step=128mA)
    bq_write_reg(REG_PCHARGE, 0b00010001);

    // REG04: Charge Voltage
    //   bit7-2: VREG = 101100 → 4.208V (offset=3.504V, step=16mV: 3.504+44*16mV=4.208)
    //   bit1: BATLOWV = 1 → pre-charge threshold 3.0V (default)
    //   bit0: VRECHG = 0 → recharge threshold 100mV below VREG
    bq_write_reg(REG_CHARGE_V, 0b10110010);

    // REG05: Charge Termination / Timer Control
    //   bit7: EN_TERM = 1 → enable termination
    //   bit6: reserved (0)
    //   bit5-4: watchdog = 00 → DISABLE watchdog timer
    //   bit3: EN_TIMER = 0 → DISABLE safety timer
    //   bit2-1: CHG_TIMER = 01 (irrelevant, timer disabled)
    //   bit0: reserved (0)
    bq_write_reg(REG_TIMER, 0b10000010);
}

static void bq_print_config() {
    Serial.println(F("BQ24195 configuration:"));
    uint8_t r1 = bq_read_reg(REG_PON_CFG);
    uint8_t r2 = bq_read_reg(REG_CHARGE_I);
    uint8_t r3 = bq_read_reg(REG_PCHARGE);
    uint8_t r5 = bq_read_reg(REG_TIMER);
    uint8_t rf = bq_read_reg(REG_FAULT);

    Serial.print(F("  Charging: "));
    Serial.println((r1 & 0x10) ? "enabled" : "DISABLED");
    uint16_t ichg = 512 + ((r2 >> 2) & 0x3F) * 64;
    Serial.print(F("  Charge current: "));
    Serial.print(ichg);
    Serial.println(F("mA"));
    uint16_t ipre = 128 + ((r3 >> 4) & 0x0F) * 128;
    Serial.print(F("  Pre-charge current: "));
    Serial.print(ipre);
    Serial.println(F("mA"));
    Serial.print(F("  Watchdog: "));
    Serial.println(((r5 >> 4) & 0x03) == 0 ? "disabled" : "enabled");
    Serial.print(F("  Safety timer: "));
    Serial.println((r5 & 0x08) ? "enabled" : "disabled");
    if (rf) {
        Serial.print(F("  Faults: 0x"));
        Serial.println(rf, HEX);
    } else {
        Serial.println(F("  Faults: none"));
    }
    Serial.println();
}

// --- Read BQ24195 charge status ---
static uint8_t read_charge_status() {
    return bq_read_reg(REG_STATUS);
}

static const char* charge_status_str(uint8_t status) {
    if (status == 0xFF)
        return "read error";
    uint8_t chrg = (status >> 4) & 0x03;
    switch (chrg) {
    case 0:
        return "not charging";
    case 1:
        return "pre-charge (<3V)";
    case 2:
        return "fast charging";
    case 3:
        return "charge complete";
    default:
        return "unknown";
    }
}

static bool is_charging(uint8_t status) {
    if (status == 0xFF)
        return false;
    uint8_t chrg = (status >> 4) & 0x03;
    return chrg == 1 || chrg == 2;
}

static bool is_charge_complete(uint8_t status) {
    if (status == 0xFF)
        return false;
    return ((status >> 4) & 0x03) == 3;
}

static bool is_usb_connected() {
    // On MKR WiFi 1010, Serial is native USB CDC.
    // If we have serial communication, USB power is available.
    return Serial;
}

// --- Classify cell from OCV ---
static const char* classify_cell(float voltage) {
    if (voltage < V_NO_CELL)
        return "NO CELL";
    if (voltage < V_DEAD)
        return "DEAD (<1V) - discard";
    if (voltage < V_DEEP_DISCH)
        return "DEEP DISCHARGED - recovery needed";
    if (voltage < V_DISCHARGED)
        return "DISCHARGED - charge before testing";
    if (voltage < V_FULL)
        return "GOOD - partially charged";
    return "FULLY CHARGED - ready for discharge test";
}

// --- Grade cell from capacity ---
static const char* grade_cell(float mah) {
    if (mah > 2000)
        return "A (>2000mAh) - excellent";
    if (mah > 1000)
        return "B (1000-2000mAh) - good for IoT";
    if (mah > 500)
        return "C (500-1000mAh) - marginal";
    return "D (<500mAh) - recycle";
}

// --- Cell removal detection ---
// Detects cell removal by: absolute low voltage, sudden voltage drop,
// or BQ24195 losing VBUS (PMIC glitches when battery is disconnected
// even though USB cable is still physically connected).
static bool cell_removed(float v) {
    if (v < V_NO_CELL)
        return true;
    // Sudden drop > 0.5V from last known reading → cell physically removed
    if (last_known_voltage > V_DEAD && (last_known_voltage - v) > V_DROP_REMOVAL)
        return true;
    return false;
}

// --- Format elapsed time as Xh Ym ---
static void print_elapsed(unsigned long start_ms) {
    unsigned long elapsed_s = (millis() - start_ms) / 1000;
    unsigned long h         = elapsed_s / 3600;
    unsigned long m         = (elapsed_s % 3600) / 60;
    unsigned long s         = elapsed_s % 60;
    if (h > 0) {
        Serial.print(h);
        Serial.print(F("h"));
        Serial.print(m);
        Serial.print(F("m"));
    } else if (m > 0) {
        Serial.print(m);
        Serial.print(F("m"));
        if (m < 10) {
            Serial.print(s);
            Serial.print(F("s"));
        }
    } else {
        Serial.print(s);
        Serial.print(F("s"));
    }
}

// --- Charge health analysis ---
// Returns true if charge looks healthy, false if cell should be discarded.
// Called every CHARGE_CHECK_MS during MONITORING state.
static bool check_charge_health(float current_v) {
    unsigned long now        = millis();
    unsigned long elapsed_s  = (now - charge_start_ms) / 1000;
    float delta_since_start  = current_v - charge_start_voltage;
    float delta_since_check  = current_v - charge_last_check_voltage;
    float check_interval_min = static_cast<float>(now - charge_last_check_ms) / 60000.0f;

    // Track maximum voltage seen
    if (current_v > charge_max_voltage) {
        charge_max_voltage = current_v;
    }

    // Rule 1: voltage dropping while charging → internal short
    if (current_v < charge_max_voltage - 0.05f) {
        Serial.println(F("\n>>> WARNING: voltage DROPPING during charge <<<"));
        Serial.print(F("    Peak was "));
        Serial.print(charge_max_voltage, 3);
        Serial.print(F("V, now "));
        Serial.print(current_v, 3);
        Serial.println(F("V"));
        Serial.println(F(">>> CELL LIKELY DEAD (internal short) — discard <<<\n"));
        return false;
    }

    // Rule 2: deep discharged cell (<2.5V) not gaining after 30 min
    if (charge_start_voltage < V_DEEP_DISCH && elapsed_s > 1800 && delta_since_start < 0.05f) {
        Serial.println(F("\n>>> WARNING: cell not responding to charge <<<"));
        Serial.print(F("    Started at "));
        Serial.print(charge_start_voltage, 3);
        Serial.print(F("V, now "));
        Serial.print(current_v, 3);
        Serial.print(F("V after "));
        print_elapsed(charge_start_ms);
        Serial.println();
        Serial.println(F(">>> CELL LIKELY DEAD — discard <<<\n"));
        return false;
    }

    // Rule 3: no progress in the last check interval (stalled)
    // Only flag after at least 5 minutes of stall, and not near full charge
    if (current_v < 4.0f && check_interval_min > 1.0f && delta_since_check < 0.005f) {
        if (!charge_warned) {
            Serial.println(F("\n>>> WARNING: charge stalled (no voltage gain) <<<"));
            Serial.print(F("    Voltage: "));
            Serial.print(current_v, 3);
            Serial.print(F("V ("));
            Serial.print(delta_since_check * 1000, 0);
            Serial.println(F("mV gain in last check interval)"));
            Serial.println(F("    If this persists, consider discarding."));
            Serial.println(F("    Send 'N' to skip to next cell.\n"));
            charge_warned = true;
        }
    } else {
        charge_warned = false; // Reset warning if progress resumes
    }

    // Update checkpoint
    charge_last_check_voltage = current_v;
    charge_last_check_ms      = now;
    return true;
}

// --- Print help ---
static void print_help() {
    Serial.println(F("\n=== 18650 Cell Tester - MKR WiFi 1010 ==="));
    Serial.println(F("Commands:"));
    Serial.println(F("  N - New cell (reset for next test)"));
    Serial.println(F("  D - Start discharge test (connect load resistor first!)"));
    Serial.println(F("  S - Stop discharge test"));
    Serial.println(F("  R - Set load resistance (e.g. R10.0)"));
    Serial.println(F("  C - Show charger status (BQ24195 registers)"));
    Serial.println(F("  H - This help"));
    Serial.print(F("Load resistance: "));
    Serial.print(load_resistance_ohm, 1);
    Serial.println(F(" ohm"));
    Serial.println();
}

// --- Process serial commands ---
static void process_serial() {
    if (!Serial.available())
        return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0)
        return;

    char c = toupper(cmd.charAt(0));

    switch (c) {
    case 'H':
        print_help();
        break;

    case 'N':
        cell_number++;
        state              = State::IDLE;
        accumulated_mah    = 0;
        last_known_voltage = 0;
        reset_ema();
        Serial.println(F("\n--- Ready for next cell ---"));
        Serial.print(F("Cell #"));
        Serial.println(cell_number);
        break;

    case 'D':
        if (state == State::DISCHARGE_TEST) {
            Serial.println(F("Already running."));
        } else {
            float v = read_battery_voltage();
            if (v < V_DISCHARGED) {
                Serial.print(F("Voltage too low ("));
                Serial.print(v, 2);
                Serial.println(F("V). Charge first."));
            } else {
                bq_set_charging(false);
                Serial.println(F("\n>>> DISCHARGE TEST STARTED (charging disabled) <<<"));
                Serial.print(F("Make sure "));
                Serial.print(load_resistance_ohm, 1);
                Serial.println(F(" ohm resistor is connected!"));
                Serial.print(F("Starting voltage: "));
                Serial.print(v, 3);
                Serial.println(F("V"));
                Serial.print(F("Cutoff: "));
                Serial.print(V_CUTOFF, 1);
                Serial.println(F("V"));
                Serial.println(F("Send 'S' to stop manually.\n"));

                state               = State::DISCHARGE_TEST;
                discharge_start_ms  = millis();
                last_sample_ms      = discharge_start_ms;
                last_sample_voltage = v;
                start_voltage       = v;
                accumulated_mah     = 0.0f;
            }
        }
        break;

    case 'S':
        if (state == State::DISCHARGE_TEST) {
            Serial.println(F("\n>>> DISCHARGE TEST STOPPED (manual) <<<"));
            bq_set_charging(true);
            state = State::TEST_COMPLETE;
        }
        break;

    case 'C':
        bq_print_config();
        break;

    case 'R': {
        float r = cmd.substring(1).toFloat();
        if (r >= 1.0f && r <= 1000.0f) {
            load_resistance_ohm = r;
            Serial.print(F("Load resistance set to "));
            Serial.print(load_resistance_ohm, 1);
            Serial.println(F(" ohm"));
        } else {
            Serial.println(F("Invalid. Use R<value> (1-1000 ohm). Example: R10"));
        }
        break;
    }

    default:
        Serial.print(F("Unknown command: "));
        Serial.println(cmd);
        Serial.println(F("Send 'H' for help."));
        break;
    }
}

// --- Print a status line ---
static unsigned long last_print_ms = 0;

static void print_status() {
    unsigned long now = millis();
    if (now - last_print_ms < PRINT_INTERVAL_MS)
        return;
    last_print_ms = now;

    float v_raw       = read_battery_voltage_raw();
    float v           = read_battery_voltage(); // EMA-smoothed
    uint8_t bq_status = read_charge_status();

    switch (state) {
    case State::IDLE:
        if (v_raw >= V_NO_CELL) {
            last_known_voltage = v_raw;
            Serial.print(F("\nCell #"));
            Serial.print(cell_number);
            Serial.println(F(" detected. Measuring OCV..."));
            ocv_start_ms = now;
            state        = State::OCV_MEASURE;
        }
        break;

    case State::OCV_MEASURE:
        if (cell_removed(v_raw)) {
            Serial.println(F("Cell removed."));
            last_known_voltage = 0;
            reset_ema();
            state = State::IDLE;
            break;
        }
        last_known_voltage = v_raw;
        Serial.print(F("  settling... "));
        Serial.print(v_raw, 3);
        Serial.println(F("V"));
        if (now - ocv_start_ms >= OCV_SETTLE_MS) {
            ocv_voltage = v_raw;
            Serial.println(F("\n============================="));
            Serial.print(F("Cell #"));
            Serial.print(cell_number);
            Serial.print(F(" OCV: "));
            Serial.print(ocv_voltage, 3);
            Serial.println(F("V"));
            Serial.print(F("Status: "));
            Serial.println(classify_cell(ocv_voltage));
            Serial.println(F("============================="));
            if (ocv_voltage >= V_DEAD && ocv_voltage < V_FULL) {
                Serial.println(F("USB charging is active if USB is connected."));
                Serial.println(F("Wait for full charge, then send 'D' to discharge test."));
            } else if (ocv_voltage >= V_FULL) {
                Serial.println(F("Ready for discharge test. Send 'D' to start."));
            }
            // Seed EMA with settled OCV so gain starts at 0
            reset_ema();
            ema_voltage     = ocv_voltage;
            ema_initialized = true;
            // Init charge monitoring
            charge_start_ms           = now;
            charge_start_voltage      = ocv_voltage;
            charge_last_check_voltage = ocv_voltage;
            charge_last_check_ms      = now;
            charge_max_voltage        = ocv_voltage;
            charge_warned             = false;
            charge_complete_seen      = false;
            // Enable charging now if USB is available
            if (is_usb_connected()) {
                bq_configure_charger();
            }
            state = State::MONITORING;
        }
        break;

    case State::MONITORING: {
        if (cell_removed(v_raw)) {
            Serial.println(F("Cell removed."));
            last_known_voltage = 0;
            reset_ema();
            state = State::IDLE;
            break;
        }
        last_known_voltage = v_raw;

        bool usb      = is_usb_connected();
        bool charging = is_charging(bq_status);
        bool complete = is_charge_complete(bq_status);

        // Hysteresis: once charge complete seen, stay "FULL" until below 4.0V
        if (complete) {
            charge_complete_seen = true;
        } else if (charge_complete_seen && v < 4.0f) {
            charge_complete_seen = false;
        }

        // Auto-enable charging if USB just connected
        if (usb && !charging && !complete && !charge_complete_seen) {
            bq_set_charging(true);
        }

        // Status line: voltage | elapsed | gain | charge status
        Serial.print(F("  "));
        Serial.print(v, 3);
        Serial.print(F("V | "));
        print_elapsed(charge_start_ms);
        Serial.print(F(" | "));
        {
            float gain = v - charge_start_voltage;
            if (gain >= 0)
                Serial.print(F("+"));
            Serial.print(gain * 1000, 0);
            Serial.print(F("mV"));
        }
        Serial.print(F(" | "));

        if (charge_complete_seen || complete) {
            Serial.print(F("FULL - send 'D' to test"));
        } else if (!usb) {
            Serial.print(F("no USB"));
        } else {
            Serial.print(charge_status_str(bq_status));
        }
        Serial.println();

        // Periodic charge health check (only while actively charging)
        if (charging && (now - charge_last_check_ms >= CHARGE_CHECK_MS)) {
            check_charge_health(v);
        }
        break;
    }

    case State::TEST_COMPLETE:
        // Print once, handled elsewhere
        break;

    case State::DISCHARGE_TEST:
        // Handled in discharge_tick()
        break;
    }
}

// --- Discharge test tick ---
static void discharge_tick() {
    if (state != State::DISCHARGE_TEST)
        return;

    unsigned long now = millis();
    if (now - last_sample_ms < DISCHARGE_SAMPLE_MS)
        return;

    float v             = read_battery_voltage();
    unsigned long dt_ms = now - last_sample_ms;
    float dt_h          = static_cast<float>(dt_ms) / 3600000.0f;

    // Trapezoidal integration: average voltage over interval → current → mAh
    float avg_v    = (last_sample_voltage + v) / 2.0f;
    float avg_i_ma = (avg_v / load_resistance_ohm) * 1000.0f;
    accumulated_mah += avg_i_ma * dt_h;

    float elapsed_min = static_cast<float>(now - discharge_start_ms) / 60000.0f;
    float current_ma  = (v / load_resistance_ohm) * 1000.0f;

    Serial.print(F("  "));
    Serial.print(elapsed_min, 1);
    Serial.print(F("min | "));
    Serial.print(v, 3);
    Serial.print(F("V | "));
    Serial.print(current_ma, 0);
    Serial.print(F("mA | "));
    Serial.print(accumulated_mah, 0);
    Serial.println(F("mAh"));

    last_sample_ms      = now;
    last_sample_voltage = v;

    // Check cutoff
    if (v <= V_CUTOFF) {
        Serial.println(F("\n>>> CUTOFF REACHED <<<"));
        bq_set_charging(true);
        state = State::TEST_COMPLETE;
    }

    // Check cell removed
    if (cell_removed(v)) {
        Serial.println(F("\n>>> CELL REMOVED - test aborted <<<"));
        bq_set_charging(true);
        last_known_voltage = 0;
        reset_ema();
        state           = State::IDLE;
        accumulated_mah = 0;
    }
}

// --- Print final results ---
static bool results_printed = false;

static void print_results() {
    if (state != State::TEST_COMPLETE || results_printed) {
        return;
    }
    results_printed = true;

    unsigned long const elapsed_ms = millis() - discharge_start_ms;
    float const elapsed_min        = static_cast<float>(elapsed_ms) / 60000.0f;

    Serial.println(F("\n╔══════════════════════════════════════╗"));
    Serial.println(F("║       DISCHARGE TEST RESULTS         ║"));
    Serial.println(F("╠══════════════════════════════════════╣"));
    Serial.print(F("║  Cell #"));
    Serial.println(cell_number);
    Serial.print(F("║  Start voltage:  "));
    Serial.print(start_voltage, 3);
    Serial.println(F("V"));
    Serial.print(F("║  End voltage:    "));
    Serial.print(last_sample_voltage, 3);
    Serial.println(F("V"));
    Serial.print(F("║  Load:           "));
    Serial.print(load_resistance_ohm, 1);
    Serial.println(F(" ohm"));
    Serial.print(F("║  Duration:       "));
    Serial.print(elapsed_min, 1);
    Serial.println(F(" min"));
    Serial.print(F("║  Capacity:       "));
    Serial.print(accumulated_mah, 0);
    Serial.println(F(" mAh"));
    Serial.print(F("║  Grade:          "));
    Serial.println(grade_cell(accumulated_mah));
    Serial.println(F("╚══════════════════════════════════════╝"));
    Serial.println(F("\nSend 'N' for next cell, 'D' to retest."));
}

// --- Arduino entry points ---

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000) { /* wait for native USB */
    }

    Wire.begin();
    analogReadResolution(12);

    Serial.println(F("\n╔══════════════════════════════════════╗"));
    Serial.println(F("║   18650 Cell Tester                  ║"));
    Serial.println(F("║   Arduino MKR WiFi 1010              ║"));
    Serial.println(F("╚══════════════════════════════════════╝"));
    Serial.println();

    // Configure BQ24195 — only enable charging if USB is connected
    if (is_usb_connected()) {
        bq_configure_charger();
        Serial.println(F("USB detected — charging enabled."));
    } else {
        Serial.println(F("No USB power detected — charging disabled."));
        Serial.println(F("Connect USB to enable charging."));
    }
    bq_print_config();

    Serial.println(F("Insert a cell into the JST connector."));
    Serial.println(F("USB charging is automatic when USB is connected."));
    Serial.print(F("Load resistance: "));
    Serial.print(load_resistance_ohm, 1);
    Serial.println(F(" ohm (change with R<value>)"));
    Serial.println(F("Send 'H' for help.\n"));
    Serial.print(F("Waiting for cell #"));
    Serial.print(cell_number);
    Serial.println(F("..."));
}

void loop() {
    process_serial();
    print_status();
    discharge_tick();
    print_results();

    if (state == State::TEST_COMPLETE && results_printed) {
        // Wait for next command, don't spam
        delay(500);
    }
}

#endif // CELL_TESTER
