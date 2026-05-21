/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_read_raw_state.c
 * @brief  Unit tests for the Sub-GHz Read Raw state-machine pure helpers.
 *
 *   Covers subghz_read_raw_state_is_tx() and subghz_read_raw_state_after_tx()
 *   from m1_csrc/m1_subghz_read_raw_state.h.  These helpers are used by the
 *   Read Raw scene's async TX driver to:
 *     - decide whether a Q_EVENT_SUBGHZ_TX completion or a BACK should be
 *       interpreted as "abort/continue async TX" or as "exit scene", and
 *     - decide which idle state to return to after natural completion or abort
 *       (Idle for freshly-recorded captures, Loaded for pre-existing files).
 */

#include "unity.h"
#include "m1_subghz_read_raw_state.h"

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* subghz_read_raw_state_is_tx                                                */
/*============================================================================*/

void test_is_tx_returns_true_for_all_four_tx_states(void)
{
    TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(SubGhzReadRawStateTX));
    TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(SubGhzReadRawStateTXRepeat));
    TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(SubGhzReadRawStateLoadKeyTX));
    TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(SubGhzReadRawStateLoadKeyTXRepeat));
}

void test_is_tx_returns_false_for_non_tx_states(void)
{
    TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(SubGhzReadRawStateStart));
    TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(SubGhzReadRawStateRecording));
    TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(SubGhzReadRawStateIdle));
    TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(SubGhzReadRawStateLoaded));
}

/*============================================================================*/
/* subghz_read_raw_state_after_tx                                             */
/*============================================================================*/

void test_after_tx_freshly_recorded_returns_to_idle(void)
{
    /* TX / TXRepeat originate from Idle (freshly-recorded capture). */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
                      subghz_read_raw_state_after_tx(SubGhzReadRawStateTX));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
                      subghz_read_raw_state_after_tx(SubGhzReadRawStateTXRepeat));
}

void test_after_tx_loaded_file_returns_to_loaded(void)
{
    /* LoadKeyTX / LoadKeyTXRepeat originate from Loaded (pre-existing file). */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoaded,
                      subghz_read_raw_state_after_tx(SubGhzReadRawStateLoadKeyTX));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoaded,
                      subghz_read_raw_state_after_tx(SubGhzReadRawStateLoadKeyTXRepeat));
}

void test_after_tx_non_tx_state_is_safe_idle_fallthrough(void)
{
    /* Defensive fall-through: calling with a non-TX state must not produce
     * a Loaded result (which would imply a load-origin state machine bug).
     * Idle is the safe default that won't corrupt the scene. */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
                      subghz_read_raw_state_after_tx(SubGhzReadRawStateStart));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
                      subghz_read_raw_state_after_tx(SubGhzReadRawStateRecording));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
                      subghz_read_raw_state_after_tx(SubGhzReadRawStateIdle));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
                      subghz_read_raw_state_after_tx(SubGhzReadRawStateLoaded));
}

/*============================================================================*/
/* Cross-helper consistency: every TX state has a sensible post-TX target.    */
/*============================================================================*/

void test_every_tx_state_returns_to_idle_or_loaded(void)
{
    SubGhzReadRawState tx_states[] = {
        SubGhzReadRawStateTX,
        SubGhzReadRawStateTXRepeat,
        SubGhzReadRawStateLoadKeyTX,
        SubGhzReadRawStateLoadKeyTXRepeat,
    };
    for (unsigned i = 0; i < sizeof(tx_states) / sizeof(tx_states[0]); i++)
    {
        SubGhzReadRawState after = subghz_read_raw_state_after_tx(tx_states[i]);
        TEST_ASSERT_TRUE(after == SubGhzReadRawStateIdle ||
                         after == SubGhzReadRawStateLoaded);
        /* The post-TX state must not itself be a TX state — otherwise the
         * scene would re-enter the TX branch with no DMA armed. */
        TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(after));
    }
}

/*============================================================================*/
/* subghz_read_raw_state_to_repeat                                            */
/*============================================================================*/

void test_to_repeat_promotes_tx_to_tx_repeat(void)
{
    /* Fresh-recorded TX → TXRepeat when OK is still held. */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTXRepeat,
                      subghz_read_raw_state_to_repeat(SubGhzReadRawStateTX));
}

void test_to_repeat_promotes_loadkeytx_to_loadkeytx_repeat(void)
{
    /* Pre-loaded LoadKeyTX → LoadKeyTXRepeat when OK is still held. */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoadKeyTXRepeat,
                      subghz_read_raw_state_to_repeat(SubGhzReadRawStateLoadKeyTX));
}

void test_to_repeat_is_idempotent_in_repeat_states(void)
{
    /* Already repeating — staying in repeat is the correct hold-down path. */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTXRepeat,
                      subghz_read_raw_state_to_repeat(SubGhzReadRawStateTXRepeat));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoadKeyTXRepeat,
                      subghz_read_raw_state_to_repeat(SubGhzReadRawStateLoadKeyTXRepeat));
}

void test_to_repeat_leaves_non_tx_states_unchanged(void)
{
    /* Defensive: must never promote a non-TX state into a TX-side state. */
    SubGhzReadRawState non_tx[] = {
        SubGhzReadRawStateStart,
        SubGhzReadRawStateRecording,
        SubGhzReadRawStateIdle,
        SubGhzReadRawStateLoaded,
    };
    for (unsigned i = 0; i < sizeof(non_tx) / sizeof(non_tx[0]); i++)
        TEST_ASSERT_EQUAL(non_tx[i], subghz_read_raw_state_to_repeat(non_tx[i]));
}

