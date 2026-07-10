#if defined(ESP8266) && !defined(NATIVE)

#include "esp8266/esp_ota.h"

#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <stdio.h>

OtaResult platform_ota_perform(const char* url, const char* md5) {
    ESPhttpUpdate.rebootOnUpdate(true);
    if (md5 && md5[0] != '\0')
        ESPhttpUpdate.setMD5sum(md5); // aborts the flash on mismatch

    WiFiClient client;
    // Blocks until done; reboots on success (rebootOnUpdate) → never returns.
    t_httpUpdate_return const r = ESPhttpUpdate.update(client, url);

    static char err[64];
    switch (r) {
    case HTTP_UPDATE_FAILED:
        snprintf(err, sizeof(err), "http_err_%d", ESPhttpUpdate.getLastError());
        return {false, err};
    case HTTP_UPDATE_NO_UPDATES:
        return {false, "no_update"};
    default:
        return {false, "update_failed"};
    }
}

#endif
