// Standalone WiFi diagnostic for ESP32-C3. Every cycle it:
//   1. scans all 2.4 GHz networks (incl. hidden) -> RX signal (AP -> C3) in dBm
//   2. connects to WIFI_SSID (validates TX at the link layer: probe/auth/assoc)
//   3. if connected: reports RX RSSI + a round-trip test to the MQTT broker
//      (TCP handshake success/latency) as an uplink/TX proxy.
// True uplink dBm is NOT observable from the STA, so round-trip is the TX proxy.
// Build/flash: pio run -e wifi_scan_c3 -t upload --upload-port /dev/ttyACMx
// Only compiled when WIFI_SCAN is defined (env:wifi_scan_c3).
#ifdef WIFI_SCAN
#include <Arduino.h>
#include <WiFi.h>
#include "credentials.h"   // WIFI_SSID / WIFI_PASSWORD / MQTT_SERVER (+ optional WIFI_CHANNEL)

#ifndef WIFI_CHANNEL
#define WIFI_CHANNEL 0     // 0 = auto; set to the AP channel for a hidden SSID
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

static volatile int s_reason = 0;  // raw esp-idf disconnect reason

void setup() {
    Serial.begin(115200);
    delay(2000);
    WiFi.mode(WIFI_STA);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        s_reason = info.wifi_sta_disconnected.reason;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    Serial.println("[DIAG] WiFi RX/TX diagnostic ready");
}

void loop() {
    // 1) SCAN — RX signal (AP -> C3), in dBm
    Serial.println("\n===== SCAN (RX: AP -> C3, dBm) =====");
    int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
    int best_target = -127;
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        Serial.printf("  ch=%2d rssi=%4d dBm enc=%d bssid=%s ssid=%s\n",
                      WiFi.channel(i), WiFi.RSSI(i), (int)WiFi.encryptionType(i),
                      WiFi.BSSIDstr(i).c_str(), ssid.length() ? ssid.c_str() : "<hidden>");
        // Track strongest AP on our channel matching the target (named or hidden).
        if (WiFi.channel(i) == WIFI_CHANNEL && WiFi.RSSI(i) > best_target)
            best_target = WiFi.RSSI(i);
    }
    WiFi.scanDelete();
    if (best_target > -127)
        Serial.printf("  -> best AP on ch %d: %d dBm (RX)\n", WIFI_CHANNEL, best_target);

    // 2) CONNECT to the real SSID — this exercises TX (probe/auth/assoc frames).
    Serial.printf("\n===== CONNECT \"%s\" (ch %d) — validates TX =====\n", WIFI_SSID, WIFI_CHANNEL);
    WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);          // network is WPA_PSK (WPA1)
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);        // enumerate ALL matching APs...
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);    // ...and pick the strongest (roaming, no BSSID pin)
    s_reason = 0;
    unsigned long t0 = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000)
        delay(200);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("  CONNECTED in %lu ms  IP=%s\n",
                      millis() - t0, WiFi.localIP().toString().c_str());
        Serial.printf("  RX (AP -> C3): RSSI = %d dBm\n", WiFi.RSSI());

        // 3) Round-trip test = TX/uplink proxy (TCP handshake needs C3 to be heard).
        const int tries = 10;
        int ok = 0; unsigned long rtt_sum = 0, rtt_min = 999999, rtt_max = 0;
        for (int i = 0; i < tries; i++) {
            WiFiClient c;
            unsigned long p0 = millis();
            if (c.connect(MQTT_SERVER, MQTT_PORT, 1500)) {
                unsigned long rtt = millis() - p0;
                ok++; rtt_sum += rtt;
                if (rtt < rtt_min) rtt_min = rtt;
                if (rtt > rtt_max) rtt_max = rtt;
                c.stop();
            }
            delay(150);
        }
        Serial.printf("  TX proxy (C3 -> %s:%d round-trip): %d/%d ok",
                      MQTT_SERVER, MQTT_PORT, ok, tries);
        if (ok) Serial.printf("  rtt min/avg/max = %lu/%lu/%lu ms",
                              rtt_min, rtt_sum / ok, rtt_max);
        Serial.println();
        Serial.printf("  => link: RX %d dBm, uplink %s\n",
                      WiFi.RSSI(),
                      ok == tries ? "healthy" : ok ? "LOSSY (weak/erratic TX)" : "DEAD (TX not reaching AP)");
    } else {
        Serial.printf("  FAILED  status=%d reason=%d  ", WiFi.status(), s_reason);
        Serial.println(s_reason == 201 ? "(NO_AP_FOUND: probe not answered -> TX or hidden discovery)"
                     : s_reason == 39  ? "(TIMEOUT: assoc not completing -> TX not reaching AP)"
                     : s_reason == 15  ? "(HANDSHAKE: wrong password)"
                                       : "(see esp_wifi reason codes)");
    }
    WiFi.disconnect(true);
    delay(4000);
}
#endif // WIFI_SCAN
