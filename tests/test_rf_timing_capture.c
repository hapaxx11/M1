/* See COPYING.txt for license details. */

/*
 * test_rf_timing_capture.c
 *
 * Unit tests for the RSSI-burst → timing-array converter
 * (rf_timing_capture.c/h).
 *
 * Convention: positive timing values = mark (signal above threshold),
 *             negative timing values = space (signal at/below threshold).
 */

#include "unity.h"
#include "rf_timing_capture.h"

#include <string.h>

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* NULL / edge-case safety                                                    */
/*============================================================================*/

void test_null_rssi_returns_zero(void)
{
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(NULL, 8, -80, 1000, out, 8);
    TEST_ASSERT_EQUAL(0, n);
}

void test_zero_samples_returns_zero(void)
{
    int16_t rssi[] = { -70 };
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 0, -80, 1000, out, 8);
    TEST_ASSERT_EQUAL(0, n);
}

void test_zero_out_max_returns_zero(void)
{
    int16_t rssi[] = { -70, -70, -90 };
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 3, -80, 1000, out, 0);
    TEST_ASSERT_EQUAL(0, n);
}

void test_zero_sample_period_treated_as_one_us(void)
{
    /* A period of 0 should not crash and should behave like period=1. */
    int16_t rssi[] = { -70, -70, -90, -90 };
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 4, -80, 0, out, 8);
    /* 2 marks → 2µs; 2 spaces → -2µs */
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL( 2, out[0]);
    TEST_ASSERT_EQUAL(-2, out[1]);
}

/*============================================================================*/
/* Single-sample inputs                                                       */
/*============================================================================*/

void test_single_sample_mark(void)
{
    int16_t rssi[] = { -60 };
    int16_t out[4];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 1, -80, 1000, out, 4);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(1000, out[0]);   /* mark, 1 × 1000 µs */
}

void test_single_sample_space(void)
{
    int16_t rssi[] = { -90 };
    int16_t out[4];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 1, -80, 1000, out, 4);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(-1000, out[0]);  /* space, 1 × 1000 µs */
}

void test_single_sample_exactly_at_threshold_is_space(void)
{
    /* Boundary: rssi == threshold is NOT above threshold → space. */
    int16_t rssi[] = { -80 };
    int16_t out[4];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 1, -80, 500, out, 4);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(-500, out[0]);   /* space */
}

/*============================================================================*/
/* Run-length merging                                                         */
/*============================================================================*/

void test_all_marks_merged_into_one(void)
{
    int16_t rssi[] = { -70, -65, -72, -68 };
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 4, -80, 1000, out, 8);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(4000, out[0]);   /* 4 × 1000 µs mark */
}

void test_all_spaces_merged_into_one(void)
{
    int16_t rssi[] = { -85, -90, -92 };
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 3, -80, 500, out, 8);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(-1500, out[0]);  /* 3 × 500 µs space */
}

void test_alternating_mark_space_mark(void)
{
    /* Pattern: M S M  each 2 ms wide */
    int16_t rssi[] = { -70, -70, -90, -90, -65, -65 };
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 6, -80, 1000, out, 8);
    TEST_ASSERT_EQUAL(3, n);
    TEST_ASSERT_EQUAL( 2000, out[0]);   /* mark */
    TEST_ASSERT_EQUAL(-2000, out[1]);   /* space */
    TEST_ASSERT_EQUAL( 2000, out[2]);   /* mark */
}

void test_alternating_space_mark_space(void)
{
    /* Pattern: S M S  each 1 sample, 300 µs period */
    int16_t rssi[] = { -90, -70, -90 };
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 3, -80, 300, out, 8);
    TEST_ASSERT_EQUAL(3, n);
    TEST_ASSERT_EQUAL(-300, out[0]);
    TEST_ASSERT_EQUAL( 300, out[1]);
    TEST_ASSERT_EQUAL(-300, out[2]);
}

void test_realistic_ook_burst(void)
{
    /*
     * Synthetic OOK-style burst: M-M-M-S-M-S-M-M-S-S
     * Threshold = -80 dBm, period = 600 µs.
     *
     * Expected timing elements:
     *   +1800  (3 marks × 600 µs)
     *   -600   (1 space × 600 µs)
     *   +600   (1 mark  × 600 µs)
     *   -600   (1 space × 600 µs)
     *   +1200  (2 marks × 600 µs)
     *   -1200  (2 spaces × 600 µs)
     */
    int16_t rssi[] = {
        -70, -70, -70,   /* 3 marks  */
        -90,             /* 1 space  */
        -70,             /* 1 mark   */
        -90,             /* 1 space  */
        -70, -70,        /* 2 marks  */
        -90, -90,        /* 2 spaces */
    };
    int16_t out[16];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 10, -80, 600, out, 16);
    TEST_ASSERT_EQUAL(6, n);
    TEST_ASSERT_EQUAL( 1800, out[0]);
    TEST_ASSERT_EQUAL( -600, out[1]);
    TEST_ASSERT_EQUAL(  600, out[2]);
    TEST_ASSERT_EQUAL( -600, out[3]);
    TEST_ASSERT_EQUAL( 1200, out[4]);
    TEST_ASSERT_EQUAL(-1200, out[5]);
}

