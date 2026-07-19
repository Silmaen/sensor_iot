#include "platform_diag.h"

#if defined(ESP32) && !defined(NATIVE)
#include <Arduino.h>
#include <esp_system.h>

uint8_t platform_reset_cause() {
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return RESET_POWER_ON;
    case ESP_RST_EXT:       return RESET_EXTERNAL;
    case ESP_RST_SW:        return RESET_SOFTWARE;
    case ESP_RST_DEEPSLEEP: return RESET_DEEP_SLEEP;
    case ESP_RST_BROWNOUT:  return RESET_BROWNOUT;
    case ESP_RST_PANIC:     return RESET_PANIC;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return RESET_WATCHDOG;
    default:                return RESET_UNKNOWN;
    }
}
uint32_t platform_free_heap() { return ESP.getFreeHeap(); }

#elif defined(ESP8266) && !defined(NATIVE)
#include <Arduino.h>
extern "C" {
#include <user_interface.h>  // rst_info, REASON_* codes
}

uint8_t platform_reset_cause() {
    switch (ESP.getResetInfoPtr()->reason) {
    case REASON_DEFAULT_RST:      return RESET_POWER_ON;
    case REASON_EXT_SYS_RST:      return RESET_EXTERNAL;
    case REASON_SOFT_RESTART:     return RESET_SOFTWARE;
    case REASON_DEEP_SLEEP_AWAKE: return RESET_DEEP_SLEEP;
    case REASON_EXCEPTION_RST:    return RESET_PANIC;
    case REASON_WDT_RST:
    case REASON_SOFT_WDT_RST:     return RESET_WATCHDOG;
    default:                      return RESET_UNKNOWN;  // ESP8266 has no brownout code
    }
}
uint32_t platform_free_heap() { return ESP.getFreeHeap(); }

#elif defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)
#include <Arduino.h>

uint8_t platform_reset_cause() {
    const uint8_t rc = PM->RCAUSE.reg;
    if (rc & PM_RCAUSE_POR)                         return RESET_POWER_ON;
    if (rc & (PM_RCAUSE_BOD12 | PM_RCAUSE_BOD33))   return RESET_BROWNOUT;
    if (rc & PM_RCAUSE_EXT)                         return RESET_EXTERNAL;
    if (rc & PM_RCAUSE_WDT)                         return RESET_WATCHDOG;
    if (rc & PM_RCAUSE_SYST)                        return RESET_SOFTWARE;
    return RESET_UNKNOWN;
}
uint32_t platform_free_heap() { return 0; }  // no simple heap metric on SAMD21

#else  // NATIVE / other
uint8_t platform_reset_cause() { return RESET_UNKNOWN; }
uint32_t platform_free_heap() { return 0; }
#endif
