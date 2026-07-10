#pragma once

#if defined(ESP32) && !defined(NATIVE)

#include "modules/ota_module.h"

// ESP32-C3 OTA performer: HTTP download + flash to the inactive OTA slot +
// reboot. On success the device reboots and never returns.
OtaResult platform_ota_perform(const char* url, const char* md5);

#endif
