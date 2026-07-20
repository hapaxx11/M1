/* See COPYING.txt for license details. */

/*
 * test_rf_repetition.c
 *
 * Unit tests for rf_repetition_detect — repeated-burst detection (the "[x2]"
 * badge from RF Rosetta).  Pure timing analysis over a signed mark/space
 * sample array.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R rf_repetition
 */

#include <string.h>
#include "unity.h"
#include "rf_repetition.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Waveform builders                                                          */
/*============================================================================*/

/* Append one burst of `pairs` mark/space pairs of nominal duration `us`.
 * Returns the new index. */
static uint16_t push_burst(int16_t *buf, uint16_t idx, int pairs, int us)
{
    for (int i = 0; i < pairs; i++)
    {
        buf[idx++] = (int16_t)us;        /* mark  */
        buf[idx++] = (int16_t)(-us);     /* space */
    }
    return idx;
}

/* Append an inter-packet gap (a long space). */
static uint16_t push_gap(int16_t *buf, uint16_t idx, int us)
{
    buf[idx++] = (int16_t)(-us);
    return idx;
}

/*============================================================================*/
/* Guard / edge cases                                                         */
/*============================================================================*/

void test_null_is_single(void)
{
    rf_repetition_t r = rf_repetition_detect(NULL, 128);
    TEST_ASSERT_EQUAL_UINT8(1, r.count);
    TEST_ASSERT_EQUAL_UINT8(0, r.confidence);
}

void test_too_short_is_single(void)
{
    int16_t buf[4] = { 300, -300, 300, -300 };
    rf_repetition_t r = rf_repetition_detect(buf, 4);
    TEST_ASSERT_EQUAL_UINT8(1, r.count);
}

/*============================================================================*/
/* Core behaviour                                                             */
/*============================================================================*/

void test_single_burst_no_repeat(void)
{
    int16_t buf[64];
    uint16_t n = push_burst(buf, 0, 20, 300);
    rf_repetition_t r = rf_repetition_detect(buf, n);
    TEST_ASSERT_EQUAL_UINT8(1, r.count);
    TEST_ASSERT_EQUAL_UINT8(1, r.burst_total);
}

void test_three_identical_bursts(void)
{
    int16_t buf[256];
    uint16_t n = 0;
    for (int b = 0; b < 3; b++)
    {
        n = push_burst(buf, n, 18, 300);
        n = push_gap(buf, n, 8000);     /* long inter-packet gap */
    }
    rf_repetition_t r = rf_repetition_detect(buf, n);
    TEST_ASSERT_EQUAL_UINT8(3, r.count);
    TEST_ASSERT_TRUE(r.burst_total >= 3);
    TEST_ASSERT_TRUE(r.confidence >= 50);
    /* burst_pulses should be near 18 marks + 18 spaces = 36 pulses */
    TEST_ASSERT_TRUE(r.burst_pulses >= 30 && r.burst_pulses <= 40);
}

void test_five_repeats_high_confidence(void)
{
    int16_t buf[512];
    uint16_t n = 0;
    for (int b = 0; b < 5; b++)
    {
        n = push_burst(buf, n, 16, 400);
        n = push_gap(buf, n, 10000);
    }
    rf_repetition_t r = rf_repetition_detect(buf, n);
    TEST_ASSERT_EQUAL_UINT8(5, r.count);
    TEST_ASSERT_TRUE(r.confidence >= 60);
}

void test_odd_burst_out_still_counts_majority(void)
{
    /* Three matching bursts of 20 pairs + one short 8-pair burst.  The
     * majority group (3) should win. */
    int16_t buf[512];
    uint16_t n = 0;
    n = push_burst(buf, n, 20, 300); n = push_gap(buf, n, 8000);
    n = push_burst(buf, n, 20, 300); n = push_gap(buf, n, 8000);
    n = push_burst(buf, n, 8,  300); n = push_gap(buf, n, 8000);  /* outlier */
    n = push_burst(buf, n, 20, 300); n = push_gap(buf, n, 8000);
    rf_repetition_t r = rf_repetition_detect(buf, n);
    TEST_ASSERT_EQUAL_UINT8(3, r.count);
    TEST_ASSERT_EQUAL_UINT8(4, r.burst_total);
}

void test_long_mark_does_not_split_burst(void)
{
    /* A long *mark* (positive) inside a burst must not split it — only a long
     * space (negative) is an inter-packet gap. */
    int16_t buf[128];
    uint16_t n = 0;
    for (int i = 0; i < 10; i++) { buf[n++] = 300; buf[n++] = -300; }
    buf[n++] = 9000;   /* long mark — NOT a gap */
    for (int i = 0; i < 10; i++) { buf[n++] = 300; buf[n++] = -300; }
    rf_repetition_t r = rf_repetition_detect(buf, n);
    TEST_ASSERT_EQUAL_UINT8(1, r.count);       /* one continuous burst */
    TEST_ASSERT_EQUAL_UINT8(1, r.burst_total);
}

void test_glitches_below_floor_ignored(void)
{
    int16_t buf[256];
    uint16_t n = 0;
    for (int b = 0; b < 2; b++)
    {
        for (int i = 0; i < 12; i++)
        {
            buf[n++] = 5;          /* sub-floor glitch, ignored */
            buf[n++] = 300;        /* real mark */
            buf[n++] = -300;       /* real space */
        }
        n = push_gap(buf, n, 8000);
    }
    rf_repetition_t r = rf_repetition_detect(buf, n);
    TEST_ASSERT_EQUAL_UINT8(2, r.count);
}

/*============================================================================*/
/* Runner                                                                     */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_null_is_single);
    RUN_TEST(test_too_short_is_single);

    RUN_TEST(test_single_burst_no_repeat);
    RUN_TEST(test_three_identical_bursts);
    RUN_TEST(test_five_repeats_high_confidence);
    RUN_TEST(test_odd_burst_out_still_counts_majority);
    RUN_TEST(test_long_mark_does_not_split_burst);
    RUN_TEST(test_glitches_below_floor_ignored);

    return UNITY_END();
}
