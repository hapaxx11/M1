/* See COPYING.txt for license details. */

/*
 * test_subghz_read_raw_status.c
 *
 * Regression tests for subghz_read_raw_start_err_line1() — the pure-logic
 * mapping that turns a Read Raw start-error code into the status line shown on
 * the "Record failed" message box.
 *
 * Before issue #610 the scene collapsed every SD failure into a single
 * "SD card error" string, so a heap shortage (write-buffer alloc, ret 2) was
 * indistinguishable from a genuine SD/FatFs fault (ret 1).  These tests pin the
 * three distinct messages so the cause stays visible on-device.
 */

#include "unity.h"
#include "subghz_read_raw_status.h"
#include <string.h>

void setUp(void)  { }
void tearDown(void) { }

void test_oom_maps_to_low_memory(void)
{
    TEST_ASSERT_EQUAL_STRING("Low memory",
                             subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_ERR_OOM));
}

void test_sd_maps_to_sd_card_error(void)
{
    TEST_ASSERT_EQUAL_STRING("SD card error",
                             subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_ERR_SD));
}

void test_mem_maps_to_sd_mem_error(void)
{
    TEST_ASSERT_EQUAL_STRING("SD mem error",
                             subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_ERR_MEM));
}

/* The SD/FatFs fault and the SD write-buffer (heap) fault MUST NOT share the
 * same message — that ambiguity is exactly what issue #610 reported. */
void test_sd_and_mem_messages_differ(void)
{
    const char *sd  = subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_ERR_SD);
    const char *mem = subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_ERR_MEM);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(sd, mem));
}

void test_ok_returns_empty_string(void)
{
    TEST_ASSERT_EQUAL_STRING("", subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_OK));
}

/* Never return NULL — the message box dereferences the pointer. */
void test_all_codes_return_nonnull(void)
{
    TEST_ASSERT_NOT_NULL(subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_OK));
    TEST_ASSERT_NOT_NULL(subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_ERR_OOM));
    TEST_ASSERT_NOT_NULL(subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_ERR_SD));
    TEST_ASSERT_NOT_NULL(subghz_read_raw_start_err_line1(SUBGHZ_READ_RAW_START_ERR_MEM));
    /* Out-of-range value must still yield a safe, non-NULL string. */
    TEST_ASSERT_NOT_NULL(subghz_read_raw_start_err_line1((subghz_read_raw_start_err_t)99));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_oom_maps_to_low_memory);
    RUN_TEST(test_sd_maps_to_sd_card_error);
    RUN_TEST(test_mem_maps_to_sd_mem_error);
    RUN_TEST(test_sd_and_mem_messages_differ);
    RUN_TEST(test_ok_returns_empty_string);
    RUN_TEST(test_all_codes_return_nonnull);
    return UNITY_END();
}
