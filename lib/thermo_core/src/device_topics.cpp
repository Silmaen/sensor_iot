#include "device_topics.h"

#include <stdio.h>
#include <string.h>

void DeviceTopics::build(const char* device_type, const char* device_id) {
    snprintf(sensors,      TOPIC_LEN, "%s/%s/sensors",      device_type, device_id);
    snprintf(status,       TOPIC_LEN, "%s/%s/status",       device_type, device_id);
    snprintf(command,      TOPIC_LEN, "%s/%s/command",      device_type, device_id);
    snprintf(capabilities, TOPIC_LEN, "%s/%s/capabilities", device_type, device_id);
    snprintf(ack,          TOPIC_LEN, "%s/%s/ack",          device_type, device_id);
    snprintf(commands,     TOPIC_LEN, "%s/%s/commands",     device_type, device_id);
    snprintf(calibration,  TOPIC_LEN, "%s/%s/calibration",  device_type, device_id);
    snprintf(diag,         TOPIC_LEN, "%s/%s/diag",         device_type, device_id);
}

bool device_id_valid(const char* device_id) {
    if (!device_id) return false;
    size_t n = 0;
    for (const char* p = device_id; *p != '\0'; ++p) {
        const char c = *p;
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
        if (++n > 63) return false; // store MAX_VAL_LEN - 1
    }
    return n > 0;
}
