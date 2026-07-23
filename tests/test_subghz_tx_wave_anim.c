/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_tx_wave_anim.c
 * @brief  Host-side unit tests for the scrolling TX "sending" wave animation.
 *
 * Coverage for the Sub-GHz Transmitter scene's UX polish (see issue where the
 * "Sending…" screen was just a static dot cycle): pins the phase-advance/wrap
 * behavior and the waveform sample math so the visual scroll stays smooth and
 * bounded without needing hardware to verify it.
 */

#include "unity.h"
#include "subghz_tx_wave_anim.h"

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* Phase advance / wrap                                                       */
/*============================================================================*/

static void test_step_increments_phase(void)
{
    uint8_t phase = 0;
    subghz_tx_wave_anim_step(&phase);
    TEST_ASSERT_EQUAL_UINT8(1, phase);

    subghz_tx_wave_anim_step(&phase);
    TEST_ASSERT_EQUAL_UINT8(2, phase);
}

static void test_step_wraps_at_period(void)
{
    uint8_t phase = SUBGHZ_TX_WAVE_ANIM_PHASE_PERIOD - 1U;
    subghz_tx_wave_anim_step(&phase);
    TEST_ASSERT_EQUAL_UINT8(0, phase);
}

static void test_step_normalizes_out_of_range_input(void)
{
    /* A stray/free-running counter beyond the period must still fold back
     * into range rather than accumulating drift. */
    uint8_t phase = (uint8_t)(SUBGHZ_TX_WAVE_ANIM_PHASE_PERIOD * 2U + 5U);
    subghz_tx_wave_anim_step(&phase);
    TEST_ASSERT_EQUAL_UINT8(6, phase);
}

static void test_step_tolerates_null(void)
{
    /* Must not crash; nothing else to assert. */
    subghz_tx_wave_anim_step(NULL);
}

/*============================================================================*/
/* Waveform sampling                                                          */
/*============================================================================*/

static void test_sample_zero_amplitude_is_flat(void)
{
    TEST_ASSERT_EQUAL_INT8(0, subghz_tx_wave_anim_sample(0, 0, 0));
    TEST_ASSERT_EQUAL_INT8(0, subghz_tx_wave_anim_sample(64, 5, 0));
}

static void test_sample_zero_crossing_at_origin(void)
{
    TEST_ASSERT_EQUAL_INT8(0, subghz_tx_wave_anim_sample(0, 0, 50));
}

static void test_sample_peak_at_quarter_period(void)
{
    /* column 64 with phase 0 lands on the LUT's quarter-period peak. */
    TEST_ASSERT_EQUAL_INT8(50, subghz_tx_wave_anim_sample(64, 0, 50));
}

static void test_sample_trough_at_three_quarter_period(void)
{
    TEST_ASSERT_EQUAL_INT8(-50, subghz_tx_wave_anim_sample(192, 0, 50));
}

static void test_sample_stays_within_amplitude_bounds(void)
{
    for (uint16_t column = 0; column < 256U; column++)
    {
        int8_t v = subghz_tx_wave_anim_sample(column, 17, 40);
        TEST_ASSERT_TRUE(v >= -40 && v <= 40);
    }
}

static void test_sample_scrolls_with_phase(void)
{
    /* Advancing the phase by one tick shifts the wave horizontally by 4
     * columns (256 index steps / 64 phase steps == 4), so sampling 4
     * columns further at the earlier phase reproduces the same value. */
    int8_t a = subghz_tx_wave_anim_sample(10, 1, 100);
    int8_t b = subghz_tx_wave_anim_sample(14, 0, 100);
    TEST_ASSERT_EQUAL_INT8(a, b);
}

static void test_sample_full_period_returns_to_start(void)
{
    /* A full phase period (64 ticks) should bring the wave back to the same
     * shape at a fixed column. */
    int8_t a = subghz_tx_wave_anim_sample(3, 0, 90);
    int8_t b = subghz_tx_wave_anim_sample(3, SUBGHZ_TX_WAVE_ANIM_PHASE_PERIOD, 90);
    TEST_ASSERT_EQUAL_INT8(a, b);
}

/*============================================================================*/
/* Runner                                                                     */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_step_increments_phase);
    RUN_TEST(test_step_wraps_at_period);
    RUN_TEST(test_step_normalizes_out_of_range_input);
    RUN_TEST(test_step_tolerates_null);
    RUN_TEST(test_sample_zero_amplitude_is_flat);
    RUN_TEST(test_sample_zero_crossing_at_origin);
    RUN_TEST(test_sample_peak_at_quarter_period);
    RUN_TEST(test_sample_trough_at_three_quarter_period);
    RUN_TEST(test_sample_stays_within_amplitude_bounds);
    RUN_TEST(test_sample_scrolls_with_phase);
    RUN_TEST(test_sample_full_period_returns_to_start);
    return UNITY_END();
}
