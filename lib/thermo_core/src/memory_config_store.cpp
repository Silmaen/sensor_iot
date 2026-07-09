#include "memory_config_store.h"

#include <string.h>

MemoryConfigStore::Entry* MemoryConfigStore::find(const char* key) {
    if (!key) return nullptr;
    for (auto& e : entries_) {
        if (e.type != Type::NONE && strncmp(e.key, key, MAX_KEY_LEN) == 0)
            return &e;
    }
    return nullptr;
}

MemoryConfigStore::Entry* MemoryConfigStore::find_or_alloc(const char* key) {
    if (!key || strlen(key) >= MAX_KEY_LEN) return nullptr;
    if (Entry* e = find(key)) return e;
    for (auto& e : entries_) {
        if (e.type == Type::NONE) {
            strncpy(e.key, key, MAX_KEY_LEN - 1);
            e.key[MAX_KEY_LEN - 1] = '\0';
            return &e;
        }
    }
    return nullptr; // store full
}

bool MemoryConfigStore::get_str(const char* key, char* out, size_t out_size) {
    if (!out || out_size == 0) return false;
    Entry* e = find(key);
    if (!e || e->type != Type::STR) return false;
    if (strlen(e->str) >= out_size) return false;
    strcpy(out, e->str);
    return true;
}

bool MemoryConfigStore::set_str(const char* key, const char* value) {
    if (!value || strlen(value) >= MAX_VAL_LEN) return false;
    Entry* e = find_or_alloc(key);
    if (!e) return false;
    strcpy(e->str, value);
    e->type = Type::STR;
    return true;
}

bool MemoryConfigStore::get_float(const char* key, float& out) {
    Entry* e = find(key);
    if (!e || e->type != Type::FLOAT) return false;
    out = e->num;
    return true;
}

bool MemoryConfigStore::set_float(const char* key, float value) {
    Entry* e = find_or_alloc(key);
    if (!e) return false;
    e->num = value;
    e->type = Type::FLOAT;
    return true;
}

bool MemoryConfigStore::has(const char* key) {
    return find(key) != nullptr;
}

bool MemoryConfigStore::remove(const char* key) {
    Entry* e = find(key);
    if (!e) return false;
    memset(e, 0, sizeof(*e)); // Type::NONE == 0
    return true;
}

bool MemoryConfigStore::clear() {
    memset(entries_, 0, sizeof(entries_));
    return true;
}
