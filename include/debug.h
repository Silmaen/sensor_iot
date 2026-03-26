#pragma once

// Serial debug macros — enabled by -DHAS_SERIAL_DEBUG build flag.
// Compiles to nothing when the flag is absent.

#if defined(HAS_SERIAL_DEBUG) && !defined(NATIVE)
#define DEBUG_INIT()    do { Serial.begin(115200); } while (0)
#define DEBUG_PRINT(x)  Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(fmt, ...) do { \
    char _dbg[128]; \
    snprintf(_dbg, sizeof(_dbg), fmt, ##__VA_ARGS__); \
    Serial.print(_dbg); \
} while(0)
#else
#define DEBUG_INIT()         ((void)0)
#define DEBUG_PRINT(x)       ((void)0)
#define DEBUG_PRINTLN(x)     ((void)0)
#define DEBUG_PRINTF(fmt, ...) ((void)0)
#endif
