/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_readraw_states.c
 * @brief  Pin the Read Raw scene TX state machine transitions (issue #469).
 *
 * The four TX states (TX / TXRepeat / LoadKeyTX / LoadKeyTXRepeat) and the
 * helpers that map between them are the canonical encoding of the async TX
 * dispatch decision.  Per CLAUDE.md, dispatch logic must be extracted into a
 * named pure helper and tested in isolation — these tests do that.
 *
 * The helpers under test are `static inline` in `m1_subghz_scene.h`, but
 * that header pulls in HAL/FreeRTOS-dependent files.  To keep this test
 * pure-logic and hardware-independent, we mirror the enum + helper
 * definitions verbatim.  CI must keep the two in sync; the canonical
 * source is `m1_csrc/m1_subghz_scene.h`.
 */

#include "unity.h"
#include <stdbool.h>

/* ── Mirror of m1_subghz_scene.h enum + inlines (issue #469) ────────────── */
typedef enum {
    SubGhzReadRawStateStart = 0,
    SubGhzReadRawStateRecording,
    SubGhzReadRawStateIdle,
    SubGhzReadRawStateLoaded,
    SubGhzReadRawStateTX,
    SubGhzReadRawStateTXRepeat,
    SubGhzReadRawStateLoadKeyTX,
    SubGhzReadRawStateLoadKeyTXRepeat,
    SubGhzReadRawStateSending = SubGhzReadRawStateTX,
} SubGhzReadRawState;

static inline SubGhzReadRawState
subghz_readraw_tx_state_for_prior(SubGhzReadRawState prior)
{
    return (prior == SubGhzReadRawStateLoaded)
           ? SubGhzReadRawStateLoadKeyTX
           : SubGhzReadRawStateTX;
}

static inline SubGhzReadRawState
subghz_readraw_repeat_state_for_tx(SubGhzReadRawState tx_state)
{
    if (tx_state == SubGhzReadRawStateTX)        return SubGhzReadRawStateTXRepeat;
    if (tx_state == SubGhzReadRawStateLoadKeyTX) return SubGhzReadRawStateLoadKeyTXRepeat;
    return tx_state;
}

static inline SubGhzReadRawState
subghz_readraw_prior_state_for_tx(SubGhzReadRawState tx_state)
{
    switch (tx_state) {
    case SubGhzReadRawStateTX:
    case SubGhzReadRawStateTXRepeat:
        return SubGhzReadRawStateIdle;
    case SubGhzReadRawStateLoadKeyTX:
    case SubGhzReadRawStateLoadKeyTXRepeat:
        return SubGhzReadRawStateLoaded;
    default:
        return tx_state;
    }
}

static inline bool
subghz_readraw_state_is_tx(SubGhzReadRawState s)
{
    return (s == SubGhzReadRawStateTX) ||
           (s == SubGhzReadRawStateTXRepeat) ||
           (s == SubGhzReadRawStateLoadKeyTX) ||
           (s == SubGhzReadRawStateLoadKeyTXRepeat);
}

void setUp(void)    { }
void tearDown(void) { }

/* ── tx_state_for_prior ─────────────────────────────────────────────────── */

void test_tx_state_for_prior_idle_to_tx(void)
{
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTX,
        subghz_readraw_tx_state_for_prior(SubGhzReadRawStateIdle));
}

void test_tx_state_for_prior_loaded_to_loadkey(void)
{
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoadKeyTX,
        subghz_readraw_tx_state_for_prior(SubGhzReadRawStateLoaded));
}

void test_tx_state_for_prior_unexpected_defaults_to_tx(void)
{
    /* Defensive: callers MUST only pass Idle or Loaded.  Any other value is
     * treated as Idle so a bad caller cannot route into LoadKeyTX. */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTX,
        subghz_readraw_tx_state_for_prior(SubGhzReadRawStateStart));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTX,
        subghz_readraw_tx_state_for_prior(SubGhzReadRawStateRecording));
}

/* ── repeat_state_for_tx ────────────────────────────────────────────────── */

void test_repeat_state_for_tx_pairs(void)
{
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTXRepeat,
        subghz_readraw_repeat_state_for_tx(SubGhzReadRawStateTX));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoadKeyTXRepeat,
        subghz_readraw_repeat_state_for_tx(SubGhzReadRawStateLoadKeyTX));
}

void test_repeat_state_for_tx_idempotent_on_repeat(void)
{
    /* Calling repeat() on an already-repeating state should be a no-op:
     * the scene stays in the same repeat state across consecutive cycles. */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTXRepeat,
        subghz_readraw_repeat_state_for_tx(SubGhzReadRawStateTXRepeat));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoadKeyTXRepeat,
        subghz_readraw_repeat_state_for_tx(SubGhzReadRawStateLoadKeyTXRepeat));
}

void test_repeat_state_for_tx_non_tx_passthrough(void)
{
    /* Bad caller: non-TX states pass through unchanged so a stray call
     * cannot accidentally enter a TX state. */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
        subghz_readraw_repeat_state_for_tx(SubGhzReadRawStateIdle));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoaded,
        subghz_readraw_repeat_state_for_tx(SubGhzReadRawStateLoaded));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateStart,
        subghz_readraw_repeat_state_for_tx(SubGhzReadRawStateStart));
}

