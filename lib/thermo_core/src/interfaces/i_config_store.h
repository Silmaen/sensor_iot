#pragma once

#include <stddef.h>

// Persistent key-value store for per-unit runtime configuration (device_id,
// calibration). It is deliberately kept out of the firmware image so that a
// single binary is device-agnostic (see docs/ota-calibration-protocol.md §3).
//
// Platform implementations back this with storage that survives an OTA (which
// only rewrites the application partition) and a power cycle:
//   - ESP8266 : LittleFS (JSON file)
//   - ESP32-C3: NVS (Preferences)
//   - SAMD21  : FlashStorage
// A portable in-memory implementation (MemoryConfigStore) backs native tests.
class IConfigStore {
public:
    virtual ~IConfigStore() = default;

    // Mount / initialise the backing storage. Returns false on failure.
    virtual bool begin() = 0;

    // Read a string value for key into out (null-terminated). Returns false if
    // the key is absent or the value does not fit in out_size.
    virtual bool get_str(const char* key, char* out, size_t out_size) = 0;

    // Write a string value. Returns false on storage failure.
    virtual bool set_str(const char* key, const char* value) = 0;

    // Read a float value into out. Returns false if the key is absent.
    virtual bool get_float(const char* key, float& out) = 0;

    // Write a float value. Returns false on storage failure.
    virtual bool set_float(const char* key, float value) = 0;

    // True if the key exists in the store.
    virtual bool has(const char* key) = 0;

    // Remove a single key. Returns true if it existed and was removed.
    virtual bool remove(const char* key) = 0;

    // Erase the whole store (factory reset). Returns false on storage failure.
    virtual bool clear() = 0;

    // Flush pending writes to the backing storage. No-op for stores that write
    // through. Returns false on storage failure.
    virtual bool commit() { return true; }
};
