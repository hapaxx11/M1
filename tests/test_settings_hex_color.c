/*
 * test_settings_hex_color.c — Unit tests for settings_parse_hex_color()
 *
 * Uses the stub-based extraction pattern: the function under test is
 * pure logic (hex string → RGB), so it compiles and runs on the host
 * with no hardware dependencies.
 */

#include "unity.h"
#include "m1_settings.h"
#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

/* ---- Happy-path tests ---- */

void test_parse_uppercase_without_hash(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(1, settings_parse_hex_color("331480", &r, &g, &b));
    TEST_ASSERT_EQUAL_HEX8(0x33, r);
    TEST_ASSERT_EQUAL_HEX8(0x14, g);
    TEST_ASSERT_EQUAL_HEX8(0x80, b);
}

void test_parse_uppercase_with_hash(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(1, settings_parse_hex_color("#FF00AA", &r, &g, &b));
    TEST_ASSERT_EQUAL_HEX8(0xFF, r);
    TEST_ASSERT_EQUAL_HEX8(0x00, g);
    TEST_ASSERT_EQUAL_HEX8(0xAA, b);
}

void test_parse_lowercase(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(1, settings_parse_hex_color("abcdef", &r, &g, &b));
    TEST_ASSERT_EQUAL_HEX8(0xAB, r);
    TEST_ASSERT_EQUAL_HEX8(0xCD, g);
    TEST_ASSERT_EQUAL_HEX8(0xEF, b);
}

void test_parse_mixed_case(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(1, settings_parse_hex_color("#aB12cD", &r, &g, &b));
    TEST_ASSERT_EQUAL_HEX8(0xAB, r);
    TEST_ASSERT_EQUAL_HEX8(0x12, g);
    TEST_ASSERT_EQUAL_HEX8(0xCD, b);
}

void test_parse_black(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(1, settings_parse_hex_color("000000", &r, &g, &b));
    TEST_ASSERT_EQUAL_HEX8(0x00, r);
    TEST_ASSERT_EQUAL_HEX8(0x00, g);
    TEST_ASSERT_EQUAL_HEX8(0x00, b);
}

void test_parse_white(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(1, settings_parse_hex_color("FFFFFF", &r, &g, &b));
    TEST_ASSERT_EQUAL_HEX8(0xFF, r);
    TEST_ASSERT_EQUAL_HEX8(0xFF, g);
    TEST_ASSERT_EQUAL_HEX8(0xFF, b);
}

/* The parser accepts >= 6 hex chars (trailing data is ignored, e.g. newline
 * in the settings file).  This is intentional. */
void test_parse_with_trailing_data(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(1, settings_parse_hex_color("331480\n", &r, &g, &b));
    TEST_ASSERT_EQUAL_HEX8(0x33, r);
    TEST_ASSERT_EQUAL_HEX8(0x14, g);
    TEST_ASSERT_EQUAL_HEX8(0x80, b);
}

/* ---- Rejection tests ---- */

void test_reject_too_short(void)
{
    uint8_t r = 0xFF, g = 0xFF, b = 0xFF;
    TEST_ASSERT_EQUAL_UINT8(0, settings_parse_hex_color("1234", &r, &g, &b));
    /* Output values unchanged on failure */
    TEST_ASSERT_EQUAL_HEX8(0xFF, r);
}

void test_reject_empty(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(0, settings_parse_hex_color("", &r, &g, &b));
}

void test_reject_hash_only(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(0, settings_parse_hex_color("#", &r, &g, &b));
}

void test_reject_invalid_char(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(0, settings_parse_hex_color("33GG80", &r, &g, &b));
}

void test_reject_spaces(void)
{
    uint8_t r, g, b;
    TEST_ASSERT_EQUAL_UINT8(0, settings_parse_hex_color("33 480", &r, &g, &b));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_parse_uppercase_without_hash);
    RUN_TEST(test_parse_uppercase_with_hash);
    RUN_TEST(test_parse_lowercase);
    RUN_TEST(test_parse_mixed_case);
    RUN_TEST(test_parse_black);
    RUN_TEST(test_parse_white);
    RUN_TEST(test_parse_with_trailing_data);
    RUN_TEST(test_reject_too_short);
    RUN_TEST(test_reject_empty);
    RUN_TEST(test_reject_hash_only);
    RUN_TEST(test_reject_invalid_char);
    RUN_TEST(test_reject_spaces);

    return UNITY_END();
}