/* ── prior_state_for_tx ─────────────────────────────────────────────────── */

void test_prior_state_for_tx_round_trip(void)
{
    /* The complete round-trip: tx -> repeat -> prior all collapse to the
     * originating state (Idle for TX-family, Loaded for LoadKey-family). */
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
        subghz_readraw_prior_state_for_tx(SubGhzReadRawStateTX));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
        subghz_readraw_prior_state_for_tx(SubGhzReadRawStateTXRepeat));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoaded,
        subghz_readraw_prior_state_for_tx(SubGhzReadRawStateLoadKeyTX));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoaded,
        subghz_readraw_prior_state_for_tx(SubGhzReadRawStateLoadKeyTXRepeat));
}

void test_prior_state_for_tx_non_tx_passthrough(void)
{
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle,
        subghz_readraw_prior_state_for_tx(SubGhzReadRawStateIdle));
    TEST_ASSERT_EQUAL(SubGhzReadRawStateStart,
        subghz_readraw_prior_state_for_tx(SubGhzReadRawStateStart));
}

/* ── state_is_tx ────────────────────────────────────────────────────────── */

void test_state_is_tx_positives(void)
{
    TEST_ASSERT_TRUE(subghz_readraw_state_is_tx(SubGhzReadRawStateTX));
    TEST_ASSERT_TRUE(subghz_readraw_state_is_tx(SubGhzReadRawStateTXRepeat));
    TEST_ASSERT_TRUE(subghz_readraw_state_is_tx(SubGhzReadRawStateLoadKeyTX));
    TEST_ASSERT_TRUE(subghz_readraw_state_is_tx(SubGhzReadRawStateLoadKeyTXRepeat));
}

void test_state_is_tx_negatives(void)
{
    TEST_ASSERT_FALSE(subghz_readraw_state_is_tx(SubGhzReadRawStateStart));
    TEST_ASSERT_FALSE(subghz_readraw_state_is_tx(SubGhzReadRawStateRecording));
    TEST_ASSERT_FALSE(subghz_readraw_state_is_tx(SubGhzReadRawStateIdle));
    TEST_ASSERT_FALSE(subghz_readraw_state_is_tx(SubGhzReadRawStateLoaded));
}

void test_legacy_sending_aliases_tx(void)
{
    /* The legacy SubGhzReadRawStateSending value MUST equal the new TX state
     * so existing callers that still set raw_state = Sending land on TX. */
    TEST_ASSERT_EQUAL((int)SubGhzReadRawStateTX, (int)SubGhzReadRawStateSending);
    TEST_ASSERT_TRUE(subghz_readraw_state_is_tx(SubGhzReadRawStateSending));
}

/* ── Composite scenario: full Idle → TX → TXRepeat → Idle cycle ─────────── */

void test_scenario_idle_tx_repeat_idle(void)
{
    /* User in Idle presses OK */
    SubGhzReadRawState s = SubGhzReadRawStateIdle;
    s = subghz_readraw_tx_state_for_prior(s);
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTX, s);

    /* TX cycle completes with OK still held → transition to repeat */
    s = subghz_readraw_repeat_state_for_tx(s);
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTXRepeat, s);

    /* Another cycle completes with OK still held → stays in repeat */
    s = subghz_readraw_repeat_state_for_tx(s);
    TEST_ASSERT_EQUAL(SubGhzReadRawStateTXRepeat, s);

    /* OK released → cycle completes, return to Idle */
    s = subghz_readraw_prior_state_for_tx(s);
    TEST_ASSERT_EQUAL(SubGhzReadRawStateIdle, s);
}

void test_scenario_loaded_loadkey_round_trip(void)
{
    SubGhzReadRawState s = SubGhzReadRawStateLoaded;
    s = subghz_readraw_tx_state_for_prior(s);
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoadKeyTX, s);
    s = subghz_readraw_repeat_state_for_tx(s);
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoadKeyTXRepeat, s);
    s = subghz_readraw_prior_state_for_tx(s);
    TEST_ASSERT_EQUAL(SubGhzReadRawStateLoaded, s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tx_state_for_prior_idle_to_tx);
    RUN_TEST(test_tx_state_for_prior_loaded_to_loadkey);
    RUN_TEST(test_tx_state_for_prior_unexpected_defaults_to_tx);
    RUN_TEST(test_repeat_state_for_tx_pairs);
    RUN_TEST(test_repeat_state_for_tx_idempotent_on_repeat);
    RUN_TEST(test_repeat_state_for_tx_non_tx_passthrough);
    RUN_TEST(test_prior_state_for_tx_round_trip);
    RUN_TEST(test_prior_state_for_tx_non_tx_passthrough);
    RUN_TEST(test_state_is_tx_positives);
    RUN_TEST(test_state_is_tx_negatives);
    RUN_TEST(test_legacy_sending_aliases_tx);
    RUN_TEST(test_scenario_idle_tx_repeat_idle);
    RUN_TEST(test_scenario_loaded_loadkey_round_trip);
    return UNITY_END();
}
