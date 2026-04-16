/* See COPYING.txt for license details. */

/*
 * test_hex_viewer.c
 *
 * Unit tests for hex_viewer.c using the Unity test framework.
 * Tests the pure-logic formatting functions: hex_viewer_format_row()
 * and hex_viewer_ascii_preview().
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "hex_viewer.h"
#include <string.h>

/* Unity setup / teardown (required stubs) */
void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * hex_viewer_format_row tests
 * =================================================================== */

void test_format_row_full_6_bytes(void)
{
    const uint8_t data[] = { 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x21 };
    char out[32];

    hex_viewer_format_row(data, 6, 0x0000, out, sizeof(out));
    /* Expected: "0000 48656C6C6F21" */
    TEST_ASSERT_EQUAL_STRING("0000 48656C6C6F21", out);
}

void test_format_row_partial_3_bytes(void)
{
    const uint8_t data[] = { 0xAA, 0xBB, 0xCC };
    char out[32];

    hex_viewer_format_row(data, 3, 0x0006, out, sizeof(out));
    /* Expected: "0006 AABBCC" */
    TEST_ASSERT_EQUAL_STRING("0006 AABBCC", out);
}

void test_format_row_single_byte(void)
{
    const uint8_t data[] = { 0xFF };
    char out[32];

    hex_viewer_format_row(data, 1, 0x00FF, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("00FF FF", out);
}

void test_format_row_zero_bytes(void)
{
    char out[32];
    memset(out, 'X', sizeof(out));

    hex_viewer_format_row(NULL, 0, 0x0010, out, sizeof(out));
    /* Should just have the address prefix "0010 " */
    TEST_ASSERT_EQUAL_STRING("0010 ", out);
}

void test_format_row_offset_wraps_16bit(void)
{
    const uint8_t data[] = { 0x01 };
    char out[32];

    /* Offset 0x12345 should display as "2345" (only low 16 bits) */
    hex_viewer_format_row(data, 1, 0x12345, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("2345 01", out);
}

void test_format_row_small_buffer(void)
{
    const uint8_t data[] = { 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x21 };
    char out[12];  /* Small buffer — will truncate */

    hex_viewer_format_row(data, 6, 0x0000, out, sizeof(out));
    /* Buffer is 12 bytes, NUL-terminated.  At minimum the address prefix fits. */
    TEST_ASSERT_EQUAL_UINT8('\0', out[11]);
    /* Should start with address */
    TEST_ASSERT_EQUAL_UINT8('0', out[0]);
}

void test_format_row_zero_buffer(void)
{
    /* Edge case: out_len == 0 should not crash */
    char out[1] = { 'X' };
    hex_viewer_format_row(NULL, 0, 0, out, 0);
    /* Buffer untouched */
    TEST_ASSERT_EQUAL_UINT8('X', out[0]);
}

/* ===================================================================
 * hex_viewer_ascii_preview tests
 * =================================================================== */

void test_ascii_preview_printable(void)
{
    const uint8_t data[] = { 'H', 'e', 'l', 'l', 'o', '!' };
    char out[16];

    hex_viewer_ascii_preview(data, 6, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello!", out);
}

void test_ascii_preview_mixed(void)
{
    const uint8_t data[] = { 'A', 0x00, 'B', 0x1F, 'C', 0x7F };
    char out[16];

    hex_viewer_ascii_preview(data, 6, out, sizeof(out));
    /* Non-printable bytes become '.' */
    TEST_ASSERT_EQUAL_STRING("A.B.C.", out);
}

void test_ascii_preview_all_nonprintable(void)
{
    const uint8_t data[] = { 0x00, 0x01, 0x02, 0x80, 0xFF };
    char out[16];

    hex_viewer_ascii_preview(data, 5, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING(".....", out);
}

void test_ascii_preview_empty(void)
{
    char out[16];
    memset(out, 'X', sizeof(out));

    hex_viewer_ascii_preview(NULL, 0, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_ascii_preview_truncated_by_buffer(void)
{
    const uint8_t data[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G' };
    char out[4];  /* Only room for 3 chars + NUL */

    hex_viewer_ascii_preview(data, 7, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("ABC", out);
}

void test_ascii_preview_zero_buffer(void)
{
    /* Edge case: out_len == 0 should not crash */
    char out[1] = { 'X' };
    hex_viewer_ascii_preview(NULL, 0, out, 0);
    /* Buffer untouched */
    TEST_ASSERT_EQUAL_UINT8('X', out[0]);
}

void test_ascii_preview_space_is_printable(void)
{
    const uint8_t data[] = { ' ', '~', 0x7E };
    char out[16];

    hex_viewer_ascii_preview(data, 3, out, sizeof(out));
    /* Space (0x20) and tilde (0x7E) are printable */
    TEST_ASSERT_EQUAL_STRING(" ~~", out);
}

/* ===================================================================
 * main
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* hex_viewer_format_row */
    RUN_TEST(test_format_row_full_6_bytes);
    RUN_TEST(test_format_row_partial_3_bytes);
    RUN_TEST(test_format_row_single_byte);
    RUN_TEST(test_format_row_zero_bytes);
    RUN_TEST(test_format_row_offset_wraps_16bit);
    RUN_TEST(test_format_row_small_buffer);
    RUN_TEST(test_format_row_zero_buffer);

    /* hex_viewer_ascii_preview */
    RUN_TEST(test_ascii_preview_printable);
    RUN_TEST(test_ascii_preview_mixed);
    RUN_TEST(test_ascii_preview_all_nonprintable);
    RUN_TEST(test_ascii_preview_empty);
    RUN_TEST(test_ascii_preview_truncated_by_buffer);
    RUN_TEST(test_ascii_preview_zero_buffer);
    RUN_TEST(test_ascii_preview_space_is_printable);

    return UNITY_END();
}
