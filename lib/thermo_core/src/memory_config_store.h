#pragma once

#include "interfaces/i_config_store.h"

#include <stdint.h>

// Portable, heap-free, fixed-capacity in-memory IConfigStore. Backs native
// unit tests and the NATIVE build; also serves as the reference implementation
// for the platform-backed stores. No Arduino / no persistence.
class MemoryConfigStore : public IConfigStore {
public:
    static constexpr size_t MAX_ENTRIES = 8;
    static constexpr size_t MAX_KEY_LEN = 16;   // NVS keys are capped at 15
    static constexpr size_t MAX_VAL_LEN = 64;

    bool begin() override { return true; }
    bool get_str(const char* key, char* out, size_t out_size) override;
    bool set_str(const char* key, const char* value) override;
    bool get_float(const char* key, float& out) override;
    bool set_float(const char* key, float value) override;
    bool has(const char* key) override;
    bool remove(const char* key) override;
    bool clear() override;

    // Serialize all entries to a text blob for platform persistence. One line
    // per entry: "<type><key>=<value>" where type is 's' (string) or 'f'
    // (float). Returns the number of chars written (excluding the terminator),
    // or a negative value on overflow.
    int serialize(char* buf, size_t buf_size) const;

    // Replace all contents from a text blob produced by serialize().
    void deserialize(const char* blob);

private:
    enum class Type : uint8_t { NONE, STR, FLOAT };
    struct Entry {
        char  key[MAX_KEY_LEN];
        char  str[MAX_VAL_LEN];
        float num;
        Type  type;
    };
    Entry entries_[MAX_ENTRIES] = {};

    Entry* find(const char* key);
    Entry* find_or_alloc(const char* key);
};
