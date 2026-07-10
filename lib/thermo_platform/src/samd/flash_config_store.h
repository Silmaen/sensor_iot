#pragma once

#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)

#include "persistent_config_store.h"

// Config store backed by the SAMD21 emulated EEPROM (FlashStorage). The
// reserved flash page survives a power cycle. (The MKR has no OTA, but the
// runtime-config refactor applies to every platform: device_id and calibration
// live here, never in the image.)
class FlashConfigStore : public PersistentConfigStore {
protected:
    bool read_blob(char* buf, size_t size) override;
    bool write_blob(const char* blob) override;
};

#endif
