/* See COPYING.txt for license details. */

/*
 * test_subghz_autosave.c
 *
 * Host-side unit tests for subghz_autosave.c — filename generation
 * and duplicate detection logic.
 */

#include "unity.h"
#include "subghz_autosave.h"
#include <string.h>

void setUp(void)
{
    subghz_autosave_dup_reset();
}

void tearDown(void) {}

/*═══════════════ Filename generation ═══════════════*/

void test_make_path_sub_format(void)
{
    char buf[80];
    int n = subghz_autosave_make_path(buf, sizeof(buf), "Princeton", 0xABC, 12345, false);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/autosave/Princeton_ABC_12345.sub", buf);
}

void test_make_path_sgh_format(void)
{
    char buf[80];
    int n = subghz_autosave_make_path(buf, sizeof(buf), "CAME", 0xFF, 999, true);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/autosave/CAME_FF_999.sgh", buf);
}

void test_make_path_zero_key(void)
{
    char buf[80];
    int n = subghz_autosave_make_path(buf, sizeof(buf), "Nice", 0, 100, false);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/autosave/Nice_0_100.sub", buf);
}

void test_make_path_null_protocol(void)
{
    char buf[80];
    int n = subghz_autosave_make_path(buf, sizeof(buf), NULL, 0xABC, 0, false);
    TEST_ASSERT_EQUAL(0, n);
}

void test_make_path_null_buffer(void)
{
    int n = subghz_autosave_make_path(NULL, 80, "X", 1, 0, false);
    TEST_ASSERT_EQUAL(0, n);
}

void test_make_path_buffer_too_small(void)
{
    char buf[10];
    int n = subghz_autosave_make_path(buf, sizeof(buf), "Princeton", 0xABC, 12345, false);
    TEST_ASSERT_EQUAL(0, n);
}

void test_make_path_large_key(void)
{
    char buf[80];
    int n = subghz_autosave_make_path(buf, sizeof(buf), "KeeLoq", 0xDEADBEEF, 42, false);
    TEST_ASSERT_GREATER_THAN(0, n);
    /* Key is truncated to 32-bit by (uint32_t) cast */
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/autosave/KeeLoq_DEADBEEF_42.sub", buf);
}

void test_make_path_max_tick(void)
{
    char buf[100];
    int n = subghz_autosave_make_path(buf, sizeof(buf), "P", 1, 4294967295UL, false);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/autosave/P_1_4294967295.sub", buf);
}

/*═══════════════ Duplicate detection ═══════════════*/

void test_dup_first_is_not_duplicate(void)
{
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Princeton", 0xABC));
}

void test_dup_same_is_duplicate(void)
{
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Princeton", 0xABC));
    subghz_autosave_record("Princeton", 0xABC);
    TEST_ASSERT_TRUE(subghz_autosave_is_duplicate("Princeton", 0xABC));
}

void test_dup_different_key_is_not_duplicate(void)
{
    subghz_autosave_record("Princeton", 0xABC);
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Princeton", 0xDEF));
}

void test_dup_different_protocol_is_not_duplicate(void)
{
    subghz_autosave_record("Princeton", 0xABC);
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("CAME", 0xABC));
}

void test_dup_reset_clears(void)
{
    subghz_autosave_record("Princeton", 0xABC);
    subghz_autosave_dup_reset();
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Princeton", 0xABC));
}

void test_dup_ring_wraps(void)
{
    /* Fill the ring with 32 unique entries */
    for (uint64_t i = 0; i < 32; i++)
    {
        TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Proto", i));
        subghz_autosave_record("Proto", i);
    }

    /* All 32 should be detected as duplicates */
    for (uint64_t i = 0; i < 32; i++)
        TEST_ASSERT_TRUE(subghz_autosave_is_duplicate("Proto", i));

    /* Adding a 33rd entry evicts the oldest (key=0) */
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Proto", 99));
    subghz_autosave_record("Proto", 99);

    /* key=0 was evicted, so it's no longer a duplicate */
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Proto", 0));
}

void test_dup_null_protocol_returns_true(void)
{
    /* NULL protocol should be treated as duplicate (prevent save) */
    TEST_ASSERT_TRUE(subghz_autosave_is_duplicate(NULL, 0xABC));
}

void test_dup_check_without_record_not_duplicate(void)
{
    /* Check without recording — second check should still say "not duplicate" */
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Princeton", 0xABC));
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Princeton", 0xABC));
    /* Only after recording should it become duplicate */
    subghz_autosave_record("Princeton", 0xABC);
    TEST_ASSERT_TRUE(subghz_autosave_is_duplicate("Princeton", 0xABC));
}

void test_dup_hash_collision_not_false_positive(void)
{
    /* Even if two protocol names happened to produce the same djb2 hash,
     * the secondary strcmp should prevent a false positive.  We can't
     * trivially forge a djb2 collision, but we verify that different
     * protocol names with the same key are NOT treated as duplicates
     * (the strcmp guard is the only thing that distinguishes them when
     * the hash alone would not). */
    subghz_autosave_record("CAME", 0x1234);
    TEST_ASSERT_FALSE(subghz_autosave_is_duplicate("Nice", 0x1234));
    TEST_ASSERT_TRUE(subghz_autosave_is_duplicate("CAME", 0x1234));
}

/*═══════════════ Main ═══════════════*/

int main(void)
{
    UNITY_BEGIN();

    /* Filename generation */
    RUN_TEST(test_make_path_sub_format);
    RUN_TEST(test_make_path_sgh_format);
    RUN_TEST(test_make_path_zero_key);
    RUN_TEST(test_make_path_null_protocol);
    RUN_TEST(test_make_path_null_buffer);
    RUN_TEST(test_make_path_buffer_too_small);
    RUN_TEST(test_make_path_large_key);
    RUN_TEST(test_make_path_max_tick);

    /* Duplicate detection */
    RUN_TEST(test_dup_first_is_not_duplicate);
    RUN_TEST(test_dup_same_is_duplicate);
    RUN_TEST(test_dup_different_key_is_not_duplicate);
    RUN_TEST(test_dup_different_protocol_is_not_duplicate);
    RUN_TEST(test_dup_reset_clears);
    RUN_TEST(test_dup_ring_wraps);
    RUN_TEST(test_dup_null_protocol_returns_true);
    RUN_TEST(test_dup_check_without_record_not_duplicate);
    RUN_TEST(test_dup_hash_collision_not_false_positive);

    return UNITY_END();
}
