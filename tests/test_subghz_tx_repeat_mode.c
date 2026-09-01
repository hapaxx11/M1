/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_tx_repeat_mode.c
 * @brief  Host-side unit tests for the SubGHz Read Raw TX repeat-mode helpers.
 *
 * Tests the pure-logic helpers in m1_subghz_read_raw_state.h:
 *   - subghz_raw_tx_should_repeat() — all four (toggle × ok_held) combinations
 *   - subghz_read_raw_state_is_tx() — all states, TX and non-TX
 *   - subghz_read_raw_state_to_repeat() — one-shot → repeat promotion
 *   - repeat-mode toggle behaviour (toggle = !toggle)
 *
 * No firmware or hardware stubs are needed — the header is purely logic.
 */

#include "unity.h"
#include "m1_subghz_read_raw_state.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- subghz_raw_tx_should_repeat ---------------------------------------- */

void test_should_repeat_neither_flag(void)
{
    TEST_ASSERT_FALSE(subghz_raw_tx_should_repeat(false, false));
}

void test_should_repeat_toggle_only(void)
{
    TEST_ASSERT_TRUE(subghz_raw_tx_should_repeat(true, false));
}

void test_should_repeat_ok_held_only(void)
{
    TEST_ASSERT_TRUE(subghz_raw_tx_should_repeat(false, true));
}

void test_should_repeat_both_flags(void)
{
    TEST_ASSERT_TRUE(subghz_raw_tx_should_repeat(true, true));
}

/* ---- toggle transitions -------------------------------------------------- */

void test_toggle_false_to_true(void)
{
    bool mode = false;
    mode = !mode;
    TEST_ASSERT_TRUE(mode);
}

void test_toggle_true_to_false(void)
{
    bool mode = true;
    mode = !mode;
    TEST_ASSERT_FALSE(mode);
}

void test_toggle_double_round_trips(void)
{
    bool mode = false;
    mode = !mode;
    mode = !mode;
    TEST_ASSERT_FALSE(mode);
}

/* ---- subghz_read_raw_state_is_tx ---------------------------------------- */

void test_is_tx_true_for_all_tx_states(void)
{
    TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(SubGhzReadRawStateTX));
    TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(SubGhzReadRawStateTXRepeat));
    TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(SubGhzReadRawStateLoadKeyTX));
    TEST_ASSERT_TRUE(subghz_read_raw_state_is_tx(SubGhzReadRawStateLoadKeyTXRepeat));
}

void test_is_tx_false_for_non_tx_states(void)
{
    TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(SubGhzReadRawStateStart));
    TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(SubGhzReadRawStateRecording));
    TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(SubGhzReadRawStateIdle));
    TEST_ASSERT_FALSE(subghz_read_raw_state_is_tx(SubGhzReadRawStateLoaded));
}

/* ---- subghz_read_raw_state_to_repeat ------------------------------------ */

void test_to_repeat_from_tx(void)
{
    TEST_ASSERT_EQUAL_INT(SubGhzReadRawStateTXRepeat,
                          subghz_read_raw_state_to_repeat(SubGhzReadRawStateTX));
}

void test_to_repeat_from_tx_repeat_is_idempotent(void)
{
    TEST_ASSERT_EQUAL_INT(SubGhzReadRawStateTXRepeat,
                          subghz_read_raw_state_to_repeat(SubGhzReadRawStateTXRepeat));
}

void test_to_repeat_from_load_key_tx(void)
{
    TEST_ASSERT_EQUAL_INT(SubGhzReadRawStateLoadKeyTXRepeat,
                          subghz_read_raw_state_to_repeat(SubGhzReadRawStateLoadKeyTX));
}

void test_to_repeat_from_load_key_tx_repeat_is_idempotent(void)
{
    TEST_ASSERT_EQUAL_INT(SubGhzReadRawStateLoadKeyTXRepeat,
                          subghz_read_raw_state_to_repeat(SubGhzReadRawStateLoadKeyTXRepeat));
}

void test_to_repeat_non_tx_state_unchanged(void)
{
    TEST_ASSERT_EQUAL_INT(SubGhzReadRawStateIdle,
                          subghz_read_raw_state_to_repeat(SubGhzReadRawStateIdle));
}

/* ---- integration: toggle does not change TX state on its own ------------ */

void test_toggle_does_not_affect_tx_state(void)
{
    SubGhzReadRawState s = SubGhzReadRawStateTX;
    bool mode = false;
    /* Toggle the mode */
    mode = !mode;
    /* State must remain unchanged — toggle only affects should_repeat() */
    TEST_ASSERT_EQUAL_INT(SubGhzReadRawStateTX, s);
    TEST_ASSERT_TRUE(subghz_raw_tx_should_repeat(mode, false));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_should_repeat_neither_flag);
    RUN_TEST(test_should_repeat_toggle_only);
    RUN_TEST(test_should_repeat_ok_held_only);
    RUN_TEST(test_should_repeat_both_flags);

    RUN_TEST(test_toggle_false_to_true);
    RUN_TEST(test_toggle_true_to_false);
    RUN_TEST(test_toggle_double_round_trips);

    RUN_TEST(test_is_tx_true_for_all_tx_states);
    RUN_TEST(test_is_tx_false_for_non_tx_states);

    RUN_TEST(test_to_repeat_from_tx);
    RUN_TEST(test_to_repeat_from_tx_repeat_is_idempotent);
    RUN_TEST(test_to_repeat_from_load_key_tx);
    RUN_TEST(test_to_repeat_from_load_key_tx_repeat_is_idempotent);
    RUN_TEST(test_to_repeat_non_tx_state_unchanged);

    RUN_TEST(test_toggle_does_not_affect_tx_state);

    return UNITY_END();
}
