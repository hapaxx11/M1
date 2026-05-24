/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_endless_tx.c
 * @brief  Host-side unit tests for the TX repeat / endless-hold policy.
 */

#include "unity.h"
#include "subghz_endless_tx.h"

void setUp(void) { }
void tearDown(void) { }

/* Shorthand */
static subghz_endless_tx_action_t evt(subghz_endless_tx_t *e,
                                       subghz_endless_tx_event_t ev)
{
    return subghz_endless_tx_event(e, ev);
}

/*============================================================================*/
/* Init                                                                       */
/*============================================================================*/

static void test_init_sets_idle_state(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 3);
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_IDLE, subghz_endless_tx_state(&e));
    TEST_ASSERT_EQUAL_UINT16(3, e.repeat_count);
    TEST_ASSERT_EQUAL_UINT16(0, e.bursts_emitted);
    TEST_ASSERT_EQUAL_UINT16(0, e.bursts_remaining);
    TEST_ASSERT_FALSE(subghz_endless_tx_is_finished(&e));
}

static void test_init_handles_null_safely(void)
{
    /* Must not crash. */
    subghz_endless_tx_init(NULL, SUBGHZ_TX_MODE_SINGLE, 3);
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_IDLE, subghz_endless_tx_state(NULL));
    TEST_ASSERT_TRUE(subghz_endless_tx_is_finished(NULL));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY,
                      subghz_endless_tx_event(NULL, SUBGHZ_ETX_EVT_START));
}

static void test_init_clamps_zero_repeat_to_one(void)
{
    /* repeat_count = 0 must not underflow — clamp to 1. */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 0);
    TEST_ASSERT_EQUAL_UINT16(1, e.repeat_count);

    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 0);
    TEST_ASSERT_EQUAL_UINT16(1, e.repeat_count);
}

static void test_events_before_start_are_ignored(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 3);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_IDLE, e.state);
    TEST_ASSERT_EQUAL_UINT16(0, e.bursts_emitted);
}

/*============================================================================*/
/* SINGLE mode                                                                */
/*============================================================================*/

static void test_single_mode_n3_completes_in_three_bursts(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 3);

    /* START arms burst 1.  After this, 2 more remain. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_START));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_RUNNING, e.state);
    TEST_ASSERT_EQUAL_UINT16(1, e.bursts_emitted);
    TEST_ASSERT_EQUAL_UINT16(2, e.bursts_remaining);

    /* Burst 1 completes → arm burst 2. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL_UINT16(2, e.bursts_emitted);
    TEST_ASSERT_EQUAL_UINT16(1, e.bursts_remaining);

    /* Burst 2 completes → arm burst 3. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL_UINT16(3, e.bursts_emitted);
    TEST_ASSERT_EQUAL_UINT16(0, e.bursts_remaining);

    /* Burst 3 completes → DONE, idle. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);
    TEST_ASSERT_TRUE(subghz_endless_tx_is_finished(&e));
}

static void test_single_mode_n1_completes_in_one_burst(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 1);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_START));
    TEST_ASSERT_EQUAL_UINT16(0, e.bursts_remaining);

    /* The one burst completes → DONE. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);
    TEST_ASSERT_EQUAL_UINT16(1, e.bursts_emitted);
}

static void test_single_mode_ignores_ok_release(void)
{
    /* Release events in SINGLE mode are no-ops — TX completes regardless. */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 2);
    evt(&e, SUBGHZ_ETX_EVT_START);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_RUNNING, e.state);

    /* Still completes normally. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);
}

/*============================================================================*/
/* ENDLESS mode                                                               */
/*============================================================================*/

static void test_endless_loops_while_held(void)
{
    /* OK held — every BURST_COMPLETE should return TX_NEXT, indefinitely. */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 3);
    evt(&e, SUBGHZ_ETX_EVT_START);

    for (int i = 0; i < 50; ++i) {
        TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT,
                          evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
        TEST_ASSERT_EQUAL(SUBGHZ_ETX_RUNNING, e.state);
    }
    TEST_ASSERT_EQUAL_UINT16(51, e.bursts_emitted);  /* 1 (START) + 50 */
}

static void test_endless_release_triggers_finalising_then_done(void)
{
    /* ENDLESS, repeat_count=3.  Pattern:
     *   START -> burst1 running
     *   BC    -> burst2 running  (TX_NEXT)
     *   RELEASE -> FINALIZING, remaining=2
     *   BC    -> burst3 (TX_NEXT, remaining=1)
     *   BC    -> burst4 (TX_NEXT, remaining=0)
     *   BC    -> DONE   (IDLE_NOW)
     */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 3);
    evt(&e, SUBGHZ_ETX_EVT_START);
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_RUNNING, e.state);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_FINALIZING, e.state);
    TEST_ASSERT_EQUAL_UINT16(2, e.bursts_remaining);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL_UINT16(1, e.bursts_remaining);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL_UINT16(0, e.bursts_remaining);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);
}

static void test_endless_release_with_n1_finishes_on_next_burst(void)
{
    /* repeat_count=1 means no tail; the next BC after release goes to DONE. */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 1);
    evt(&e, SUBGHZ_ETX_EVT_START);
    evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE);   /* another burst while held */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_FINALIZING, e.state);
    TEST_ASSERT_EQUAL_UINT16(0, e.bursts_remaining);

    /* Empty tail — next BC goes straight to DONE. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW,
                      evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);
}

static void test_endless_release_immediately_after_start(void)
{
    /* Tap-and-release: START, RELEASE, then bursts complete the tail. */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 3);
    evt(&e, SUBGHZ_ETX_EVT_START);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_FINALIZING, e.state);

    /* Tail of 2 more bursts. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);
    TEST_ASSERT_EQUAL_UINT16(3, e.bursts_emitted);
}

static void test_endless_duplicate_release_is_noop(void)
{
    /* Debounce: a second OK_RELEASED while FINALIZING must not extend
     * or shorten the tail. */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 3);
    evt(&e, SUBGHZ_ETX_EVT_START);
    evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED);
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_FINALIZING, e.state);
    TEST_ASSERT_EQUAL_UINT16(2, e.bursts_remaining);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_FINALIZING, e.state);
    TEST_ASSERT_EQUAL_UINT16(2, e.bursts_remaining);
}

