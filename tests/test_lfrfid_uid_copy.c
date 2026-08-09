/* See COPYING.txt for license details. */

/*
 * test_lfrfid_uid_copy.c — regression test for the .rfid "HData" clamp bug:
 * lfrfid_profile_load() used to memcpy() the full decoded hex length straight
 * into LFRFIDTagInfo.uid[5] without checking it against the buffer size, so any
 * .rfid file with more than 5 data bytes (HID=6, AWID=9, FDX-B=11, GProxII=12,
 * ...) overflowed into adjacent struct fields / .bss. lfrfid_uid_copy_len()
 * is the extracted clamp that fixes it.
 */

#include "unity.h"
#include "lfrfid_uid_copy.h"

void setUp(void) { }
void tearDown(void) { }

void test_uid_copy_len_within_capacity_is_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT(5, lfrfid_uid_copy_len(5, 5));
    TEST_ASSERT_EQUAL_UINT(3, lfrfid_uid_copy_len(3, 5));
    TEST_ASSERT_EQUAL_UINT(0, lfrfid_uid_copy_len(0, 5));
}

void test_uid_copy_len_over_capacity_is_clamped(void)
{
    /* Regression case: HID (6 bytes) and larger protocols into uid[5]. */
    TEST_ASSERT_EQUAL_UINT(5, lfrfid_uid_copy_len(6, 5));
    TEST_ASSERT_EQUAL_UINT(5, lfrfid_uid_copy_len(12, 5));
    TEST_ASSERT_EQUAL_UINT(5, lfrfid_uid_copy_len(200, 5));
}

void test_uid_copy_len_zero_capacity(void)
{
    TEST_ASSERT_EQUAL_UINT(0, lfrfid_uid_copy_len(0, 0));
    TEST_ASSERT_EQUAL_UINT(0, lfrfid_uid_copy_len(10, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uid_copy_len_within_capacity_is_unchanged);
    RUN_TEST(test_uid_copy_len_over_capacity_is_clamped);
    RUN_TEST(test_uid_copy_len_zero_capacity);
    return UNITY_END();
}
