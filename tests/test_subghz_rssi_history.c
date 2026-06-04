/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_rssi_history.c
 * @brief  Regression tests for the Sub-GHz Read Raw RSSI spectrogram logic.
 *
 *  Tests subghz_rssi_history_push() and subghz_rssi_history_reset() from
 *  m1_csrc/subghz_rssi_history.h — the pure-logic module extracted from
 *  m1_sub_ghz.c so it can be exercised without pulling in hardware headers.
 *
 *  Regression target (issue "Read Raw graph" / v0.9.1.48):
 *    subghz_rssi_history_push(trace=false) wrote u_rssi=0 to the last
 *    committed history slot whenever RSSI fell below THRESHOLD_MIN (-90 dBm).
 *    Signal that briefly exceeded the capture threshold and then dropped back
 *    to the noise floor erased every committed bar, producing an empty waveform
 *    despite 8192+ samples captured to SD card.
 *
 *    The fix: only update the last committed slot when u_rssi > 0.
 */

#include "unity.h"
#include "subghz_rssi_history.h"

static SubghzRssiHistory h;

void setUp(void)    { subghz_rssi_history_reset(&h); }
void tearDown(void) { }

/* =========================================================================
 * subghz_rssi_to_u8 — conversion helper
 * ====================================================================== */

void test_rssi_to_u8_at_minimum_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_to_u8(SUBGHZ_RSSI_THRESHOLD_MIN));
}

void test_rssi_to_u8_below_minimum_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_to_u8(-91.0f));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_to_u8(-120.0f));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_to_u8(-200.0f));
}

void test_rssi_to_u8_above_minimum_is_nonzero(void)
{
    /* -80 dBm: 10 / 1.8 ≈ 5 */
    TEST_ASSERT_GREATER_THAN_UINT8(0, subghz_rssi_to_u8(-80.0f));
    /* -70 dBm: 20 / 1.8 ≈ 11 */
    TEST_ASSERT_GREATER_THAN_UINT8(0, subghz_rssi_to_u8(-70.0f));
}

void test_rssi_to_u8_increases_with_signal_strength(void)
{
    /* Stronger signal (less negative) → taller bar */
    TEST_ASSERT_GREATER_THAN_UINT8(subghz_rssi_to_u8(-80.0f),
                                   subghz_rssi_to_u8(-70.0f));
}

/* =========================================================================
 * subghz_rssi_history_reset
 * ====================================================================== */

void test_reset_zeroes_all_state(void)
{
    /* Dirty the struct first */
    h.buf[0] = 42;
    h.current = 7;
    h.head = 50;
    h.end = true;

    subghz_rssi_history_reset(&h);

    TEST_ASSERT_EQUAL_UINT8(0, h.buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0, h.current);
    TEST_ASSERT_EQUAL_UINT8(0, h.head);
    TEST_ASSERT_FALSE(h.end);
}

/* =========================================================================
 * subghz_rssi_history_push — trace=true (cursor advance)
 * ====================================================================== */

void test_trace_true_commits_bar_and_advances_head(void)
{
    subghz_rssi_history_push(&h, -70.0f, true);

    TEST_ASSERT_EQUAL_UINT8(1, h.head);
    TEST_ASSERT_GREATER_THAN_UINT8(0, h.buf[0]);
}

void test_trace_true_multiple_advances_head(void)
{
    subghz_rssi_history_push(&h, -70.0f, true);
    subghz_rssi_history_push(&h, -70.0f, true);
    subghz_rssi_history_push(&h, -70.0f, true);

    TEST_ASSERT_EQUAL_UINT8(3, h.head);
}

void test_trace_true_bar_height_matches_conversion(void)
{
    float rssi = -75.0f;
    subghz_rssi_history_push(&h, rssi, true);

    TEST_ASSERT_EQUAL_UINT8(subghz_rssi_to_u8(rssi), h.buf[0]);
}

void test_trace_true_below_threshold_min_commits_zero_bar(void)
{
    /* RSSI below -90 dBm → u_rssi=0 → bar is zero (nothing to display).
     * The cursor still advances so recording progresses. */
    subghz_rssi_history_push(&h, -95.0f, true);

    TEST_ASSERT_EQUAL_UINT8(1, h.head);
    TEST_ASSERT_EQUAL_UINT8(0, h.buf[0]);
}

void test_trace_true_wraps_head_and_sets_end_flag(void)
{
    for (int i = 0; i < SUBGHZ_RSSI_HISTORY_SIZE; i++)
        subghz_rssi_history_push(&h, -70.0f, true);

    TEST_ASSERT_TRUE(h.end);
    TEST_ASSERT_EQUAL_UINT8(0, h.head); /* rolled back to 0 */
}

