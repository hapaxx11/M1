/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_transmitter_ctl.c
 * @brief  Host-side unit tests for the Transmitter scene controller.
 */

#include "unity.h"
#include "subghz_transmitter_ctl.h"

static subghz_transmitter_ctl_t c;

void setUp(void)    { /* per-test init in each test */ }
void tearDown(void) { }

/*============================================================================*/
/* Initialisation                                                             */
/*============================================================================*/

static void test_init_starts_in_ready_phase(void)
{
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 3, false, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_READY,
                       subghz_transmitter_ctl_phase(&c));
    TEST_ASSERT_FALSE(subghz_transmitter_ctl_is_tx(&c));
    TEST_ASSERT_EQUAL_UINT16(0, subghz_transmitter_ctl_bursts_emitted(&c));
}

static void test_init_propagates_mode_and_repeat(void)
{
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_ENDLESS, 5, true, 4);
    TEST_ASSERT_EQUAL(SUBGHZ_TX_MODE_ENDLESS, c.mode);
    TEST_ASSERT_EQUAL_UINT16(5, c.repeat_count);
    TEST_ASSERT_TRUE(c.allow_button_cycle);
    TEST_ASSERT_EQUAL_UINT8(4, c.button_count);
    TEST_ASSERT_EQUAL_UINT8(0, c.button_index);
}

static void test_init_is_null_safe(void)
{
    /* No crash — just verifies the NULL guard. */
    subghz_transmitter_ctl_init(NULL, SUBGHZ_TX_MODE_SINGLE, 1, false, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_READY,
                       subghz_transmitter_ctl_phase(NULL));
    TEST_ASSERT_FALSE(subghz_transmitter_ctl_is_tx(NULL));
}

static void test_event_is_null_safe(void)
{
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(NULL, SUBGHZ_TXCTL_EVT_OK_PRESS));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(NULL, SUBGHZ_TXCTL_EVT_BACK));
}

/*============================================================================*/
/* READY phase                                                                */
/*============================================================================*/

static void test_ready_back_exits_scene(void)
{
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 1, false, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_EXIT_SCENE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_BACK));
}

static void test_ready_ok_press_starts_tx(void)
{
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 3, false, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_START,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_TX,
                       subghz_transmitter_ctl_phase(&c));
    TEST_ASSERT_TRUE(subghz_transmitter_ctl_is_tx(&c));
    /* Engine should have accounted the START burst. */
    TEST_ASSERT_EQUAL_UINT16(1, subghz_transmitter_ctl_bursts_emitted(&c));
}

static void test_ready_spurious_events_are_noops(void)
{
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 1, false, 0);
    /* These events have no meaning in READY — must be silently ignored
     * without changing phase. */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_RELEASE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TEARDOWN_DONE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_READY,
                       subghz_transmitter_ctl_phase(&c));
}

/*============================================================================*/
/* SINGLE mode TX through completion                                          */
/*============================================================================*/

static void test_single_mode_completes_after_n_bursts(void)
{
    /* repeat=3 → START burst + 2 BURST_COMPLETE → TX_NEXT, then the 3rd
     * BURST_COMPLETE drives IDLE_NOW (TX_TEARDOWN). */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 3, false, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_START,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS));

    /* Burst 2 */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_NEXT_BURST,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    /* Burst 3 */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_NEXT_BURST,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    TEST_ASSERT_EQUAL_UINT16(3, subghz_transmitter_ctl_bursts_emitted(&c));

    /* Last burst complete → engine IDLE_NOW → phase EXITING */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_TEARDOWN,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_EXITING,
                       subghz_transmitter_ctl_phase(&c));
    TEST_ASSERT_FALSE(subghz_transmitter_ctl_is_tx(&c));

    /* Scene confirms teardown → EXIT_SCENE */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_EXIT_SCENE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TEARDOWN_DONE));
}

static void test_single_mode_repeat_one_completes_immediately(void)
{
    /* Static OOK: repeat=1.  One burst is sent on START, and the very
     * next BURST_COMPLETE drives IDLE_NOW. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 1, false, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_START,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_TEARDOWN,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_EXITING,
                       subghz_transmitter_ctl_phase(&c));
}

static void test_single_mode_ok_release_is_ignored(void)
{
    /* OK_RELEASE in SINGLE mode is meaningless — TX continues unchanged. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 3, false, 0);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS);

    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_RELEASE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_TX,
                       subghz_transmitter_ctl_phase(&c));

    /* TX still completes normally after the remaining bursts. */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_NEXT_BURST,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_NEXT_BURST,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_TEARDOWN,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
}

