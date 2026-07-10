#pragma once

#if defined(ESP8266) && !defined(NATIVE)

#include "modules/ota_module.h"

// ESP8266 OTA performer: HTTP download + MD5 verify + flash + reboot.
// On success the device reboots and never returns (see OtaResult contract).
OtaResult platform_ota_perform(const char* url, const char* md5);

#endif
