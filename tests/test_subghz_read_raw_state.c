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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_is_tx_returns_true_for_all_four_tx_states);
    RUN_TEST(test_is_tx_returns_false_for_non_tx_states);
    RUN_TEST(test_after_tx_freshly_recorded_returns_to_idle);
    RUN_TEST(test_after_tx_loaded_file_returns_to_loaded);
    RUN_TEST(test_after_tx_non_tx_state_is_safe_idle_fallthrough);
    RUN_TEST(test_every_tx_state_returns_to_idle_or_loaded);
    return UNITY_END();
}
