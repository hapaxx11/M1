/* test_wifi_ch_analysis.c — Unit tests for wifi_ch_analysis.c
 *
 * Pure-logic channel analysis helpers extracted from wifi_survey_24g().
 * Tests verify channel counting, busiest/best detection, RSSI tracking,
 * bar-height scaling, and edge cases (empty input, out-of-range channels).
 */
#include "unity.h"
#include "wifi_ch_analysis.h"

void setUp(void)  {}
void tearDown(void) {}

/* ---- wifi_ch_analysis_compute ---- */

void test_compute_empty_input(void)
{
    wifi_ch_analysis_t a;
    wifi_ch_analysis_compute(NULL, NULL, 0, &a);

    TEST_ASSERT_EQUAL_UINT16(0, a.total_aps);
    TEST_ASSERT_EQUAL_INT8(-128, a.strongest_rssi);
    TEST_ASSERT_EQUAL_UINT8(1, a.busiest_ch);
    TEST_ASSERT_EQUAL_UINT8(1, a.best_ch);
    TEST_ASSERT_EQUAL_UINT8(0, a.busiest_count);
    TEST_ASSERT_EQUAL_UINT8(0, a.best_count);
}

void test_compute_null_output_does_not_crash(void)
{
    uint8_t ch[] = { 1 };
    int8_t  rs[] = { -50 };
    wifi_ch_analysis_compute(ch, rs, 1, NULL);
    /* no crash = pass */
}

void test_compute_single_ap(void)
{
    uint8_t ch[] = { 6 };
    int8_t  rs[] = { -42 };
    wifi_ch_analysis_t a;
    wifi_ch_analysis_compute(ch, rs, 1, &a);

    TEST_ASSERT_EQUAL_UINT16(1, a.total_aps);
    TEST_ASSERT_EQUAL_INT8(-42, a.strongest_rssi);
    TEST_ASSERT_EQUAL_UINT8(6, a.busiest_ch);
    TEST_ASSERT_EQUAL_UINT8(1, a.busiest_count);
    /* best_ch: channel 1 has 0 APs (quietest) */
    TEST_ASSERT_EQUAL_UINT8(0, a.best_count);
    TEST_ASSERT_TRUE(a.best_ch >= 1 && a.best_ch <= 13);
    TEST_ASSERT_TRUE(a.best_ch != 6); /* ch 6 has 1 AP, many others have 0 */
}

void test_compute_multiple_channels(void)
{
    uint8_t ch[] = { 1, 1, 1, 6, 6, 11 };
    int8_t  rs[] = { -60, -55, -70, -40, -45, -30 };
    wifi_ch_analysis_t a;
    wifi_ch_analysis_compute(ch, rs, 6, &a);

    TEST_ASSERT_EQUAL_UINT16(6, a.total_aps);
    TEST_ASSERT_EQUAL_INT8(-30, a.strongest_rssi);
    TEST_ASSERT_EQUAL_UINT8(1, a.busiest_ch);  /* 3 APs */
    TEST_ASSERT_EQUAL_UINT8(3, a.busiest_count);
    TEST_ASSERT_EQUAL_UINT8(0, a.best_count);   /* many channels have 0 */
}

void test_compute_out_of_range_channels_skipped(void)
{
    uint8_t ch[] = { 0, 14, 15, 255, 6 };
    int8_t  rs[] = { -50, -50, -50, -50, -42 };
    wifi_ch_analysis_t a;
    wifi_ch_analysis_compute(ch, rs, 5, &a);

    /* Only channel 6 should be counted */
    TEST_ASSERT_EQUAL_UINT16(1, a.total_aps);
    TEST_ASSERT_EQUAL_UINT8(1, a.ch_count[6]);
    /* RSSI should still track all APs (including out-of-range channels) */
    TEST_ASSERT_EQUAL_INT8(-42, a.strongest_rssi);
}

