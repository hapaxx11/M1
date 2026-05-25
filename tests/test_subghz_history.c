/* See COPYING.txt for license details. */

/*
 * test_subghz_history.c
 *
 * Unit tests for the SubGhz signal history ring buffer.
 *
 * Tests subghz_history_add(), subghz_history_get(), and
 * subghz_history_reset() — pure data-structure logic with
 * no hardware dependencies.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"

void setUp(void) {}
void tearDown(void) {}

/* Helper — build a Dec_Info with specific key/protocol/bit_len */
static SubGHz_Dec_Info_t make_info(uint64_t key, uint16_t proto, uint16_t bits)
{
    SubGHz_Dec_Info_t info;
    memset(&info, 0, sizeof(info));
    info.key      = key;
    info.protocol = proto;
    info.bit_len  = bits;
    info.rssi     = -50;
    return info;
}

/* ===================================================================
 * subghz_history_reset — basic state
 * =================================================================== */

void test_reset_zeros_count_and_head(void)
{
    SubGHz_History_t hist;
    hist.count = 42;
    hist.head  = 17;
    subghz_history_reset(&hist);
    TEST_ASSERT_EQUAL_UINT8(0, hist.count);
    TEST_ASSERT_EQUAL_UINT8(0, hist.head);
}

/* ===================================================================
 * subghz_history_add — basic insertion
 * =================================================================== */

void test_add_single_entry(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0xABCDEF, PRINCETON, 24);
    uint8_t count = subghz_history_add(&hist, &info, 433920000);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT8(1, hist.count);
}

void test_add_stores_frequency(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0x1234, PRINCETON, 24);
    subghz_history_add(&hist, &info, 315000000);

    const SubGHz_History_Entry_t *entry = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(315000000, entry->frequency);
}

void test_add_clears_raw_data_ptr(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0xFF, CAME_12BIT, 12);
    uint16_t fake_data[4] = {100, 200, 300, 400};
    info.raw_data = fake_data;
    info.raw = true;

    subghz_history_add(&hist, &info, 433920000);

    const SubGHz_History_Entry_t *entry = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NULL(entry->info.raw_data);
    TEST_ASSERT_FALSE(entry->info.raw);
}

void test_add_initial_count_is_one(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0xAA, PRINCETON, 24);
    subghz_history_add(&hist, &info, 433920000);

    const SubGHz_History_Entry_t *entry = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(1, entry->count);
}

/* ===================================================================
 * subghz_history_add — duplicate detection
 * =================================================================== */

void test_dup_increments_count(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0xABCD, PRINCETON, 24);
    subghz_history_add(&hist, &info, 433920000);
    subghz_history_add(&hist, &info, 433920000);
    subghz_history_add(&hist, &info, 433920000);

    /* Should still be 1 entry, not 3 */
    TEST_ASSERT_EQUAL_UINT8(1, hist.count);

    const SubGHz_History_Entry_t *entry = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(3, entry->count);
}

void test_dup_updates_rssi(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0xABCD, PRINCETON, 24);
    info.rssi = -80;
    subghz_history_add(&hist, &info, 433920000);

    info.rssi = -40;
    subghz_history_add(&hist, &info, 433920000);

    const SubGHz_History_Entry_t *entry = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT16(-40, entry->info.rssi);
}

void test_different_key_not_dup(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info1 = make_info(0x1111, PRINCETON, 24);
    SubGHz_Dec_Info_t info2 = make_info(0x2222, PRINCETON, 24);

    subghz_history_add(&hist, &info1, 433920000);
    subghz_history_add(&hist, &info2, 433920000);

    TEST_ASSERT_EQUAL_UINT8(2, hist.count);
}

void test_different_protocol_not_dup(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info1 = make_info(0x1111, PRINCETON, 24);
    SubGHz_Dec_Info_t info2 = make_info(0x1111, CAME_12BIT, 24);

    subghz_history_add(&hist, &info1, 433920000);
    subghz_history_add(&hist, &info2, 433920000);

    TEST_ASSERT_EQUAL_UINT8(2, hist.count);
}

