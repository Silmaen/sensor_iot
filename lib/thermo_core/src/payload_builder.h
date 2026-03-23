#pragma once

#include <stddef.h>
#include <stdint.h>

struct PayloadBuilder {
    char* buf;
    size_t buf_size;
    int pos;
    bool first_field;

    void begin();
    void add_float(const char* key, float value, uint8_t decimals);
    void add_uint(const char* key, uint32_t value);
    int end();
};
