#include "payload_builder.h"
#include <stdio.h>

void PayloadBuilder::begin() {
    pos = 0;
    first_field = true;
    if (buf_size > 0) {
        buf[0] = '{';
        pos = 1;
    }
}

void PayloadBuilder::add_float(const char* key, float value, uint8_t decimals) {
    if (pos < 0 || static_cast<size_t>(pos) >= buf_size) return;

    int n = snprintf(buf + pos, buf_size - pos,
                     "%s\"%s\":%.*f",
                     first_field ? "" : ",",
                     key, decimals, static_cast<double>(value));
    if (n > 0) {
        pos += n;
        first_field = false;
    }
}

void PayloadBuilder::add_uint(const char* key, uint32_t value) {
    if (pos < 0 || static_cast<size_t>(pos) >= buf_size) return;

    int n = snprintf(buf + pos, buf_size - pos,
                     "%s\"%s\":%lu",
                     first_field ? "" : ",",
                     key, static_cast<unsigned long>(value));
    if (n > 0) {
        pos += n;
        first_field = false;
    }
}

void PayloadBuilder::add_int(const char* key, int32_t value) {
    if (pos < 0 || static_cast<size_t>(pos) >= buf_size) return;

    int n = snprintf(buf + pos, buf_size - pos,
                     "%s\"%s\":%ld",
                     first_field ? "" : ",",
                     key, static_cast<long>(value));
    if (n > 0) {
        pos += n;
        first_field = false;
    }
}

void PayloadBuilder::add_string(const char* key, const char* value) {
    if (pos < 0 || static_cast<size_t>(pos) >= buf_size) return;

    int n = snprintf(buf + pos, buf_size - pos,
                     "%s\"%s\":\"%s\"",
                     first_field ? "" : ",",
                     key, value);
    if (n > 0) {
        pos += n;
        first_field = false;
    }
}

int PayloadBuilder::end() {
    if (pos >= 0 && static_cast<size_t>(pos) < buf_size) {
        buf[pos++] = '}';
    }
    if (pos >= 0 && static_cast<size_t>(pos) < buf_size) {
        buf[pos] = '\0';
    }
    return pos;
}
