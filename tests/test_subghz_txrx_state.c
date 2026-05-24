/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_txrx_state.c
 * @brief  Host-side unit tests for the Sub-GHz TX/RX lifecycle state machine.
 */

#include "unity.h"
#include "subghz_txrx_state.h"

void setUp(void) { }
void tearDown(void) { }

/*----------------------------------------------------------------------------*/
/* Pure helpers                                                               */
/*----------------------------------------------------------------------------*/

static void test_name_for_every_state_is_non_null_and_nonempty(void)
{
    for (int s = 0; s < SUBGHZ_TXRX_STATE_COUNT; ++s) {
        const char *n = subghz_txrx_state_name((subghz_txrx_state_t)s);
        TEST_ASSERT_NOT_NULL(n);
        TEST_ASSERT_TRUE(n[0] != '\0');
    }
    TEST_ASSERT_EQUAL_STRING("INVALID", subghz_txrx_state_name((subghz_txrx_state_t)SUBGHZ_TXRX_STATE_COUNT));
    TEST_ASSERT_EQUAL_STRING("INVALID", subghz_txrx_state_name((subghz_txrx_state_t)99));
}

static void test_is_rx_classifies_correctly(void)
{
    TEST_ASSERT_FALSE(subghz_txrx_state_is_rx(SUBGHZ_TXRX_STATE_OFF));
    TEST_ASSERT_FALSE(subghz_txrx_state_is_rx(SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_rx(SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_rx(SUBGHZ_TXRX_STATE_RX_ACTIVE));
    TEST_ASSERT_FALSE(subghz_txrx_state_is_rx(SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_FALSE(subghz_txrx_state_is_rx(SUBGHZ_TXRX_STATE_TX_ASYNC));
}

static void test_is_tx_classifies_correctly(void)
{
    TEST_ASSERT_FALSE(subghz_txrx_state_is_tx(SUBGHZ_TXRX_STATE_OFF));
    TEST_ASSERT_FALSE(subghz_txrx_state_is_tx(SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_FALSE(subghz_txrx_state_is_tx(SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_FALSE(subghz_txrx_state_is_tx(SUBGHZ_TXRX_STATE_RX_ACTIVE));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_tx(SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_tx(SUBGHZ_TXRX_STATE_TX_ASYNC));
}

static void test_is_powered_classifies_correctly(void)
{
    TEST_ASSERT_FALSE(subghz_txrx_state_is_powered(SUBGHZ_TXRX_STATE_OFF));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_powered(SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_powered(SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_powered(SUBGHZ_TXRX_STATE_RX_ACTIVE));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_powered(SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_TRUE (subghz_txrx_state_is_powered(SUBGHZ_TXRX_STATE_TX_ASYNC));
    TEST_ASSERT_FALSE(subghz_txrx_state_is_powered((subghz_txrx_state_t)99));
}

/*----------------------------------------------------------------------------*/
/* Transition legality table                                                  */
/*----------------------------------------------------------------------------*/

static void test_off_transitions(void)
{
    /* OFF can only go to IDLE. */
    for (int t = 0; t < SUBGHZ_TXRX_STATE_COUNT; ++t) {
        bool expected = (t == SUBGHZ_TXRX_STATE_IDLE);
        TEST_ASSERT_EQUAL(expected,
            subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_OFF,
                                                   (subghz_txrx_state_t)t));
    }
}

static void test_idle_transitions(void)
{
    /* IDLE → {OFF, RX_PASSIVE, RX_ACTIVE, TX_BLOCK, TX_ASYNC} */
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_IDLE, SUBGHZ_TXRX_STATE_OFF));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_IDLE, SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_IDLE, SUBGHZ_TXRX_STATE_RX_ACTIVE));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_IDLE, SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_IDLE, SUBGHZ_TXRX_STATE_TX_ASYNC));
}

static void test_rx_passive_transitions(void)
{
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_PASSIVE, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_PASSIVE, SUBGHZ_TXRX_STATE_RX_ACTIVE));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_PASSIVE, SUBGHZ_TXRX_STATE_OFF));
    /* Cannot jump directly from passive RX to TX. */
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_PASSIVE, SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_PASSIVE, SUBGHZ_TXRX_STATE_TX_ASYNC));
}

