#pragma once

#if defined(ESP32) && !defined(NATIVE)

#include "persistent_config_store.h"

#include <Preferences.h>
#include <string.h>

// Config store backed by an NVS string (Preferences). NVS is a dedicated flash
// partition, untouched by an OTA of the app partition, and survives power loss.
class NvsConfigStore : public PersistentConfigStore {
protected:
    bool read_blob(char* buf, size_t size) override {
        Preferences prefs;
        if (!prefs.begin(NS, /*readOnly=*/true)) return false;
        size_t n = prefs.getString(KEY, buf, size);
        prefs.end();
        return n > 0;
    }

    bool write_blob(const char* blob) override {
        Preferences prefs;
        if (!prefs.begin(NS, /*readOnly=*/false)) return false;
        size_t n = prefs.putString(KEY, blob);
        prefs.end();
        return n == strlen(blob);
    }

    bool erase_blob() override {
        Preferences prefs;
        if (!prefs.begin(NS, /*readOnly=*/false)) return false;
        bool ok = prefs.remove(KEY);
        prefs.end();
        return ok;
    }

private:
    static constexpr const char* NS  = "thermo";
    static constexpr const char* KEY = "cfg";
};

#endif
