#if defined(ARDUINO_SAMD_MKRWIFI1010) && !defined(NATIVE)

#include "samd/flash_config_store.h"

#include <FlashStorage.h>
#include <string.h>

// One flash page holds a validity marker + the serialized config blob.
struct FlashCfg {
    uint8_t valid;                                  // 0xA5 once written
    char    blob[PersistentConfigStore::BLOB_SIZE];
};

static constexpr uint8_t FLASH_VALID = 0xA5;

FlashStorage(flash_cfg_store, FlashCfg);

bool FlashConfigStore::read_blob(char* buf, size_t size) {
    FlashCfg c = flash_cfg_store.read();
    if (c.valid != FLASH_VALID) return false;
    strncpy(buf, c.blob, size - 1);
    buf[size - 1] = '\0';
    return buf[0] != '\0';
}

bool FlashConfigStore::write_blob(const char* blob) {
    FlashCfg c = {};
    c.valid = FLASH_VALID;
    strncpy(c.blob, blob, sizeof(c.blob) - 1);
    c.blob[sizeof(c.blob) - 1] = '\0';
    flash_cfg_store.write(c);
    return true;
}

#endif