/*============================================================================*/
/* ABORT                                                                      */
/*============================================================================*/

static void test_abort_from_running_transitions_to_aborted(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 3);
    evt(&e, SUBGHZ_ETX_EVT_START);
    evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE);

    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_ABORT));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ABORTED, e.state);
    TEST_ASSERT_TRUE(subghz_endless_tx_is_finished(&e));
}

static void test_abort_from_finalising_transitions_to_aborted(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 5);
    evt(&e, SUBGHZ_ETX_EVT_START);
    evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED);
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_FINALIZING, e.state);

    /* BACK during finalisation must cut the tail. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_ABORT));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ABORTED, e.state);
}

static void test_abort_from_idle_is_safe(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 3);
    /* ABORT before START — should still mark aborted (terminal) and ask
     * the caller to idle (defensive: caller may have set up TX state). */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_ABORT));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ABORTED, e.state);
}

static void test_abort_from_terminal_state_is_noop(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 1);
    evt(&e, SUBGHZ_ETX_EVT_START);
    evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE);
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);

    /* Repeated ABORT after natural completion must not regress state
     * (DONE is meaningfully different from ABORTED for diagnostics). */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_ABORT));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);
}

static void test_events_after_done_are_ignored(void)
{
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 1);
    evt(&e, SUBGHZ_ETX_EVT_START);
    evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE);
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);

    /* Late ISR delivering a stale TC must not re-arm TX. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_STAY, evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED));
}

/*============================================================================*/
/* Realistic sequences                                                        */
/*============================================================================*/

static void test_realistic_keeloq_single_burst_3x(void)
{
    /* Saved scene PACKET emulate of a KeeLoq key would use SINGLE mode
     * with N=3 (rolling-code reliability).  Three TX_NEXTs total emit,
     * including START. */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_SINGLE, 3);
    evt(&e, SUBGHZ_ETX_EVT_START);

    int next_count = 0, idle_count = 0;
    for (int i = 0; i < 10 && !subghz_endless_tx_is_finished(&e); ++i) {
        subghz_endless_tx_action_t a = evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE);
        if (a == SUBGHZ_ETX_ACT_TX_NEXT) next_count++;
        else if (a == SUBGHZ_ETX_ACT_IDLE_NOW) idle_count++;
    }
    TEST_ASSERT_EQUAL_INT(2, next_count);   /* Two TX_NEXTs after START burst. */
    TEST_ASSERT_EQUAL_INT(1, idle_count);
    TEST_ASSERT_EQUAL_UINT16(3, e.bursts_emitted);
}

static void test_realistic_remote_hold_5_bursts_then_release(void)
{
    /* Remote scene with hold-OK: user holds for 5 bursts, releases,
     * tail of 3 (repeat_count=3) finishes the press. */
    subghz_endless_tx_t e;
    subghz_endless_tx_init(&e, SUBGHZ_TX_MODE_ENDLESS, 3);
    evt(&e, SUBGHZ_ETX_EVT_START);

    /* Held bursts. */
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT,
                          evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    }
    TEST_ASSERT_EQUAL_UINT16(5, e.bursts_emitted);  /* START + 4 BC */

    /* User releases mid-burst-5. */
    evt(&e, SUBGHZ_ETX_EVT_OK_RELEASED);
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_FINALIZING, e.state);

    /* Tail of 2 more bursts + the closing one. */
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_TX_NEXT, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_ACT_IDLE_NOW, evt(&e, SUBGHZ_ETX_EVT_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_ETX_DONE, e.state);
    TEST_ASSERT_EQUAL_UINT16(7, e.bursts_emitted);  /* 5 held + 2 tail TX_NEXT */
}

/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    /* Init */
    RUN_TEST(test_init_sets_idle_state);
    RUN_TEST(test_init_handles_null_safely);
    RUN_TEST(test_init_clamps_zero_repeat_to_one);
    RUN_TEST(test_events_before_start_are_ignored);
    /* SINGLE mode */
    RUN_TEST(test_single_mode_n3_completes_in_three_bursts);
    RUN_TEST(test_single_mode_n1_completes_in_one_burst);
    RUN_TEST(test_single_mode_ignores_ok_release);
    /* ENDLESS mode */
    RUN_TEST(test_endless_loops_while_held);
    RUN_TEST(test_endless_release_triggers_finalising_then_done);
    RUN_TEST(test_endless_release_with_n1_finishes_on_next_burst);
    RUN_TEST(test_endless_release_immediately_after_start);
    RUN_TEST(test_endless_duplicate_release_is_noop);
    /* ABORT */
    RUN_TEST(test_abort_from_running_transitions_to_aborted);
    RUN_TEST(test_abort_from_finalising_transitions_to_aborted);
    RUN_TEST(test_abort_from_idle_is_safe);
    RUN_TEST(test_abort_from_terminal_state_is_noop);
    RUN_TEST(test_events_after_done_are_ignored);
    /* Realistic */
    RUN_TEST(test_realistic_keeloq_single_burst_3x);
    RUN_TEST(test_realistic_remote_hold_5_bursts_then_release);
    return UNITY_END();
}