void test_different_bit_len_not_dup(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info1 = make_info(0x1111, PRINCETON, 24);
    SubGHz_Dec_Info_t info2 = make_info(0x1111, PRINCETON, 32);

    subghz_history_add(&hist, &info1, 433920000);
    subghz_history_add(&hist, &info2, 433920000);

    TEST_ASSERT_EQUAL_UINT8(2, hist.count);
}

void test_dup_count_saturates_at_255(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0xAA, PRINCETON, 24);
    for (int i = 0; i < 300; i++)
        subghz_history_add(&hist, &info, 433920000);

    const SubGHz_History_Entry_t *entry = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(255, entry->count);
}

/* ===================================================================
 * subghz_history_get — ordering
 * =================================================================== */

void test_get_index_0_is_most_recent(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info1 = make_info(0x1111, PRINCETON, 24);
    SubGHz_Dec_Info_t info2 = make_info(0x2222, CAME_12BIT, 12);
    SubGHz_Dec_Info_t info3 = make_info(0x3333, NICE_FLO, 12);

    subghz_history_add(&hist, &info1, 433920000);
    subghz_history_add(&hist, &info2, 315000000);
    subghz_history_add(&hist, &info3, 868000000);

    const SubGHz_History_Entry_t *e0 = subghz_history_get(&hist, 0);
    const SubGHz_History_Entry_t *e1 = subghz_history_get(&hist, 1);
    const SubGHz_History_Entry_t *e2 = subghz_history_get(&hist, 2);

    TEST_ASSERT_EQUAL_UINT64(0x3333, e0->info.key);
    TEST_ASSERT_EQUAL_UINT64(0x2222, e1->info.key);
    TEST_ASSERT_EQUAL_UINT64(0x1111, e2->info.key);
}

void test_get_out_of_range_returns_null(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0x1111, PRINCETON, 24);
    subghz_history_add(&hist, &info, 433920000);

    TEST_ASSERT_NULL(subghz_history_get(&hist, 1));
    TEST_ASSERT_NULL(subghz_history_get(&hist, 50));
    TEST_ASSERT_NULL(subghz_history_get(&hist, 255));
}

void test_get_on_empty_returns_null(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);
    TEST_ASSERT_NULL(subghz_history_get(&hist, 0));
}

/* ===================================================================
 * Circular buffer overflow — more than SUBGHZ_HISTORY_MAX (50)
 * =================================================================== */

void test_overflow_wraps_and_evicts_oldest(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    /* Fill with 60 unique signals (10 more than max) */
    for (int i = 0; i < 60; i++)
    {
        SubGHz_Dec_Info_t info = make_info((uint64_t)(i + 1), PRINCETON, 24);
        subghz_history_add(&hist, &info, 433920000);
    }

    /* Count should saturate at SUBGHZ_HISTORY_MAX */
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_HISTORY_MAX, hist.count);

    /* Most recent = key 60 */
    const SubGHz_History_Entry_t *newest = subghz_history_get(&hist, 0);
    TEST_ASSERT_EQUAL_UINT64(60, newest->info.key);

    /* Oldest still available = key 11 (first 10 evicted) */
    const SubGHz_History_Entry_t *oldest = subghz_history_get(&hist, SUBGHZ_HISTORY_MAX - 1);
    TEST_ASSERT_EQUAL_UINT64(11, oldest->info.key);
}