void test_compute_all_channels_populated(void)
{
    uint8_t ch[13];
    int8_t  rs[13];
    for (uint8_t i = 0; i < 13; i++)
    {
        ch[i] = i + 1;
        rs[i] = (int8_t)(-80 + i);
    }
    wifi_ch_analysis_t a;
    wifi_ch_analysis_compute(ch, rs, 13, &a);

    TEST_ASSERT_EQUAL_UINT16(13, a.total_aps);
    /* All channels have exactly 1 AP — busiest is first found (ch 1),
     * best is also first found with min count 1 (ch 1) */
    TEST_ASSERT_EQUAL_UINT8(1, a.busiest_count);
    TEST_ASSERT_EQUAL_UINT8(1, a.best_count);
    TEST_ASSERT_EQUAL_INT8(-80 + 12, a.strongest_rssi); /* -68 */
}

void test_compute_strongest_rssi_across_all(void)
{
    /* RSSI is tracked for ALL APs, even those on out-of-range channels */
    uint8_t ch[] = { 0, 1, 255 };
    int8_t  rs[] = { -10, -80, -5 };
    wifi_ch_analysis_t a;
    wifi_ch_analysis_compute(ch, rs, 3, &a);

    TEST_ASSERT_EQUAL_INT8(-5, a.strongest_rssi);
    TEST_ASSERT_EQUAL_UINT16(1, a.total_aps); /* only ch 1 counted */
}

void test_compute_ch_count_array_correct(void)
{
    uint8_t ch[] = { 1, 1, 3, 3, 3, 11, 11 };
    int8_t  rs[] = { -50, -50, -50, -50, -50, -50, -50 };
    wifi_ch_analysis_t a;
    wifi_ch_analysis_compute(ch, rs, 7, &a);

    TEST_ASSERT_EQUAL_UINT8(2, a.ch_count[1]);
    TEST_ASSERT_EQUAL_UINT8(0, a.ch_count[2]);
    TEST_ASSERT_EQUAL_UINT8(3, a.ch_count[3]);
    TEST_ASSERT_EQUAL_UINT8(0, a.ch_count[4]);
    TEST_ASSERT_EQUAL_UINT8(2, a.ch_count[11]);
    TEST_ASSERT_EQUAL_UINT8(3, a.busiest_ch);
    TEST_ASSERT_EQUAL_UINT8(3, a.busiest_count);
}

/* ---- wifi_ch_bar_height ---- */

void test_bar_height_zero_count(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, wifi_ch_bar_height(0, 10, 28));
}

void test_bar_height_max_count(void)
{
    /* count == max_count → full chart height */
    TEST_ASSERT_EQUAL_UINT8(28, wifi_ch_bar_height(10, 10, 28));
}

void test_bar_height_half(void)
{
    TEST_ASSERT_EQUAL_UINT8(14, wifi_ch_bar_height(5, 10, 28));
}

void test_bar_height_minimum_clamp(void)
{
    /* 1 AP out of 255 max → (1*28/255)=0 → clamped to 1 */
    TEST_ASSERT_EQUAL_UINT8(1, wifi_ch_bar_height(1, 255, 28));
}

void test_bar_height_max_count_zero_treated_as_one(void)
{
    /* max_count == 0 is defensive: treated as 1 */
    TEST_ASSERT_EQUAL_UINT8(28, wifi_ch_bar_height(1, 0, 28));
}

void test_bar_height_various_chart_sizes(void)
{
    TEST_ASSERT_EQUAL_UINT8(50, wifi_ch_bar_height(5, 5, 50));
    TEST_ASSERT_EQUAL_UINT8(1,  wifi_ch_bar_height(1, 100, 10));
}

int main(void)
{
    UNITY_BEGIN();

    /* wifi_ch_analysis_compute */
    RUN_TEST(test_compute_empty_input);
    RUN_TEST(test_compute_null_output_does_not_crash);
    RUN_TEST(test_compute_single_ap);
    RUN_TEST(test_compute_multiple_channels);
    RUN_TEST(test_compute_out_of_range_channels_skipped);
    RUN_TEST(test_compute_all_channels_populated);
    RUN_TEST(test_compute_strongest_rssi_across_all);
    RUN_TEST(test_compute_ch_count_array_correct);

    /* wifi_ch_bar_height */
    RUN_TEST(test_bar_height_zero_count);
    RUN_TEST(test_bar_height_max_count);
    RUN_TEST(test_bar_height_half);
    RUN_TEST(test_bar_height_minimum_clamp);
    RUN_TEST(test_bar_height_max_count_zero_treated_as_one);
    RUN_TEST(test_bar_height_various_chart_sizes);

    return UNITY_END();
}
