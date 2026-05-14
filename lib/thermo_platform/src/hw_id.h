#pragma once

#include <stddef.h>

// Write the platform-specific hardware ID into buf.
// Format: "ESP-XXXXXX", "C3-XXXXXX", "MKR-XXXXXXXX", or "UNKNOWN".
void get_hw_id(char* buf, size_t len);