void test_to_repeat_result_is_always_a_tx_state_when_input_is(void)
{
    /* Cross-helper consistency: promoting any TX state must yield a TX state. */
    SubGhzReadRawState tx_states[] = {
        SubGhzReadRawStateTX,
        SubGhzReadRawStateTXRepeat,
        SubGhzReadRawStateLoadKeyTX,
        SubGhzReadRawStateLoadKeyTXRepeat,
    };
    for (unsigned i = 0; i < sizeof(tx_states) / sizeof(tx_states[0]); i++)
    {
        SubGhzReadRawState repeated = subghz_read_raw_state_to_repeat(tx_states[i]);
        TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(repeated));
        /* And the post-TX target is preserved across the repeat promotion —
         * TX/TXRepeat both return to Idle; LoadKeyTX/LoadKeyTXRepeat both
         * return to Loaded. */
        TEST_ASSERT_EQUAL(subghz_read_raw_state_after_tx(tx_states[i]),
                          subghz_read_raw_state_after_tx(repeated));
    }
}

/*============================================================================*/
/* subghz_read_raw_display_count_update                                       */
/*                                                                            */
/* Regression coverage for the issue "Read Raw is counting SPL when none are  */
/* being collected".  The displayed counter must follow the same gating rule  */
/* as the waveform cursor: only advance while signal_present is true.         */
/*============================================================================*/

void test_display_count_advances_when_signal_seen(void)
{
    /* Recording with signal present in the last draw tick — the displayed
     * counter must catch up to the file's total. */
    TEST_ASSERT_EQUAL_UINT32(
        512u,
        subghz_read_raw_display_count_update(0u, 512u, true));
    TEST_ASSERT_EQUAL_UINT32(
        2048u,
        subghz_read_raw_display_count_update(1536u, 2048u, true));
}

void test_display_count_freezes_when_no_signal_seen(void)
{
    /* Recording without any signal_present sample since the last flush — the
     * displayed counter must stay where it was, even though the file on SD
     * has grown by another 512-sample chunk from ambient noise that passed
     * the 80 µs ISR filter.  This is the regression guard for the issue. */
    TEST_ASSERT_EQUAL_UINT32(
        0u,
        subghz_read_raw_display_count_update(0u, 512u, false));
    TEST_ASSERT_EQUAL_UINT32(
        1536u,
        subghz_read_raw_display_count_update(1536u, 2048u, false));
}

void test_display_count_freezes_across_many_noise_chunks(void)
{
    /* Simulate a long silent recording: total_samples climbs in 512-sample
     * steps from noise, but signal_seen_in_chunk stays false the entire
     * time.  The displayed counter must remain at zero throughout. */
    uint32_t displayed = 0u;
    for (uint32_t total = 512u; total <= 10u * 512u; total += 512u)
        displayed = subghz_read_raw_display_count_update(displayed, total, false);
    TEST_ASSERT_EQUAL_UINT32(0u, displayed);
}

void test_display_count_catches_up_when_signal_returns(void)
{
    /* Realistic mix: silent → noisy chunks (frozen) → signal arrives in a
     * later chunk (counter jumps to the current file total, not a stale
     * subtotal).  This is the behaviour the user expects after pressing OK
     * to start recording and then a real burst arriving a few seconds in. */
    uint32_t displayed = 0u;
    displayed = subghz_read_raw_display_count_update(displayed, 512u, false);
    displayed = subghz_read_raw_display_count_update(displayed, 1024u, false);
    displayed = subghz_read_raw_display_count_update(displayed, 1536u, false);
    TEST_ASSERT_EQUAL_UINT32(0u, displayed);
    /* Signal finally seen in this chunk — counter catches up to the full
     * current total, not just 512. */
    displayed = subghz_read_raw_display_count_update(displayed, 2048u, true);
    TEST_ASSERT_EQUAL_UINT32(2048u, displayed);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_is_tx_returns_true_for_all_four_tx_states);
    RUN_TEST(test_is_tx_returns_false_for_non_tx_states);
    RUN_TEST(test_after_tx_freshly_recorded_returns_to_idle);
    RUN_TEST(test_after_tx_loaded_file_returns_to_loaded);
    RUN_TEST(test_after_tx_non_tx_state_is_safe_idle_fallthrough);
    RUN_TEST(test_every_tx_state_returns_to_idle_or_loaded);
    RUN_TEST(test_to_repeat_promotes_tx_to_tx_repeat);
    RUN_TEST(test_to_repeat_promotes_loadkeytx_to_loadkeytx_repeat);
    RUN_TEST(test_to_repeat_is_idempotent_in_repeat_states);
    RUN_TEST(test_to_repeat_leaves_non_tx_states_unchanged);
    RUN_TEST(test_to_repeat_result_is_always_a_tx_state_when_input_is);
    RUN_TEST(test_display_count_advances_when_signal_seen);
    RUN_TEST(test_display_count_freezes_when_no_signal_seen);
    RUN_TEST(test_display_count_freezes_across_many_noise_chunks);
    RUN_TEST(test_display_count_catches_up_when_signal_returns);
    return UNITY_END();
}
