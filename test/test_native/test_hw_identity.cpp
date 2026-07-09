#include <unity.h>
#include "hw_identity.h"

void test_hw_code_valid_accepts_8_alnum(void) {
    TEST_ASSERT_TRUE(hw_code_valid("E8BMEBAT"));
    TEST_ASSERT_TRUE(hw_code_valid("C3BMELUX"));
    TEST_ASSERT_TRUE(hw_code_valid("MKENVBAT"));
    TEST_ASSERT_TRUE(hw_code_valid("E8000001"));
}

void test_hw_code_valid_rejects_wrong_length(void) {
    TEST_ASSERT_FALSE(hw_code_valid("E8BMEBA"));   // 7
    TEST_ASSERT_FALSE(hw_code_valid("E8BMEBATT")); // 9
    TEST_ASSERT_FALSE(hw_code_valid(""));          // 0
    TEST_ASSERT_FALSE(hw_code_valid("unknown"));   // 7, and lowercase
}

void test_hw_code_valid_rejects_bad_chars(void) {
    TEST_ASSERT_FALSE(hw_code_valid("e8bmebat")); // lowercase
    TEST_ASSERT_FALSE(hw_code_valid("E8-BMEBA")); // hyphen
    TEST_ASSERT_FALSE(hw_code_valid("E8 BMEBA")); // space
    TEST_ASSERT_FALSE(hw_code_valid("E8BME_AT")); // underscore
}

void test_hw_code_valid_null(void) {
    TEST_ASSERT_FALSE(hw_code_valid(nullptr));
}
