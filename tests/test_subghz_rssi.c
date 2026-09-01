/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_rssi.c
 * @brief  Host-side unit tests for the Sub-GHz RSSI bar geometry helper.
 *
 * Tests subghz_rssi_fill_w() in Sub_Ghz/subghz_rssi_bar.inc:
 *   - boundary values (floor, ceil)
 *   - clamping below floor and above ceil
 *   - midpoint and quartile linear mapping
 *   - zero bar_max_w (degenerate)
 */

#include "unity.h"

/* Include the .inc directly — it is a header-only geometry helper */
#include "subghz_rssi_bar.inc"

void setUp(void) {}
void tearDown(void) {}

/* ---- boundary values ---------------------------------------------------- */

void test_fill_w_at_floor_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_fill_w(SUBGHZ_RSSI_FLOOR, 108));
}

void test_fill_w_at_ceil_is_max(void)
{
    TEST_ASSERT_EQUAL_UINT8(108, subghz_rssi_fill_w(SUBGHZ_RSSI_CEIL, 108));
}

/* ---- clamping ------------------------------------------------------------ */

void test_fill_w_below_floor_clamped_to_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_fill_w(-120, 108));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_fill_w(-200, 108));
}

void test_fill_w_above_ceil_clamped_to_max(void)
{
    TEST_ASSERT_EQUAL_UINT8(108, subghz_rssi_fill_w(0, 108));
    TEST_ASSERT_EQUAL_UINT8(108, subghz_rssi_fill_w(10, 108));
}

/* ---- linear mapping ----------------------------------------------------- */

void test_fill_w_midpoint(void)
{
    /* Midpoint of [-110, -30] is -70 dBm; should map to ~54 px (108/2) */
    int16_t mid = (int16_t)((SUBGHZ_RSSI_FLOOR + SUBGHZ_RSSI_CEIL) / 2); /* -70 */
    uint8_t w = subghz_rssi_fill_w(mid, 108);
    TEST_ASSERT_EQUAL_UINT8(54, w);
}

void test_fill_w_quartile(void)
{
    /* One quarter of the range: -110 + 20 = -90 dBm → 27 px */
    uint8_t w = subghz_rssi_fill_w(-90, 108);
    TEST_ASSERT_EQUAL_UINT8(27, w);
}

void test_fill_w_three_quarter(void)
{
    /* Three quarters: -110 + 60 = -50 dBm → 81 px */
    uint8_t w = subghz_rssi_fill_w(-50, 108);
    TEST_ASSERT_EQUAL_UINT8(81, w);
}

/* ---- degenerate bar_max_w ----------------------------------------------- */

void test_fill_w_zero_bar_max(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, subghz_rssi_fill_w(-70, 0));
}

/* ---- constants match existing display layout ----------------------------- */

void test_floor_and_ceil_constants(void)
{
    TEST_ASSERT_EQUAL_INT(-110, SUBGHZ_RSSI_FLOOR);
    TEST_ASSERT_EQUAL_INT(-30,  SUBGHZ_RSSI_CEIL);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fill_w_at_floor_is_zero);
    RUN_TEST(test_fill_w_at_ceil_is_max);
    RUN_TEST(test_fill_w_below_floor_clamped_to_zero);
    RUN_TEST(test_fill_w_above_ceil_clamped_to_max);
    RUN_TEST(test_fill_w_midpoint);
    RUN_TEST(test_fill_w_quartile);
    RUN_TEST(test_fill_w_three_quarter);
    RUN_TEST(test_fill_w_zero_bar_max);
    RUN_TEST(test_floor_and_ceil_constants);

    return UNITY_END();
}