/*============================================================================*/
/* ENDLESS mode TX with hold-OK + graceful tail                               */
/*============================================================================*/

static void test_endless_mode_loops_while_held(void)
{
    /* While OK held, every BURST_COMPLETE re-arms the next burst. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_ENDLESS, 3, false, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_START,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS));

    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_NEXT_BURST,
            subghz_transmitter_ctl_event(&c,
                SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
        TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_TX,
                           subghz_transmitter_ctl_phase(&c));
    }
    /* 1 START + 10 loops = 11 bursts emitted. */
    TEST_ASSERT_EQUAL_UINT16(11, subghz_transmitter_ctl_bursts_emitted(&c));
}

static void test_endless_mode_release_does_n_minus_1_tail(void)
{
    /* repeat=3, ENDLESS: release after 5 bursts → 2 more tail bursts
     * (current finalising burst + 2 remaining = 3 total tail per engine). */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_ENDLESS, 3, false, 0);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS); /* burst 1 */
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE); /* 2 */
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE); /* 3 */
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE); /* 4 */
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE); /* 5 */
    TEST_ASSERT_EQUAL_UINT16(5, subghz_transmitter_ctl_bursts_emitted(&c));

    /* OK released — engine transitions to FINALIZING, scene gets STAY. */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_RELEASE));
    /* Tail burst 1 of 2 */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_NEXT_BURST,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    /* Tail burst 2 of 2 */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_NEXT_BURST,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    /* Tail done → teardown */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_TEARDOWN,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_EXITING,
                       subghz_transmitter_ctl_phase(&c));
}

/*============================================================================*/
/* BACK / abort paths                                                         */
/*============================================================================*/

static void test_tx_back_aborts_engine_and_tears_down(void)
{
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_ENDLESS, 3, false, 0);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE);

    /* BACK during TX → engine aborts → TX_TEARDOWN + phase EXITING */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_TEARDOWN,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_BACK));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_EXITING,
                       subghz_transmitter_ctl_phase(&c));

    /* Scene confirms teardown → EXIT_SCENE */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_EXIT_SCENE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TEARDOWN_DONE));
}

static void test_exiting_phase_ignores_back_and_other_events(void)
{
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 1, false, 0);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_EXITING,
                       subghz_transmitter_ctl_phase(&c));

    /* All non-TEARDOWN_DONE events in EXITING must be no-ops. */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_BACK));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c,
            SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_LEFT));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_EXITING,
                       subghz_transmitter_ctl_phase(&c));

    /* Only TEARDOWN_DONE escapes. */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_EXIT_SCENE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TEARDOWN_DONE));
}

/*============================================================================*/
/* Second TX run from the same loaded key                                     */
/*============================================================================*/

static void test_second_ok_press_after_completion_reinits_engine(void)
{
    /* After a SINGLE-mode run completes and the scene tears down, the
     * scene calls TEARDOWN_DONE, controller emits EXIT_SCENE.  If we
     * instead simulate "user re-presses OK from READY again" the engine
     * counters must reset to a fresh run.  In practice the scene exits
     * after TEARDOWN_DONE, but the controller must also be reusable in
     * place for callers that keep the scene alive. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 2, false, 0);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE);
    TEST_ASSERT_EQUAL_UINT16(2, subghz_transmitter_ctl_bursts_emitted(&c));

    /* Manually park back in READY (simulates external reset for
     * re-arm). */
    c.phase = SUBGHZ_TXCTL_PHASE_READY;

    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_TX_START,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS));
    /* Counter reset by re-init on OK_PRESS. */
    TEST_ASSERT_EQUAL_UINT16(1, subghz_transmitter_ctl_bursts_emitted(&c));
}

/*============================================================================*/
/* Button cycle (Phase 4 hook)                                                */
/*============================================================================*/

static void test_button_cycle_disabled_when_flag_off(void)
{
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 1,
                                 /*allow=*/false, /*count=*/4);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_LEFT));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(0, c.button_index);
}

