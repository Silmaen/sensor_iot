#include <unity.h>
#include <string.h>
#include <stdio.h>
#include "memory_config_store.h"
#include "config_store.h"

void test_config_store_str_roundtrip(void) {
    MemoryConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_TRUE(store.set_str(config_keys::device_id, "thermo_1"));
    char buf[32];
    TEST_ASSERT_TRUE(store.get_str(config_keys::device_id, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("thermo_1", buf);
}

void test_config_store_float_roundtrip(void) {
    MemoryConfigStore store;
    TEST_ASSERT_TRUE(store.set_float(config_keys::bat_divider, 2.771f));
    float v = 0.0f;
    TEST_ASSERT_TRUE(store.get_float(config_keys::bat_divider, v));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.771f, v);
}

void test_config_store_missing_key(void) {
    MemoryConfigStore store;
    char buf[16];
    float v = 0.0f;
    TEST_ASSERT_FALSE(store.get_str(config_keys::device_id, buf, sizeof(buf)));
    TEST_ASSERT_FALSE(store.get_float(config_keys::cal_temp, v));
    TEST_ASSERT_FALSE(store.has(config_keys::device_id));
}

void test_config_store_overwrite(void) {
    MemoryConfigStore store;
    store.set_str(config_keys::device_id, "old");
    store.set_str(config_keys::device_id, "new");
    char buf[16];
    store.get_str(config_keys::device_id, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("new", buf);
}

void test_config_store_type_mismatch(void) {
    MemoryConfigStore store;
    store.set_float(config_keys::cal_temp, 1.5f);
    char buf[16];
    // Reading a float key as a string must fail (not misinterpret).
    TEST_ASSERT_FALSE(store.get_str(config_keys::cal_temp, buf, sizeof(buf)));
}

void test_config_store_get_str_overflow(void) {
    MemoryConfigStore store;
    store.set_str(config_keys::device_id, "a_fairly_long_device_id");
    char small[8];
    TEST_ASSERT_FALSE(store.get_str(config_keys::device_id, small, sizeof(small)));
}

void test_config_store_set_str_too_long(void) {
    MemoryConfigStore store;
    char big[MemoryConfigStore::MAX_VAL_LEN + 10];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    TEST_ASSERT_FALSE(store.set_str(config_keys::device_id, big));
}

void test_config_store_remove(void) {
    MemoryConfigStore store;
    store.set_str(config_keys::device_id, "thermo_1");
    TEST_ASSERT_TRUE(store.has(config_keys::device_id));
    TEST_ASSERT_TRUE(store.remove(config_keys::device_id));
    TEST_ASSERT_FALSE(store.has(config_keys::device_id));
    TEST_ASSERT_FALSE(store.remove(config_keys::device_id)); // already gone
}

void test_config_store_clear(void) {
    MemoryConfigStore store;
    store.set_str(config_keys::device_id, "thermo_1");
    store.set_float(config_keys::cal_temp, -2.5f);
    TEST_ASSERT_TRUE(store.clear());
    TEST_ASSERT_FALSE(store.has(config_keys::device_id));
    TEST_ASSERT_FALSE(store.has(config_keys::cal_temp));
}

void test_config_store_full(void) {
    MemoryConfigStore store;
    char key[8];
    for (size_t i = 0; i < MemoryConfigStore::MAX_ENTRIES; i++) {
        snprintf(key, sizeof(key), "k%zu", i);
        TEST_ASSERT_TRUE(store.set_float(key, (float)i));
    }
    // One past capacity fails.
    TEST_ASSERT_FALSE(store.set_float("overflow", 1.0f));
}

void test_config_store_serialize_roundtrip(void) {
    MemoryConfigStore a;
    a.set_str(config_keys::device_id, "thermo_1");
    a.set_float(config_keys::cal_temp, -2.5f);
    a.set_float(config_keys::bat_divider, 2.771f);

    char blob[256];
    int len = a.serialize(blob, sizeof(blob));
    TEST_ASSERT_GREATER_THAN(0, len);

    MemoryConfigStore b;
    b.deserialize(blob);

    char id[32];
    TEST_ASSERT_TRUE(b.get_str(config_keys::device_id, id, sizeof(id)));
    TEST_ASSERT_EQUAL_STRING("thermo_1", id);
    float t = 0, d = 0;
    TEST_ASSERT_TRUE(b.get_float(config_keys::cal_temp, t));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.5f, t);
    TEST_ASSERT_TRUE(b.get_float(config_keys::bat_divider, d));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.771f, d);
}

void test_config_store_deserialize_replaces(void) {
    MemoryConfigStore s;
    s.set_str(config_keys::device_id, "stale");
    s.deserialize("sdevice_id=fresh\n");
    char id[16];
    s.get_str(config_keys::device_id, id, sizeof(id));
    TEST_ASSERT_EQUAL_STRING("fresh", id);
    TEST_ASSERT_FALSE(s.has(config_keys::cal_temp));
}

void test_config_store_deserialize_empty_and_garbage(void) {
    MemoryConfigStore s;
    s.deserialize("");           // no crash, empty
    TEST_ASSERT_FALSE(s.has(config_keys::device_id));
    s.deserialize("garbage-no-equals\nx\n"); // ignored lines
    TEST_ASSERT_FALSE(s.has(config_keys::device_id));
}

void test_config_calibration_present_flag(void) {
    MemoryConfigStore store;
    // Fresh store, only device_id provisioned → no calibration.
    store.set_str(config_keys::device_id, "thermo_1");
    TEST_ASSERT_FALSE(config_calibration_present(store));
    // A single calibration key flips the flag.
    store.set_float(config_keys::cal_temp, -2.5f);
    TEST_ASSERT_TRUE(config_calibration_present(store));
}