static void test_rx_active_transitions(void)
{
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_ACTIVE, SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_ACTIVE, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_ACTIVE, SUBGHZ_TXRX_STATE_OFF));
    /* Cannot jump directly to TX from active RX (must go via IDLE). */
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_ACTIVE, SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_RX_ACTIVE, SUBGHZ_TXRX_STATE_TX_ASYNC));
}

static void test_tx_block_transitions(void)
{
    /* TX_BLOCK is a legacy wrapper that always exits to OFF (the wrappers
     * call menu_sub_ghz_exit() at the end) or to IDLE (after caller
     * restores via menu_sub_ghz_init()). */
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_BLOCK, SUBGHZ_TXRX_STATE_OFF));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_BLOCK, SUBGHZ_TXRX_STATE_IDLE));
    /* TX_BLOCK cannot pivot directly into RX. */
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_BLOCK, SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_BLOCK, SUBGHZ_TXRX_STATE_RX_ACTIVE));
}

static void test_tx_async_transitions(void)
{
    /* TX_ASYNC completes by returning to IDLE (general case) or
     * RX_PASSIVE (scene-resumes-listen pattern in Read Raw). */
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_ASYNC, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE (subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_ASYNC, SUBGHZ_TXRX_STATE_RX_PASSIVE));
    /* Async TX cannot pivot to active RX or to another TX without going via IDLE. */
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_ASYNC, SUBGHZ_TXRX_STATE_RX_ACTIVE));
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_ASYNC, SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_TX_ASYNC, SUBGHZ_TXRX_STATE_OFF));
}

static void test_same_state_query_is_false(void)
{
    /* The pure query intentionally returns false for X→X so that callers
     * have to think explicitly about idempotency.  try_transition()
     * treats X→X as a no-op (tested separately). */
    for (int s = 0; s < SUBGHZ_TXRX_STATE_COUNT; ++s) {
        TEST_ASSERT_FALSE(
            subghz_txrx_state_transition_is_legal((subghz_txrx_state_t)s,
                                                   (subghz_txrx_state_t)s));
    }
}

static void test_out_of_range_is_rejected(void)
{
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal((subghz_txrx_state_t)99, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_FALSE(subghz_txrx_state_transition_is_legal(SUBGHZ_TXRX_STATE_IDLE, (subghz_txrx_state_t)99));
}

/*----------------------------------------------------------------------------*/
/* Stateful façade                                                            */
/*----------------------------------------------------------------------------*/

static void test_init_yields_off_and_zero_counter(void)
{
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);
    TEST_ASSERT_EQUAL(SUBGHZ_TXRX_STATE_OFF, ctx.state);
    TEST_ASSERT_EQUAL(0u, ctx.illegal_transition_count);
}

static void test_init_handles_null_safely(void)
{
    /* Should not crash. */
    subghz_txrx_state_init(NULL);
}

static void test_happy_path_powerup_listen_record_stop(void)
{
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);

    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_RX_ACTIVE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_OFF));
    TEST_ASSERT_EQUAL(SUBGHZ_TXRX_STATE_OFF, ctx.state);
    TEST_ASSERT_EQUAL(0u, ctx.illegal_transition_count);
}

static void test_happy_path_async_tx_then_resume_listen(void)
{
    /* Read Raw async TX → restore passive RX */
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);

    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_RX_PASSIVE));
    /* Read Raw deinits RX before starting async TX (RX_PASSIVE→IDLE→TX_ASYNC) */
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_TX_ASYNC));
    /* After TX completion the scene restores listen mode */
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_RX_PASSIVE));
    TEST_ASSERT_EQUAL(0u, ctx.illegal_transition_count);
}

static void test_happy_path_blocking_replay_full_cycle(void)
{
    /* Saved scene PACKET/key emulate path:
     *   OFF → IDLE → TX_BLOCK → OFF (wrapper exits via menu_sub_ghz_exit)
     *       → IDLE (caller restores via menu_sub_ghz_init)
     */
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);

    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_OFF));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_EQUAL(0u, ctx.illegal_transition_count);
}

static void test_no_op_transition_is_accepted_and_not_counted(void)
{
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);
    /* OFF → OFF as a no-op should be accepted without counter bump. */
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_OFF));
    TEST_ASSERT_EQUAL(0u, ctx.illegal_transition_count);

    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_EQUAL(0u, ctx.illegal_transition_count);
}

