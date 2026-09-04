/* See COPYING.txt for license details. */

/**
 * @file   test_ir_cust_name.c
 * @brief  Host-side unit tests for ir_cust_name.c sanitization helper.
 */

#include "unity.h"
#include "ir_cust_name.h"
#include <string.h>

void setUp(void) { }
void tearDown(void) { }

void test_sanitize_passes_through_simple_name(void)
{
    char out[32];
    ir_cust_sanitize_name("My Remote", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("My Remote", out);
}

void test_sanitize_replaces_reserved_chars(void)
{
    char out[32];
    ir_cust_sanitize_name("a/b\\c:d*e?f\"g<h>i|j", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("a_b_c_d_e_f_g_h_i_j", out);
}

void test_sanitize_replaces_control_chars(void)
{
    char out[32];
    ir_cust_sanitize_name("hello\tworld", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("hello_world", out);
}

void test_sanitize_trims_trailing_spaces_and_dots(void)
{
    char out[32];
    ir_cust_sanitize_name("remote ", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("remote", out);

    ir_cust_sanitize_name("remote.", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("remote", out);

    ir_cust_sanitize_name("remote .", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("remote", out);
}

void test_sanitize_empty_input_falls_back(void)
{
    char out[32];
    ir_cust_sanitize_name("", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Remote", out);
}

void test_sanitize_only_whitespace_falls_back(void)
{
    char out[32];
    ir_cust_sanitize_name("   ", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Remote", out);
}

void test_sanitize_null_input_falls_back(void)
{
    char out[32];
    ir_cust_sanitize_name(NULL, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Remote", out);
}

void test_sanitize_respects_buffer_size(void)
{
    char out[8];
    ir_cust_sanitize_name("VeryLongRemoteName", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("VeryLon", out);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sanitize_passes_through_simple_name);
    RUN_TEST(test_sanitize_replaces_reserved_chars);
    RUN_TEST(test_sanitize_replaces_control_chars);
    RUN_TEST(test_sanitize_trims_trailing_spaces_and_dots);
    RUN_TEST(test_sanitize_empty_input_falls_back);
    RUN_TEST(test_sanitize_only_whitespace_falls_back);
    RUN_TEST(test_sanitize_null_input_falls_back);
    RUN_TEST(test_sanitize_respects_buffer_size);
    return UNITY_END();
}