static void test_button_cycle_disabled_when_count_le_1(void)
{
    /* Static OOK: only one "button" — LEFT/RIGHT are no-ops even with
     * the cycle flag on. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 1,
                                 /*allow=*/true, /*count=*/1);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(0, c.button_index);

    /* Edge case: button_count == 0 — also no-op. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 1, true, 0);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_LEFT));
}

static void test_button_cycle_right_advances_and_wraps(void)
{
    /* 4-button KeeLoq remote.  RIGHT: 0→1→2→3→0. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 3, true, 4);

    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_NEXT,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(1, c.button_index);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_NEXT,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(2, c.button_index);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_NEXT,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(3, c.button_index);
    /* Wrap */
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_NEXT,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(0, c.button_index);
}

static void test_button_cycle_left_retreats_and_wraps(void)
{
    /* LEFT from 0 must wrap to count-1 (modular subtraction). */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 3, true, 4);

    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_PREV,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_LEFT));
    TEST_ASSERT_EQUAL_UINT8(3, c.button_index);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_PREV,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_LEFT));
    TEST_ASSERT_EQUAL_UINT8(2, c.button_index);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_PREV,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_LEFT));
    TEST_ASSERT_EQUAL_UINT8(1, c.button_index);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_PREV,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_LEFT));
    TEST_ASSERT_EQUAL_UINT8(0, c.button_index);
}

static void test_button_cycle_ignored_during_tx(void)
{
    /* LEFT/RIGHT during TX must not interfere — they're only meaningful
     * in READY (before a key fires).  Phase 4 may revisit this but the
     * current contract is "no cycle mid-burst". */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_ENDLESS, 3, true, 4);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS);

    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_LEFT));
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_ACT_NONE,
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(0, c.button_index);
    TEST_ASSERT_EQUAL(SUBGHZ_TXCTL_PHASE_TX,
                       subghz_transmitter_ctl_phase(&c));
}

/*============================================================================*/
/* Cross-cutting invariants                                                   */
/*============================================================================*/

static void test_invariant_no_tx_start_outside_ready(void)
{
    /* TX_START must NEVER be emitted from any phase other than READY. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 3, false, 0);
    subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS);
    /* In TX phase — OK_PRESS must NOT return TX_START again. */
    subghz_transmitter_action_t a =
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_OK_PRESS);
    TEST_ASSERT_NOT_EQUAL(SUBGHZ_TXCTL_ACT_TX_START, a);
}

static void test_invariant_no_next_burst_outside_tx(void)
{
    /* TX_NEXT_BURST must only occur in TX phase. */
    subghz_transmitter_ctl_init(&c, SUBGHZ_TX_MODE_SINGLE, 1, false, 0);
    subghz_transmitter_action_t a =
        subghz_transmitter_ctl_event(&c, SUBGHZ_TXCTL_EVT_TX_BURST_COMPLETE);
    TEST_ASSERT_NOT_EQUAL(SUBGHZ_TXCTL_ACT_TX_NEXT_BURST, a);
}

/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    /* Init */
    RUN_TEST(test_init_starts_in_ready_phase);
    RUN_TEST(test_init_propagates_mode_and_repeat);
    RUN_TEST(test_init_is_null_safe);
    RUN_TEST(test_event_is_null_safe);
    /* READY */
    RUN_TEST(test_ready_back_exits_scene);
    RUN_TEST(test_ready_ok_press_starts_tx);
    RUN_TEST(test_ready_spurious_events_are_noops);
    /* SINGLE mode */
    RUN_TEST(test_single_mode_completes_after_n_bursts);
    RUN_TEST(test_single_mode_repeat_one_completes_immediately);
    RUN_TEST(test_single_mode_ok_release_is_ignored);
    /* ENDLESS mode */
    RUN_TEST(test_endless_mode_loops_while_held);
    RUN_TEST(test_endless_mode_release_does_n_minus_1_tail);
    /* Abort */
    RUN_TEST(test_tx_back_aborts_engine_and_tears_down);
    RUN_TEST(test_exiting_phase_ignores_back_and_other_events);
    /* Reuse */
    RUN_TEST(test_second_ok_press_after_completion_reinits_engine);
    /* Button cycle */
    RUN_TEST(test_button_cycle_disabled_when_flag_off);
    RUN_TEST(test_button_cycle_disabled_when_count_le_1);
    RUN_TEST(test_button_cycle_right_advances_and_wraps);
    RUN_TEST(test_button_cycle_left_retreats_and_wraps);
    RUN_TEST(test_button_cycle_ignored_during_tx);
    /* Invariants */
    RUN_TEST(test_invariant_no_tx_start_outside_ready);
    RUN_TEST(test_invariant_no_next_burst_outside_tx);
    return UNITY_END();
}