void test_trace_true_after_wrap_overwrites_oldest_entry(void)
{
    /* Fill the buffer */
    for (int i = 0; i < SUBGHZ_RSSI_HISTORY_SIZE; i++)
        subghz_rssi_history_push(&h, -70.0f, true);

    /* One more push: overwrites slot 0 with a different RSSI */
    float new_rssi = -60.0f;
    subghz_rssi_history_push(&h, new_rssi, true);

    TEST_ASSERT_EQUAL_UINT8(subghz_rssi_to_u8(new_rssi), h.buf[0]);
    TEST_ASSERT_EQUAL_UINT8(1, h.head);
}

/* =========================================================================
 * subghz_rssi_history_push — trace=false (cursor freeze)
 * ====================================================================== */

void test_trace_false_does_not_advance_head(void)
{
    /* Commit one bar, then freeze */
    subghz_rssi_history_push(&h, -70.0f, true);
    subghz_rssi_history_push(&h, -75.0f, false);

    TEST_ASSERT_EQUAL_UINT8(1, h.head); /* unchanged */
}

void test_trace_false_updates_current_rssi(void)
{
    subghz_rssi_history_push(&h, -70.0f, true);
    subghz_rssi_history_push(&h, -75.0f, false);

    TEST_ASSERT_EQUAL_UINT8(subghz_rssi_to_u8(-75.0f), h.current);
}

void test_trace_false_nonzero_rssi_updates_last_committed_bar(void)
{
    /* Above threshold_min but below capture threshold → update the bar */
    subghz_rssi_history_push(&h, -70.0f, true);   /* bar[0] = u(-70) */
    subghz_rssi_history_push(&h, -80.0f, false);  /* bar[0] ← u(-80) */

    TEST_ASSERT_EQUAL_UINT8(subghz_rssi_to_u8(-80.0f), h.buf[0]);
}

/* =========================================================================
 * BUG REGRESSION — empty waveform when signal drops below THRESHOLD_MIN
 *
 * Before fix: push(trace=false, rssi<-90) wrote 0 to the last committed
 * slot, erasing committed bars.  Alternating above-threshold / noise-floor
 * draws left the waveform completely blank.
 *
 * After fix: push(trace=false, u_rssi=0) is a no-op on the history buffer.
 * ====================================================================== */

void test_regression_noise_floor_does_not_erase_committed_bar(void)
{
    /* Commit a bar at -70 dBm (strong signal) */
    subghz_rssi_history_push(&h, -70.0f, true);
    uint8_t committed = h.buf[0];
    TEST_ASSERT_GREATER_THAN_UINT8(0, committed);

    /* Signal drops to noise floor (below -90 dBm) */
    subghz_rssi_history_push(&h, -95.0f, false);

    /* BUG: before fix, h.buf[0] would now be 0 */
    TEST_ASSERT_EQUAL_UINT8(committed, h.buf[0]);
}

void test_regression_alternating_signal_noise_preserves_all_bars(void)
{
    /* Simulate the exact scenario that caused the empty-waveform bug:
     * strong burst (trace=true) immediately followed by noise floor
     * (trace=false, rssi < -90).  Run 10 cycles. */
    for (int i = 0; i < 10; i++)
    {
        subghz_rssi_history_push(&h, -65.0f, true);   /* burst: commit bar */
        subghz_rssi_history_push(&h, -100.0f, false); /* noise: must NOT erase */
    }

    /* All 10 committed bars must be non-zero */
    for (int i = 0; i < 10; i++)
        TEST_ASSERT_GREATER_THAN_UINT8(0, h.buf[i]);
}

void test_regression_waveform_nonempty_after_100_signal_noise_pairs(void)
{
    /* Drive the buffer to full wrap with the alternating pattern */
    for (int i = 0; i < SUBGHZ_RSSI_HISTORY_SIZE; i++)
    {
        subghz_rssi_history_push(&h, -65.0f, true);
        subghz_rssi_history_push(&h, -100.0f, false);
    }

    /* History must be fully wrapped */
    TEST_ASSERT_TRUE(h.end);

    /* Every entry must be non-zero — no bars should have been erased */
    int nonzero = 0;
    for (int i = 0; i < SUBGHZ_RSSI_HISTORY_SIZE; i++)
        if (h.buf[i] > 0) nonzero++;

    TEST_ASSERT_EQUAL_INT(SUBGHZ_RSSI_HISTORY_SIZE, nonzero);
}

/* =========================================================================
 * Edge cases around the initial state (head=0, end=false)
 * ====================================================================== */

void test_trace_false_at_initial_state_no_crash(void)
{
    /* No committed bar yet — slot=0 (sentinel), update only if u>0 */
    subghz_rssi_history_push(&h, -80.0f, false);
    /* buf[0] updated to u(-80) — no crash, no out-of-bounds */
    TEST_ASSERT_EQUAL_UINT8(subghz_rssi_to_u8(-80.0f), h.buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0, h.head); /* head unchanged */
}

