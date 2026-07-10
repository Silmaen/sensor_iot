#pragma once

#include "memory_config_store.h"

// IConfigStore that keeps entries in RAM (MemoryConfigStore) and persists them
// as one serialized blob through platform hooks. Writes are buffered in RAM and
// flushed to storage on commit(). Platform subclasses implement read_blob /
// write_blob (and optionally erase_blob) for LittleFS / NVS / FlashStorage.
class PersistentConfigStore : public IConfigStore {
public:
    static constexpr size_t BLOB_SIZE = 768;

    bool begin() override {
        char blob[BLOB_SIZE];
        if (read_blob(blob, sizeof(blob)))
            mem_.deserialize(blob);
        return true;
    }

    bool get_str(const char* k, char* o, size_t n) override { return mem_.get_str(k, o, n); }
    bool set_str(const char* k, const char* v) override { bool r = mem_.set_str(k, v); dirty_ |= r; return r; }
    bool get_float(const char* k, float& o) override { return mem_.get_float(k, o); }
    bool set_float(const char* k, float v) override { bool r = mem_.set_float(k, v); dirty_ |= r; return r; }
    bool has(const char* k) override { return mem_.has(k); }
    bool remove(const char* k) override { bool r = mem_.remove(k); dirty_ |= r; return r; }

    bool clear() override {
        mem_.clear();
        dirty_ = false;
        return erase_blob();
    }

    bool commit() override {
        if (!dirty_) return true;
        char blob[BLOB_SIZE];
        if (mem_.serialize(blob, sizeof(blob)) < 0) return false;
        if (!write_blob(blob)) return false;
        dirty_ = false;
        return true;
    }

protected:
    // Load the stored blob into buf; return false if none stored yet.
    virtual bool read_blob(char* buf, size_t size) = 0;
    // Persist the blob. Return false on storage failure.
    virtual bool write_blob(const char* blob) = 0;
    // Erase stored data (factory reset). Default: write an empty blob.
    virtual bool erase_blob() { return write_blob(""); }

    MemoryConfigStore mem_;
    bool dirty_ = false;
};
