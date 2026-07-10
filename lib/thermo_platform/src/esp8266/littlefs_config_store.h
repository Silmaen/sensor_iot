#pragma once

#if defined(ESP8266) && !defined(NATIVE)

#include "persistent_config_store.h"

#include <LittleFS.h>

// Config store backed by a JSON-ish text blob in LittleFS. The filesystem lives
// in its own flash region, so it survives an OTA (which only rewrites the app
// partition) and a power cycle.
class LittleFsConfigStore : public PersistentConfigStore {
protected:
    bool read_blob(char* buf, size_t size) override {
        if (!ensure_mounted()) return false;
        File f = LittleFS.open(PATH, "r");
        if (!f) return false;
        size_t n = f.readBytes(buf, size - 1);
        buf[n] = '\0';
        f.close();
        return n > 0;
    }

    bool write_blob(const char* blob) override {
        if (!ensure_mounted()) return false;
        File f = LittleFS.open(PATH, "w");
        if (!f) return false;
        f.print(blob);
        f.close();
        return true;
    }

private:
    static constexpr const char* PATH = "/config.txt";
    bool mounted_ = false;

    bool ensure_mounted() {
        if (mounted_) return true;
        if (LittleFS.begin()) { mounted_ = true; return true; }
        // First boot on a blank FS: format then mount.
        if (LittleFS.format() && LittleFS.begin()) { mounted_ = true; return true; }
        return false;
    }
};

#endif