void test_overflow_head_wraps_correctly(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    /* Fill exactly to capacity */
    for (int i = 0; i < SUBGHZ_HISTORY_MAX; i++)
    {
        SubGHz_Dec_Info_t info = make_info((uint64_t)(i + 1), PRINCETON, 24);
        subghz_history_add(&hist, &info, 433920000);
    }

    /* Head should have wrapped to 0 */
    TEST_ASSERT_EQUAL_UINT8(0, hist.head);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_HISTORY_MAX, hist.count);

    /* Add one more — overwrites slot 0, head becomes 1 */
    SubGHz_Dec_Info_t extra = make_info(0x9999, PRINCETON, 24);
    subghz_history_add(&hist, &extra, 433920000);

    TEST_ASSERT_EQUAL_UINT8(1, hist.head);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_HISTORY_MAX, hist.count);

    /* Most recent is the extra one */
    const SubGHz_History_Entry_t *newest = subghz_history_get(&hist, 0);
    TEST_ASSERT_EQUAL_UINT64(0x9999, newest->info.key);
}

/* ===================================================================
 * Extended fields — serial_number, rolling_code, button_id
 * =================================================================== */

void test_add_preserves_extended_fields(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0xDEAD, KEELOQ, 66);
    info.serial_number = 0x12345678;
    info.rolling_code  = 0xABCDABCD;
    info.button_id     = 3;

    subghz_history_add(&hist, &info, 433920000);

    const SubGHz_History_Entry_t *entry = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, entry->info.serial_number);
    TEST_ASSERT_EQUAL_UINT32(0xABCDABCD, entry->info.rolling_code);
    TEST_ASSERT_EQUAL_UINT8(3, entry->info.button_id);
}

/* ===================================================================
 * Dup detection only checks most recent entry (not all)
 * =================================================================== */

void test_dup_only_checks_last_entry(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t a = make_info(0xAAAA, PRINCETON, 24);
    SubGHz_Dec_Info_t b = make_info(0xBBBB, PRINCETON, 24);

    /* Add A, B, then A again — A shouldn't dedup because B is most recent */
    subghz_history_add(&hist, &a, 433920000);
    subghz_history_add(&hist, &b, 433920000);
    subghz_history_add(&hist, &a, 433920000);

    TEST_ASSERT_EQUAL_UINT8(3, hist.count);
}

/* ===================================================================
 * Phase 12 — subghz_history_add_ex toggle semantics
 * =================================================================== */

/* remove_duplicates=true matches the default subghz_history_add() behaviour:
 * a duplicate (same proto/key/bit_len as the most recent entry) merges. */
void test_ex_remove_dups_on_matches_default(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0x123, PRINCETON, 24);
    subghz_history_add_ex(&hist, &info, 433920000, true, true);
    uint8_t cnt = subghz_history_add_ex(&hist, &info, 433920000, true, true);

    TEST_ASSERT_EQUAL_UINT8(1, cnt);
    TEST_ASSERT_EQUAL_UINT8(1, hist.count);
    const SubGHz_History_Entry_t *e = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(2, e->count);   /* merged duplicate */
}

/* remove_duplicates=false: every reception adds a new entry, even when
 * the most recent matches on (protocol, key, bit_len). */
void test_ex_remove_dups_off_adds_new_entry(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    SubGHz_Dec_Info_t info = make_info(0x123, PRINCETON, 24);
    subghz_history_add_ex(&hist, &info, 433920000, false, true);
    uint8_t cnt = subghz_history_add_ex(&hist, &info, 433920000, false, true);

    TEST_ASSERT_EQUAL_UINT8(2, cnt);
    TEST_ASSERT_EQUAL_UINT8(2, hist.count);
    /* Each entry should have count=1 because dedup was disabled. */
    const SubGHz_History_Entry_t *e0 = subghz_history_get(&hist, 0);
    const SubGHz_History_Entry_t *e1 = subghz_history_get(&hist, 1);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_UINT8(1, e0->count);
    TEST_ASSERT_EQUAL_UINT8(1, e1->count);
}

/* delete_old_signals=true: full ring evicts the oldest entry on the next
 * insert (the default subghz_history_add() behaviour). */