static void test_illegal_transition_increments_counter_and_preserves_state(void)
{
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);
    subghz_txrx_state_force(&ctx, SUBGHZ_TXRX_STATE_RX_ACTIVE);

    /* RX_ACTIVE → TX_ASYNC is illegal (must go via IDLE). */
    TEST_ASSERT_FALSE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_TX_ASYNC));
    TEST_ASSERT_EQUAL(SUBGHZ_TXRX_STATE_RX_ACTIVE, ctx.state);
    TEST_ASSERT_EQUAL(1u, ctx.illegal_transition_count);

    /* Second illegal attempt also counts. */
    TEST_ASSERT_FALSE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_TX_BLOCK));
    TEST_ASSERT_EQUAL(2u, ctx.illegal_transition_count);

    /* Legal transition resets nothing but proceeds. */
    TEST_ASSERT_TRUE(subghz_txrx_state_try_transition(&ctx, SUBGHZ_TXRX_STATE_IDLE));
    TEST_ASSERT_EQUAL(SUBGHZ_TXRX_STATE_IDLE, ctx.state);
    TEST_ASSERT_EQUAL(2u, ctx.illegal_transition_count);
}

static void test_out_of_range_to_state_is_rejected_and_counted(void)
{
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);
    TEST_ASSERT_FALSE(subghz_txrx_state_try_transition(&ctx, (subghz_txrx_state_t)99));
    TEST_ASSERT_EQUAL(SUBGHZ_TXRX_STATE_OFF, ctx.state);
    TEST_ASSERT_EQUAL(1u, ctx.illegal_transition_count);
}

static void test_force_bypasses_legality(void)
{
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);
    /* OFF → TX_ASYNC is not a legal transition, but force() must accept
     * it for error-recovery paths. */
    subghz_txrx_state_force(&ctx, SUBGHZ_TXRX_STATE_TX_ASYNC);
    TEST_ASSERT_EQUAL(SUBGHZ_TXRX_STATE_TX_ASYNC, ctx.state);
    TEST_ASSERT_EQUAL(0u, ctx.illegal_transition_count);
}

static void test_force_rejects_out_of_range(void)
{
    subghz_txrx_state_ctx_t ctx;
    subghz_txrx_state_init(&ctx);
    subghz_txrx_state_force(&ctx, SUBGHZ_TXRX_STATE_RX_PASSIVE);
    subghz_txrx_state_force(&ctx, (subghz_txrx_state_t)99); /* ignored */
    TEST_ASSERT_EQUAL(SUBGHZ_TXRX_STATE_RX_PASSIVE, ctx.state);
}

static void test_try_transition_with_null_is_safe(void)
{
    TEST_ASSERT_FALSE(subghz_txrx_state_try_transition(NULL, SUBGHZ_TXRX_STATE_IDLE));
}

static void test_force_with_null_is_safe(void)
{
    subghz_txrx_state_force(NULL, SUBGHZ_TXRX_STATE_IDLE);
}

/*----------------------------------------------------------------------------*/
/* Runner                                                                     */
/*----------------------------------------------------------------------------*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_name_for_every_state_is_non_null_and_nonempty);
    RUN_TEST(test_is_rx_classifies_correctly);
    RUN_TEST(test_is_tx_classifies_correctly);
    RUN_TEST(test_is_powered_classifies_correctly);
    RUN_TEST(test_off_transitions);
    RUN_TEST(test_idle_transitions);
    RUN_TEST(test_rx_passive_transitions);
    RUN_TEST(test_rx_active_transitions);
    RUN_TEST(test_tx_block_transitions);
    RUN_TEST(test_tx_async_transitions);
    RUN_TEST(test_same_state_query_is_false);
    RUN_TEST(test_out_of_range_is_rejected);
    RUN_TEST(test_init_yields_off_and_zero_counter);
    RUN_TEST(test_init_handles_null_safely);
    RUN_TEST(test_happy_path_powerup_listen_record_stop);
    RUN_TEST(test_happy_path_async_tx_then_resume_listen);
    RUN_TEST(test_happy_path_blocking_replay_full_cycle);
    RUN_TEST(test_no_op_transition_is_accepted_and_not_counted);
    RUN_TEST(test_illegal_transition_increments_counter_and_preserves_state);
    RUN_TEST(test_out_of_range_to_state_is_rejected_and_counted);
    RUN_TEST(test_force_bypasses_legality);
    RUN_TEST(test_force_rejects_out_of_range);
    RUN_TEST(test_try_transition_with_null_is_safe);
    RUN_TEST(test_force_with_null_is_safe);
    return UNITY_END();
}
