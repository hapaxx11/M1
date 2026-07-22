/* See COPYING.txt for license details. */

/*
 * test_rf_smart_scan.c
 *
 * Unit tests for the Smart ID pre-scan probe-plan builder
 * (rf_smart_scan.c/h).
 */

#include "unity.h"
#include "rf_smart_scan.h"
#include "rf_scan_plan.h"

#include <string.h>
#include <stdint.h>

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* rf_smart_scan_freq_valid                                                   */
/*============================================================================*/

void test_freq_valid_lower_bound(void)
{
    TEST_ASSERT_TRUE(rf_smart_scan_freq_valid(RF_SMART_SCAN_FREQ_MIN_HZ));
}

void test_freq_valid_upper_bound(void)
{
    TEST_ASSERT_TRUE(rf_smart_scan_freq_valid(RF_SMART_SCAN_FREQ_MAX_HZ));
}

void test_freq_valid_typical_433(void)
{
    TEST_ASSERT_TRUE(rf_smart_scan_freq_valid(433920000UL));
}

void test_freq_valid_typical_868(void)
{
    TEST_ASSERT_TRUE(rf_smart_scan_freq_valid(868350000UL));
}

void test_freq_valid_typical_915(void)
{
    TEST_ASSERT_TRUE(rf_smart_scan_freq_valid(915000000UL));
}

void test_freq_invalid_below_min(void)
{
    TEST_ASSERT_FALSE(rf_smart_scan_freq_valid(RF_SMART_SCAN_FREQ_MIN_HZ - 1UL));
}

void test_freq_invalid_zero(void)
{
    TEST_ASSERT_FALSE(rf_smart_scan_freq_valid(0));
}

void test_freq_invalid_above_max(void)
{
    TEST_ASSERT_FALSE(rf_smart_scan_freq_valid(RF_SMART_SCAN_FREQ_MAX_HZ + 1UL));
}

void test_freq_invalid_2_4ghz(void)
{
    TEST_ASSERT_FALSE(rf_smart_scan_freq_valid(2400000000UL));
}

/*============================================================================*/
/* rf_smart_scan_build_plan — NULL / empty guard                              */
/*============================================================================*/

void test_build_plan_null_freq_hz(void)
{
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(NULL, 4, out, 4);
    TEST_ASSERT_EQUAL_UINT8(0, n);
}

void test_build_plan_null_out(void)
{
    uint32_t freqs[] = { 433920000UL };
    uint8_t n = rf_smart_scan_build_plan(freqs, 1, NULL, 4);
    TEST_ASSERT_EQUAL_UINT8(0, n);
}

void test_build_plan_max_out_zero(void)
{
    uint32_t freqs[] = { 433920000UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 1, out, 0);
    TEST_ASSERT_EQUAL_UINT8(0, n);
}

void test_build_plan_empty_input(void)
{
    uint32_t freqs[1] = { 0 }; /* dummy storage; count=0 means nothing is read */
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 0, out, 4);
    TEST_ASSERT_EQUAL_UINT8(0, n);
}

/*============================================================================*/
/* rf_smart_scan_build_plan — single entry                                    */
/*============================================================================*/

