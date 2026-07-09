#pragma once

#include <stddef.h>

// A valid HW_CODE is exactly 8 uppercase-alphanumeric characters: ^[A-Z0-9]{8}$
// (see docs/ota-calibration-protocol.md §1.1). The device refuses to start when
// its compiled HW_CODE is invalid, guarding against a mis-configured build.
inline bool hw_code_valid(const char* hw_code) {
    if (!hw_code) return false;
    size_t n = 0;
    for (const char* p = hw_code; *p != '\0'; ++p) {
        if (n >= 8) return false; // longer than 8
        const char c = *p;
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!ok) return false;
        n++;
    }
    return n == 8;
}
