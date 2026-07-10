#if defined(ESP32) && !defined(NATIVE)

#include "esp32/esp32_ota.h"

#include <WiFi.h>
#include <HTTPUpdate.h>
#include <stdio.h>

OtaResult platform_ota_perform(const char* url, const char* md5) {
    httpUpdate.rebootOnUpdate(true);
    // NOTE(v1.1): Arduino-ESP32 HTTPUpdate exposes no setMD5sum(); integrity
    // here relies on the app image's embedded SHA256 (verified by the 2nd-stage
    // bootloader). Explicit MD5 (via low-level Update.setMD5 + streamed write)
    // is a planned hardening. The `md5` argument is accepted for protocol
    // symmetry with the ESP8266 performer.
    (void)md5;

    WiFiClient client;
    // Blocks; writes to the inactive OTA slot and reboots on success.
    t_httpUpdate_return const r = httpUpdate.update(client, url);

    static char err[64];
    switch (r) {
    case HTTP_UPDATE_FAILED:
        snprintf(err, sizeof(err), "http_err_%d", httpUpdate.getLastError());
        return {false, err};
    case HTTP_UPDATE_NO_UPDATES:
        return {false, "no_update"};
    default:
        return {false, "update_failed"};
    }
}

#endif