void test_ex_delete_old_on_evicts_oldest(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    /* Fill ring with SUBGHZ_HISTORY_MAX distinct entries. */
    for (uint16_t i = 0; i < SUBGHZ_HISTORY_MAX; i++)
    {
        SubGHz_Dec_Info_t info = make_info((uint64_t)(0x1000 + i), PRINCETON, 24);
        subghz_history_add_ex(&hist, &info, 433920000, true, true);
    }
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_HISTORY_MAX, hist.count);

    /* Insert one more; with delete_old_signals=true the count saturates
     * at SUBGHZ_HISTORY_MAX but the newest entry replaces the oldest. */
    SubGHz_Dec_Info_t newest = make_info(0xDEADBEEF, PRINCETON, 24);
    uint8_t cnt = subghz_history_add_ex(&hist, &newest, 433920000, true, true);

    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_HISTORY_MAX, cnt);
    const SubGHz_History_Entry_t *e0 = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)0xDEADBEEF, e0->info.key);  /* newest is index 0 */
    /* Oldest must have shifted off — the key 0x1000 must no longer appear. */
    bool oldest_found = false;
    for (uint8_t i = 0; i < hist.count; i++)
    {
        const SubGHz_History_Entry_t *e = subghz_history_get(&hist, i);
        if (e && e->info.key == (uint64_t)0x1000)
        {
            oldest_found = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(oldest_found);
}

/* delete_old_signals=false: full ring drops the new entry instead of
 * evicting the oldest.  The original oldest must be preserved. */
void test_ex_delete_old_off_drops_new_when_full(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    /* Fill ring with SUBGHZ_HISTORY_MAX distinct entries. */
    for (uint16_t i = 0; i < SUBGHZ_HISTORY_MAX; i++)
    {
        SubGHz_Dec_Info_t info = make_info((uint64_t)(0x1000 + i), PRINCETON, 24);
        subghz_history_add_ex(&hist, &info, 433920000, true, false);
    }
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_HISTORY_MAX, hist.count);

    /* Snapshot the current oldest key so we can verify it survives. */
    const SubGHz_History_Entry_t *oldest_before =
        subghz_history_get(&hist, SUBGHZ_HISTORY_MAX - 1);
    TEST_ASSERT_NOT_NULL(oldest_before);
    uint64_t oldest_key = oldest_before->info.key;

    /* Insert one more with delete_old=false — must be a no-op. */
    SubGHz_Dec_Info_t rejected = make_info(0xDEADBEEF, PRINCETON, 24);
    uint8_t cnt = subghz_history_add_ex(&hist, &rejected, 433920000, true, false);

    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_HISTORY_MAX, cnt);

    /* Newest (idx 0) is still the last successful insert, not 0xDEADBEEF. */
    const SubGHz_History_Entry_t *e0 = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_EQUAL((uint64_t)0xDEADBEEF, e0->info.key);

    /* The oldest entry must still be present. */
    const SubGHz_History_Entry_t *oldest_after =
        subghz_history_get(&hist, SUBGHZ_HISTORY_MAX - 1);
    TEST_ASSERT_NOT_NULL(oldest_after);
    TEST_ASSERT_EQUAL_UINT64(oldest_key, oldest_after->info.key);
}

/* delete_old=false still allows duplicate-merge to fire when the ring is
 * full — the merge path returns before the overflow check, so an incoming
 * duplicate of the most recent entry still increments its count.  This is
 * a deliberate property: dedup is independent of the eviction policy. */
void test_ex_delete_old_off_allows_dup_merge_when_full(void)
{
    SubGHz_History_t hist;
    subghz_history_reset(&hist);

    /* Fill with distinct entries. */
    for (uint16_t i = 0; i < SUBGHZ_HISTORY_MAX; i++)
    {
        SubGHz_Dec_Info_t info = make_info((uint64_t)(0x1000 + i), PRINCETON, 24);
        subghz_history_add_ex(&hist, &info, 433920000, true, false);
    }
    /* Most recent entry has key = 0x1000 + SUBGHZ_HISTORY_MAX - 1. */
    uint64_t newest_key = (uint64_t)(0x1000 + SUBGHZ_HISTORY_MAX - 1);

    /* Sending the same key again must merge (count++) even though the ring
     * is full and delete_old=false. */
    SubGHz_Dec_Info_t dup = make_info(newest_key, PRINCETON, 24);
    subghz_history_add_ex(&hist, &dup, 433920000, true, false);

    const SubGHz_History_Entry_t *e0 = subghz_history_get(&hist, 0);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_EQUAL_UINT64(newest_key, e0->info.key);
    TEST_ASSERT_EQUAL_UINT8(2, e0->count);  /* merged */
}

