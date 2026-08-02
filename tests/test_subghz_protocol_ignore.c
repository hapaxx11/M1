/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See COPYING.txt for license details. */

/*
 * test_subghz_protocol_ignore.c
 *
 * Unit tests for the Sub-GHz protocol ignore-list module
 * (Sub_Ghz/subghz_protocol_ignore.c).
 *
 * Covers:
 *   - Default state (nothing ignored) and reset.
 *   - set / is_ignored / toggle round-trips, including bounds behaviour.
 *   - count() correctness.
 *   - Hex serialize/deserialize round-trip and specific bit placement
 *     (little-endian bit -> big-endian string ordering).
 *   - Name-based helpers via a minimal stub registry.
 *
 * Build:
 *   cmake -B tests/build-tests -S tests && cmake --build tests/build-tests
 *   ctest --test-dir tests/build-tests -R subghz_protocol_ignore --output-on-failure
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity.h"
#include "subghz_protocol_ignore.h"
#include "subghz_protocol_registry.h"

/*============================================================================*/
/* Minimal stub registry — only find_by_name is exercised by the module.      */
/*============================================================================*/

static const char *const g_stub_names[] = {
    "Princeton",   /* index 0 */
    "CAME",        /* index 1 */
    "NiceFlo",     /* index 2 */
    "KeeLoq",      /* index 3 */
};
#define STUB_COUNT ((int)(sizeof(g_stub_names) / sizeof(g_stub_names[0])))

int16_t subghz_protocol_find_by_name(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < STUB_COUNT; i++)
        if (strcmp(g_stub_names[i], name) == 0)
            return (int16_t)i;
    return -1;
}

/* Provide the other registry symbols in case the linker wants them. */
const char *subghz_protocol_get_name(uint16_t i)
{
    return (i < STUB_COUNT) ? g_stub_names[i] : 0;
}

void setUp(void)   { subghz_ignore_reset(); }
void tearDown(void) {}

/*============================================================================*/
/* Core bitset tests                                                          */
/*============================================================================*/

void test_default_nothing_ignored(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_count());
    for (uint16_t i = 0; i < SUBGHZ_IGNORE_MAX_PROTOCOLS; i++)
        TEST_ASSERT_FALSE(subghz_ignore_is_ignored(i));
}

void test_set_and_query(void)
{
    subghz_ignore_set(5, true);
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(5));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(4));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(6));
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_count());

    subghz_ignore_set(5, false);
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(5));
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_count());
}

void test_toggle(void)
{
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(10));
    subghz_ignore_toggle(10);
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(10));
    subghz_ignore_toggle(10);
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(10));
}

void test_cross_word_boundary(void)
{
    /* Indices spanning multiple 32-bit words. */
    subghz_ignore_set(0, true);
    subghz_ignore_set(31, true);
    subghz_ignore_set(32, true);
    subghz_ignore_set(63, true);
    subghz_ignore_set(127, true);

    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(0));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(31));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(32));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(63));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(127));
    TEST_ASSERT_EQUAL_UINT16(5, subghz_ignore_count());
}

void test_out_of_range_is_noop(void)
{
    subghz_ignore_set(SUBGHZ_IGNORE_MAX_PROTOCOLS, true);       /* == 128 */
    subghz_ignore_set(SUBGHZ_IGNORE_MAX_PROTOCOLS + 100, true);
    subghz_ignore_toggle(SUBGHZ_IGNORE_MAX_PROTOCOLS + 5);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_count());
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(SUBGHZ_IGNORE_MAX_PROTOCOLS));
}

void test_reset_clears_all(void)
{
    for (uint16_t i = 0; i < 20; i++)
        subghz_ignore_set(i, true);
    TEST_ASSERT_EQUAL_UINT16(20, subghz_ignore_count());
    subghz_ignore_reset();
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_count());
}

/*============================================================================*/
/* Hex serialization tests                                                    */
/*============================================================================*/