/*============================================================================*/
/* Output buffer capacity limits                                              */
/*============================================================================*/

void test_output_truncated_at_out_max(void)
{
    /* Pattern produces 5 transitions but we only allow 3 output elements. */
    int16_t rssi[] = { -70, -90, -70, -90, -70, -90 };
    int16_t out[3];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 6, -80, 1000, out, 3);
    TEST_ASSERT_EQUAL(3, n);
    TEST_ASSERT_EQUAL( 1000, out[0]);
    TEST_ASSERT_EQUAL(-1000, out[1]);
    TEST_ASSERT_EQUAL( 1000, out[2]);
}

void test_out_max_one_flushes_first_run_only(void)
{
    int16_t rssi[] = { -70, -70, -90, -90 };
    int16_t out[1];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 4, -80, 1000, out, 1);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(2000, out[0]);   /* first run = 2 marks × 1000 µs */
}

/*============================================================================*/
/* Overflow / capping                                                         */
/*============================================================================*/

void test_long_run_capped_at_max(void)
{
    /*
     * Fill 64 mark-samples with a period of 1000 µs each.
     * Total would be 64 000 µs, but that exceeds RF_TIMING_CAPTURE_MAX_US
     * (32 767), so the element must be capped.
     */
    int16_t rssi[64];
    for (int i = 0; i < 64; i++)
        rssi[i] = -70;   /* all marks */

    int16_t out[4];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 64, -80, 1000, out, 4);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(RF_TIMING_CAPTURE_MAX_US, out[0]);
}

void test_single_sample_period_at_max(void)
{
    /* A single sample whose period exactly equals the cap. */
    int16_t rssi[] = { -70 };
    int16_t out[4];
    uint16_t n = rf_timing_from_rssi_burst(
        rssi, 1, -80, RF_TIMING_CAPTURE_MAX_US, out, 4);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(RF_TIMING_CAPTURE_MAX_US, out[0]);
}

/*============================================================================*/
/* Integration: output compatible with subghz_mod_suggest noise-floor        */
/*============================================================================*/

void test_marks_and_spaces_above_noise_floor(void)
{
    /*
     * subghz_mod_suggest ignores samples < 40 µs (NOISE_FLOOR), so the
     * compatibility contract is that each emitted timing magnitude is
     * at least 40 µs and won't be discarded as noise.
     */
    int16_t rssi[] = { -70, -90, -70, -90 };
    int16_t out[8];
    uint16_t n = rf_timing_from_rssi_burst(rssi, 4, -80, 100, out, 8);
    TEST_ASSERT_EQUAL(4, n);
    for (uint16_t i = 0; i < n; i++) {
        int16_t mag = out[i] < 0 ? (int16_t)(-out[i]) : out[i];
        TEST_ASSERT_GREATER_OR_EQUAL(40, mag);
    }
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Null / edge-case safety */
    RUN_TEST(test_null_rssi_returns_zero);
    RUN_TEST(test_zero_samples_returns_zero);
    RUN_TEST(test_zero_out_max_returns_zero);
    RUN_TEST(test_zero_sample_period_treated_as_one_us);

    /* Single-sample inputs */
    RUN_TEST(test_single_sample_mark);
    RUN_TEST(test_single_sample_space);
    RUN_TEST(test_single_sample_exactly_at_threshold_is_space);

    /* Run-length merging */
    RUN_TEST(test_all_marks_merged_into_one);
    RUN_TEST(test_all_spaces_merged_into_one);
    RUN_TEST(test_alternating_mark_space_mark);
    RUN_TEST(test_alternating_space_mark_space);
    RUN_TEST(test_realistic_ook_burst);

    /* Output buffer capacity limits */
    RUN_TEST(test_output_truncated_at_out_max);
    RUN_TEST(test_out_max_one_flushes_first_run_only);

    /* Overflow / capping */
    RUN_TEST(test_long_run_capped_at_max);
    RUN_TEST(test_single_sample_period_at_max);

    /* Integration */
    RUN_TEST(test_marks_and_spaces_above_noise_floor);

    return UNITY_END();
}