void test_build_plan_single_433(void)
{
    uint32_t freqs[] = { 433920000UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 1, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_EQUAL_UINT32(433920000UL, out[0].freq_hz);
    TEST_ASSERT_FALSE(out[0].use_915);
    TEST_ASSERT_NOT_NULL(out[0].label);
}

void test_build_plan_single_868_use915_flag(void)
{
    uint32_t freqs[] = { 868350000UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 1, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_TRUE(out[0].use_915);
}

void test_build_plan_single_915(void)
{
    uint32_t freqs[] = { 915000000UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 1, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_TRUE(out[0].use_915);
    TEST_ASSERT_EQUAL_UINT32(915000000UL, out[0].freq_hz);
}

void test_build_plan_boundary_849_not_915(void)
{
    /* Just below the 850 MHz boundary — must not set use_915. */
    uint32_t freqs[] = { RF_SCAN_915_BOUNDARY_HZ - 1UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 1, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_FALSE(out[0].use_915);
}

void test_build_plan_boundary_850_is_915(void)
{
    uint32_t freqs[] = { RF_SCAN_915_BOUNDARY_HZ };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 1, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_TRUE(out[0].use_915);
}

/*============================================================================*/
/* rf_smart_scan_build_plan — invalid frequencies are skipped                 */
/*============================================================================*/

void test_build_plan_invalid_freq_skipped(void)
{
    uint32_t freqs[] = { 100000000UL, 433920000UL, 2400000000UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 3, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_EQUAL_UINT32(433920000UL, out[0].freq_hz);
}

void test_build_plan_all_invalid(void)
{
    uint32_t freqs[] = { 0, 100000000UL, 2400000000UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 3, out, 4);
    TEST_ASSERT_EQUAL_UINT8(0, n);
}

/*============================================================================*/
/* rf_smart_scan_build_plan — deduplication                                   */
/*============================================================================*/

void test_build_plan_dedup_exact(void)
{
    uint32_t freqs[] = { 433920000UL, 433920000UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 2, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
}

void test_build_plan_dedup_within_radius(void)
{
    /* 50 kHz apart — within the 100 kHz dedup radius */
    uint32_t freqs[] = { 433920000UL, 433970000UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 2, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
}

void test_build_plan_dedup_at_boundary(void)
{
    /* Exactly RF_SMART_SCAN_DEDUP_HZ apart — still deduplicated (< not <=) */
    uint32_t freqs[] = { 433920000UL, 433920000UL + RF_SMART_SCAN_DEDUP_HZ - 1UL };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 2, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
}

void test_build_plan_no_dedup_beyond_radius(void)
{
    /* RF_SMART_SCAN_DEDUP_HZ apart — distinct channels */
    uint32_t freqs[] = { 433920000UL, 433920000UL + RF_SMART_SCAN_DEDUP_HZ };
    rf_scan_point_t out[4];
    uint8_t n = rf_smart_scan_build_plan(freqs, 2, out, 4);
    TEST_ASSERT_EQUAL_UINT8(2, n);
}

void test_build_plan_multi_distinct(void)
{
    uint32_t freqs[] = { 315000000UL, 433920000UL, 868350000UL, 915000000UL };
    rf_scan_point_t out[8];
    uint8_t n = rf_smart_scan_build_plan(freqs, 4, out, 8);
    TEST_ASSERT_EQUAL_UINT8(4, n);
    TEST_ASSERT_EQUAL_UINT32(315000000UL, out[0].freq_hz);
    TEST_ASSERT_EQUAL_UINT32(433920000UL, out[1].freq_hz);
    TEST_ASSERT_EQUAL_UINT32(868350000UL, out[2].freq_hz);
    TEST_ASSERT_EQUAL_UINT32(915000000UL, out[3].freq_hz);
}

/*============================================================================*/
/* rf_smart_scan_build_plan — max_out cap                                     */
/*============================================================================*/

void test_build_plan_capped_by_max_out(void)
{
    uint32_t freqs[] = { 315000000UL, 433920000UL, 868350000UL, 915000000UL };
    rf_scan_point_t out[2];
    uint8_t n = rf_smart_scan_build_plan(freqs, 4, out, 2);
    TEST_ASSERT_EQUAL_UINT8(2, n);
    TEST_ASSERT_EQUAL_UINT32(315000000UL, out[0].freq_hz);
    TEST_ASSERT_EQUAL_UINT32(433920000UL, out[1].freq_hz);
}

/*============================================================================*/
/* rf_smart_scan_build_plan — label content                                   */
/*============================================================================*/

void test_build_plan_label_433_92(void)
{
    uint32_t freqs[] = { 433920000UL };
    rf_scan_point_t out[4];
    rf_smart_scan_build_plan(freqs, 1, out, 4);
    TEST_ASSERT_NOT_NULL(out[0].label);
    TEST_ASSERT_EQUAL_STRING("433.92", out[0].label);
}

void test_build_plan_label_868_35(void)
{
    uint32_t freqs[] = { 868350000UL };
    rf_scan_point_t out[4];
    rf_smart_scan_build_plan(freqs, 1, out, 4);
    TEST_ASSERT_NOT_NULL(out[0].label);
    TEST_ASSERT_EQUAL_STRING("868.35", out[0].label);
}

void test_build_plan_label_315_00(void)
{
    uint32_t freqs[] = { 315000000UL };
    rf_scan_point_t out[4];
    rf_smart_scan_build_plan(freqs, 1, out, 4);
    TEST_ASSERT_NOT_NULL(out[0].label);
    TEST_ASSERT_EQUAL_STRING("315.00", out[0].label);
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_freq_valid_lower_bound);
    RUN_TEST(test_freq_valid_upper_bound);
    RUN_TEST(test_freq_valid_typical_433);
    RUN_TEST(test_freq_valid_typical_868);
    RUN_TEST(test_freq_valid_typical_915);
    RUN_TEST(test_freq_invalid_below_min);
    RUN_TEST(test_freq_invalid_zero);
    RUN_TEST(test_freq_invalid_above_max);
    RUN_TEST(test_freq_invalid_2_4ghz);

    RUN_TEST(test_build_plan_null_freq_hz);
    RUN_TEST(test_build_plan_null_out);
    RUN_TEST(test_build_plan_max_out_zero);
    RUN_TEST(test_build_plan_empty_input);

    RUN_TEST(test_build_plan_single_433);
    RUN_TEST(test_build_plan_single_868_use915_flag);
    RUN_TEST(test_build_plan_single_915);
    RUN_TEST(test_build_plan_boundary_849_not_915);
    RUN_TEST(test_build_plan_boundary_850_is_915);

    RUN_TEST(test_build_plan_invalid_freq_skipped);
    RUN_TEST(test_build_plan_all_invalid);

    RUN_TEST(test_build_plan_dedup_exact);
    RUN_TEST(test_build_plan_dedup_within_radius);
    RUN_TEST(test_build_plan_dedup_at_boundary);
    RUN_TEST(test_build_plan_no_dedup_beyond_radius);
    RUN_TEST(test_build_plan_multi_distinct);

    RUN_TEST(test_build_plan_capped_by_max_out);

    RUN_TEST(test_build_plan_label_433_92);
    RUN_TEST(test_build_plan_label_868_35);
    RUN_TEST(test_build_plan_label_315_00);

    return UNITY_END();
}