void test_serialize_empty(void)
{
    char buf[SUBGHZ_IGNORE_HEX_BUFSZ];
    size_t n = subghz_ignore_serialize_hex(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(SUBGHZ_IGNORE_HEX_LEN, n);
    /* All zero. */
    for (size_t i = 0; i < n; i++)
        TEST_ASSERT_EQUAL_CHAR('0', buf[i]);
}

void test_serialize_bit0(void)
{
    /* Bit 0 set → least-significant nibble of the last (rightmost) char. */
    subghz_ignore_set(0, true);
    char buf[SUBGHZ_IGNORE_HEX_BUFSZ];
    subghz_ignore_serialize_hex(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('1', buf[SUBGHZ_IGNORE_HEX_LEN - 1]);
    TEST_ASSERT_EQUAL_CHAR('0', buf[0]);
}

void test_serialize_high_bit(void)
{
    /* Bit 127 set → most-significant nibble of the first char = 0x8. */
    subghz_ignore_set(127, true);
    char buf[SUBGHZ_IGNORE_HEX_BUFSZ];
    subghz_ignore_serialize_hex(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('8', buf[0]);
    TEST_ASSERT_EQUAL_CHAR('0', buf[SUBGHZ_IGNORE_HEX_LEN - 1]);
}

void test_serialize_buffer_too_small(void)
{
    char buf[4];
    size_t n = subghz_ignore_serialize_hex(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(0, n);
    n = subghz_ignore_serialize_hex(NULL, 100);
    TEST_ASSERT_EQUAL_size_t(0, n);
}

void test_serialize_deserialize_roundtrip(void)
{
    const uint16_t bits[] = { 1, 2, 3, 7, 33, 64, 100, 127 };
    for (unsigned i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
        subghz_ignore_set(bits[i], true);

    char buf[SUBGHZ_IGNORE_HEX_BUFSZ];
    subghz_ignore_serialize_hex(buf, sizeof(buf));

    /* Wipe, then restore from the string. */
    subghz_ignore_reset();
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_count());
    subghz_ignore_deserialize_hex(buf);

    for (unsigned i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
        TEST_ASSERT_TRUE(subghz_ignore_is_ignored(bits[i]));
    TEST_ASSERT_EQUAL_UINT16(sizeof(bits) / sizeof(bits[0]), subghz_ignore_count());
}

void test_deserialize_null_and_empty_clear(void)
{
    subghz_ignore_set(3, true);
    subghz_ignore_deserialize_hex(NULL);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_count());

    subghz_ignore_set(3, true);
    subghz_ignore_deserialize_hex("");
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_count());
}

void test_deserialize_short_string(void)
{
    /* "3" → bits 0 and 1 set (0x3), nothing else. */
    subghz_ignore_deserialize_hex("3");
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(0));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(1));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(2));
    TEST_ASSERT_EQUAL_UINT16(2, subghz_ignore_count());
}

void test_deserialize_leading_whitespace(void)
{
    subghz_ignore_deserialize_hex("   1");
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(0));
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_count());
}

/*============================================================================*/
/* Name-based helper tests                                                    */
/*============================================================================*/

void test_set_name_and_query_name(void)
{
    TEST_ASSERT_TRUE(subghz_ignore_set_name("CAME", true));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored_name("CAME"));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(1));   /* CAME == index 1 */
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored_name("Princeton"));
}

void test_set_name_unknown_returns_false(void)
{
    TEST_ASSERT_FALSE(subghz_ignore_set_name("Nonexistent", true));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored_name("Nonexistent"));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored_name(NULL));
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_count());
}

/*============================================================================*/
/* main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_default_nothing_ignored);
    RUN_TEST(test_set_and_query);
    RUN_TEST(test_toggle);
    RUN_TEST(test_cross_word_boundary);
    RUN_TEST(test_out_of_range_is_noop);
    RUN_TEST(test_reset_clears_all);

    RUN_TEST(test_serialize_empty);
    RUN_TEST(test_serialize_bit0);
    RUN_TEST(test_serialize_high_bit);
    RUN_TEST(test_serialize_buffer_too_small);
    RUN_TEST(test_serialize_deserialize_roundtrip);
    RUN_TEST(test_deserialize_null_and_empty_clear);
    RUN_TEST(test_deserialize_short_string);
    RUN_TEST(test_deserialize_leading_whitespace);

    RUN_TEST(test_set_name_and_query_name);
    RUN_TEST(test_set_name_unknown_returns_false);

    return UNITY_END();
}