void test_trace_false_below_min_at_initial_state_does_not_write(void)
{
    /* buf[0] starts as 0; noise-floor push must leave it 0 */
    subghz_rssi_history_push(&h, -100.0f, false);
    TEST_ASSERT_EQUAL_UINT8(0, h.buf[0]);
}

/* =========================================================================
 * Wrapped state — slot resolution for head=0, end=true
 * ====================================================================== */

void test_trace_false_after_wrap_updates_last_slot(void)
{
    /* Fill buffer then add one more to force head back to 0 */
    for (int i = 0; i < SUBGHZ_RSSI_HISTORY_SIZE; i++)
        subghz_rssi_history_push(&h, -70.0f, true);
    /* head=0, end=true; last committed slot = buf[HISTORY_SIZE - 1] */
    TEST_ASSERT_TRUE(h.end);
    TEST_ASSERT_EQUAL_UINT8(0, h.head);

    float new_rssi = -75.0f;
    subghz_rssi_history_push(&h, new_rssi, false);

    TEST_ASSERT_EQUAL_UINT8(subghz_rssi_to_u8(new_rssi),
                            h.buf[SUBGHZ_RSSI_HISTORY_SIZE - 1]);
}

void test_trace_false_after_wrap_noise_floor_does_not_erase(void)
{
    for (int i = 0; i < SUBGHZ_RSSI_HISTORY_SIZE; i++)
        subghz_rssi_history_push(&h, -70.0f, true);

    uint8_t last = h.buf[SUBGHZ_RSSI_HISTORY_SIZE - 1];
    TEST_ASSERT_GREATER_THAN_UINT8(0, last);

    /* Noise floor push must not erase the last entry */
    subghz_rssi_history_push(&h, -100.0f, false);
    TEST_ASSERT_EQUAL_UINT8(last, h.buf[SUBGHZ_RSSI_HISTORY_SIZE - 1]);
}

/* =========================================================================
 * current field always reflects live RSSI
 * ====================================================================== */

void test_current_tracks_live_rssi_on_trace_true(void)
{
    subghz_rssi_history_push(&h, -65.0f, true);
    TEST_ASSERT_EQUAL_UINT8(subghz_rssi_to_u8(-65.0f), h.current);
}

void test_current_tracks_live_rssi_on_trace_false(void)
{
    subghz_rssi_history_push(&h, -65.0f, true);
    subghz_rssi_history_push(&h, -80.0f, false);
    TEST_ASSERT_EQUAL_UINT8(subghz_rssi_to_u8(-80.0f), h.current);
}

void test_current_zero_when_rssi_below_min(void)
{
    subghz_rssi_history_push(&h, -65.0f, true);
    subghz_rssi_history_push(&h, -95.0f, false);
    TEST_ASSERT_EQUAL_UINT8(0, h.current);
}

/* =========================================================================
 * Test runner
 * ====================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* subghz_rssi_to_u8 */
    RUN_TEST(test_rssi_to_u8_at_minimum_is_zero);
    RUN_TEST(test_rssi_to_u8_below_minimum_is_zero);
    RUN_TEST(test_rssi_to_u8_above_minimum_is_nonzero);
    RUN_TEST(test_rssi_to_u8_increases_with_signal_strength);

    /* reset */
    RUN_TEST(test_reset_zeroes_all_state);

    /* trace=true */
    RUN_TEST(test_trace_true_commits_bar_and_advances_head);
    RUN_TEST(test_trace_true_multiple_advances_head);
    RUN_TEST(test_trace_true_bar_height_matches_conversion);
    RUN_TEST(test_trace_true_below_threshold_min_commits_zero_bar);
    RUN_TEST(test_trace_true_wraps_head_and_sets_end_flag);
    RUN_TEST(test_trace_true_after_wrap_overwrites_oldest_entry);

    /* trace=false */
    RUN_TEST(test_trace_false_does_not_advance_head);
    RUN_TEST(test_trace_false_updates_current_rssi);
    RUN_TEST(test_trace_false_nonzero_rssi_updates_last_committed_bar);

    /* BUG REGRESSION */
    RUN_TEST(test_regression_noise_floor_does_not_erase_committed_bar);
    RUN_TEST(test_regression_alternating_signal_noise_preserves_all_bars);
    RUN_TEST(test_regression_waveform_nonempty_after_100_signal_noise_pairs);

    /* Edge cases */
    RUN_TEST(test_trace_false_at_initial_state_no_crash);
    RUN_TEST(test_trace_false_below_min_at_initial_state_does_not_write);

    /* Wrapped state */
    RUN_TEST(test_trace_false_after_wrap_updates_last_slot);
    RUN_TEST(test_trace_false_after_wrap_noise_floor_does_not_erase);

    /* current field */
    RUN_TEST(test_current_tracks_live_rssi_on_trace_true);
    RUN_TEST(test_current_tracks_live_rssi_on_trace_false);
    RUN_TEST(test_current_zero_when_rssi_below_min);

    return UNITY_END();
}