/* subghz_history_add() (the 3-arg wrapper) must be equivalent to
 * subghz_history_add_ex(..., true, true) — guard against drift. */
void test_default_wrapper_matches_ex_true_true(void)
{
    SubGHz_History_t a, b;
    subghz_history_reset(&a);
    subghz_history_reset(&b);

    for (uint16_t i = 0; i < 5; i++)
    {
        SubGHz_Dec_Info_t info = make_info((uint64_t)(0x100 + i), PRINCETON, 24);
        subghz_history_add(&a, &info, 433920000);
        subghz_history_add_ex(&b, &info, 433920000, true, true);
    }
    /* Add a duplicate of the most recent to exercise dedup. */
    SubGHz_Dec_Info_t dup = make_info((uint64_t)(0x100 + 4), PRINCETON, 24);
    subghz_history_add(&a, &dup, 433920000);
    subghz_history_add_ex(&b, &dup, 433920000, true, true);

    TEST_ASSERT_EQUAL_UINT8(a.count, b.count);
    TEST_ASSERT_EQUAL_UINT8(a.head,  b.head);
    for (uint8_t i = 0; i < a.count; i++)
    {
        const SubGHz_History_Entry_t *ea = subghz_history_get(&a, i);
        const SubGHz_History_Entry_t *eb = subghz_history_get(&b, i);
        TEST_ASSERT_NOT_NULL(ea);
        TEST_ASSERT_NOT_NULL(eb);
        TEST_ASSERT_EQUAL_UINT64(ea->info.key, eb->info.key);
        TEST_ASSERT_EQUAL_UINT8(ea->count, eb->count);
    }
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Reset */
    RUN_TEST(test_reset_zeros_count_and_head);

    /* Basic add */
    RUN_TEST(test_add_single_entry);
    RUN_TEST(test_add_stores_frequency);
    RUN_TEST(test_add_clears_raw_data_ptr);
    RUN_TEST(test_add_initial_count_is_one);

    /* Duplicate detection */
    RUN_TEST(test_dup_increments_count);
    RUN_TEST(test_dup_updates_rssi);
    RUN_TEST(test_different_key_not_dup);
    RUN_TEST(test_different_protocol_not_dup);
    RUN_TEST(test_different_bit_len_not_dup);
    RUN_TEST(test_dup_count_saturates_at_255);

    /* Get ordering */
    RUN_TEST(test_get_index_0_is_most_recent);
    RUN_TEST(test_get_out_of_range_returns_null);
    RUN_TEST(test_get_on_empty_returns_null);

    /* Overflow */
    RUN_TEST(test_overflow_wraps_and_evicts_oldest);
    RUN_TEST(test_overflow_head_wraps_correctly);

    /* Extended fields */
    RUN_TEST(test_add_preserves_extended_fields);

    /* Dup edge case */
    RUN_TEST(test_dup_only_checks_last_entry);

    /* Phase 12 — _ex toggle semantics */
    RUN_TEST(test_ex_remove_dups_on_matches_default);
    RUN_TEST(test_ex_remove_dups_off_adds_new_entry);
    RUN_TEST(test_ex_delete_old_on_evicts_oldest);
    RUN_TEST(test_ex_delete_old_off_drops_new_when_full);
    RUN_TEST(test_ex_delete_old_off_allows_dup_merge_when_full);
    RUN_TEST(test_default_wrapper_matches_ex_true_true);

    return UNITY_END();
}
