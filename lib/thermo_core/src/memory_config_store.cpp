#include "memory_config_store.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

int MemoryConfigStore::serialize(char* buf, size_t buf_size) const {
    if (!buf || buf_size == 0) return -1;
    int pos = 0;
    for (const auto& e : entries_) {
        int n = 0;
        if (e.type == Type::STR) {
            n = snprintf(buf + pos, buf_size - pos, "s%s=%s\n", e.key, e.str);
        } else if (e.type == Type::FLOAT) {
            n = snprintf(buf + pos, buf_size - pos, "f%s=%.7g\n", e.key, (double)e.num);
        } else {
            continue;
        }
        if (n < 0 || (size_t)(pos + n) >= buf_size) return -1; // overflow
        pos += n;
    }
    buf[pos] = '\0';
    return pos;
}

void MemoryConfigStore::deserialize(const char* blob) {
    clear();
    if (!blob) return;
    const char* line = blob;
    while (*line) {
        const char* nl = strchr(line, '\n');
        const char* line_end = nl ? nl : (line + strlen(line));
        // A line is at minimum "<type><key>=" (>= 3 chars, non-empty key/value).
        const char type = line[0];
        const char* eq = (const char*)memchr(line, '=', line_end - line);
        if ((type == 's' || type == 'f') && eq && eq > line + 1) {
            char key[MAX_KEY_LEN];
            char val[MAX_VAL_LEN];
            size_t klen = (size_t)(eq - (line + 1));
            size_t vlen = (size_t)(line_end - (eq + 1));
            if (klen < MAX_KEY_LEN && vlen < MAX_VAL_LEN) {
                memcpy(key, line + 1, klen); key[klen] = '\0';
                memcpy(val, eq + 1, vlen);   val[vlen] = '\0';
                if (type == 's') set_str(key, val);
                else             set_float(key, (float)atof(val));
            }
        }
        if (!nl) break;
        line = nl + 1;
    }
}
