/*
 * test_led_color_ease.c — Unit tests for m1_led_color_ease()
 *
 * Pure-logic function: Gaussian drop-off easing between two RGB colors
 * based on battery level (0–100).  No hardware dependencies.
 *
 * Easing behavior:
 *   level >= 90  → weight = 100 (full color; no easing)
 *   level <  90  → d = (90 - level) / 90
 *                  weight = 100 * (e^(-d^2) - e^(-1)) / (1 - e^(-1))
 *
 * Boundary values: weight(0)=0 → low color; weight(≥90)=100 → full color.
 */

#include "unity.h"
#include "m1_led_color.h"
#include <stddef.h>

void setUp(void)  {}
void tearDown(void) {}

/* ---- Boundary tests ---- */

void test_ease_level_0_returns_low_color(void)
{
    uint8_t r, g, b;
    m1_led_color_ease(0,
                      0xFF, 0x00, 0x80,  /* full */
                      0x00, 0xFF, 0x40,  /* low  */
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(0x00, r);
    TEST_ASSERT_EQUAL_HEX8(0xFF, g);
    TEST_ASSERT_EQUAL_HEX8(0x40, b);
}

void test_ease_level_100_returns_full_color(void)
{
    uint8_t r, g, b;
    m1_led_color_ease(100,
                      0xFF, 0x00, 0x80,
                      0x00, 0xFF, 0x40,
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(0xFF, r);
    TEST_ASSERT_EQUAL_HEX8(0x00, g);
    TEST_ASSERT_EQUAL_HEX8(0x80, b);
}

/* ---- Threshold tests: >= 90 always returns full color ---- */

void test_ease_level_90_returns_full_color(void)
{
    uint8_t r, g, b;
    m1_led_color_ease(90,
                      0xFF, 0x00, 0x80,
                      0x00, 0xFF, 0x40,
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(0xFF, r);
    TEST_ASSERT_EQUAL_HEX8(0x00, g);
    TEST_ASSERT_EQUAL_HEX8(0x80, b);
}

void test_ease_level_91_returns_full_color(void)
{
    uint8_t r, g, b;
    m1_led_color_ease(91,
                      0xFF, 0x00, 0x80,
                      0x00, 0xFF, 0x40,
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(0xFF, r);
    TEST_ASSERT_EQUAL_HEX8(0x00, g);
    TEST_ASSERT_EQUAL_HEX8(0x80, b);
}

void test_ease_level_50_returns_gaussian_blend(void)
{
    uint8_t r, g, b;
    /*
     * level=50: d=(90-50)/90≈0.444, gauss weight≈72
     * full=(0,255,100), low=(0,0,0)
     *   r = 0
     *   g = 255*72/100 = 183
     *   b = 100*72/100 = 72
     */
    m1_led_color_ease(50,
                      0x00, 0xFF, 0x64,  /* full: 0, 255, 100 */
                      0x00, 0x00, 0x00,  /* low:  0,   0,   0 */
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(0x00, r);
    TEST_ASSERT_EQUAL_HEX8(183,  g);  /* 255*72/100 = 183 */
    TEST_ASSERT_EQUAL_HEX8(72,   b);  /* 100*72/100 = 72  */
}

/* ---- Default color test: #331480 ↔ ~#331480 = #CCEB7F ---- */

void test_ease_default_colors_level_0(void)
{
    uint8_t r, g, b;
    m1_led_color_ease(0,
                      0x33, 0x14, 0x80,
                      (uint8_t)~0x33, (uint8_t)~0x14, (uint8_t)~0x80,
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)~0x33, r);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)~0x14, g);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)~0x80, b);
}

void test_ease_default_colors_level_100(void)
{
    uint8_t r, g, b;
    m1_led_color_ease(100,
                      0x33, 0x14, 0x80,
                      (uint8_t)~0x33, (uint8_t)~0x14, (uint8_t)~0x80,
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(0x33, r);
    TEST_ASSERT_EQUAL_HEX8(0x14, g);
    TEST_ASSERT_EQUAL_HEX8(0x80, b);
}

/* ---- Same color for both: easing always returns that color ---- */

void test_ease_same_color(void)
{
    uint8_t r, g, b;
    m1_led_color_ease(42,
                      0xAA, 0xBB, 0xCC,
                      0xAA, 0xBB, 0xCC,
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(0xAA, r);
    TEST_ASSERT_EQUAL_HEX8(0xBB, g);
    TEST_ASSERT_EQUAL_HEX8(0xCC, b);
}

/* ---- Clamping: level > 100 is treated as 100 → returns full color ---- */

void test_ease_level_above_100_clamps(void)
{
    uint8_t r, g, b;
    m1_led_color_ease(200,
                      0xFF, 0x00, 0x80,
                      0x00, 0xFF, 0x40,
                      &r, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(0xFF, r);
    TEST_ASSERT_EQUAL_HEX8(0x00, g);
    TEST_ASSERT_EQUAL_HEX8(0x80, b);
}

/* ---- NULL output pointers are safe ---- */

void test_ease_null_output_r(void)
{
    uint8_t g, b;
    /*
     * level=50: gauss weight ≈ 72
     * full=(0xFF,0xFF,0xFF), low=(0,0,0)
     *   g = b = 255*72/100 = 183
     */
    m1_led_color_ease(50, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
                      NULL, &g, &b);
    TEST_ASSERT_EQUAL_HEX8(183, g);
    TEST_ASSERT_EQUAL_HEX8(183, b);
}

void test_ease_null_output_all(void)
{
    /* Just verify it doesn't crash */
    m1_led_color_ease(50, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
                      NULL, NULL, NULL);
}

/* ---- Monotonicity: increasing level moves toward full color ---- */

void test_ease_monotonic(void)
{
    /*
     * Within the eased region (level < 90), Gaussian weight increases
     * with level, so each channel moves toward the full color.
     * level=25 → weight≈36, level=50 → weight≈72, level=75 → weight≈96
     */
    uint8_t r25, r50, r75;
    uint8_t g_dummy, b_dummy;
    m1_led_color_ease(25, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
                      &r25, &g_dummy, &b_dummy);
    m1_led_color_ease(50, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
                      &r50, &g_dummy, &b_dummy);
    m1_led_color_ease(75, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
                      &r75, &g_dummy, &b_dummy);
    TEST_ASSERT_TRUE(r25 < r50);
    TEST_ASSERT_TRUE(r50 < r75);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ease_level_0_returns_low_color);
    RUN_TEST(test_ease_level_100_returns_full_color);
    RUN_TEST(test_ease_level_90_returns_full_color);
    RUN_TEST(test_ease_level_91_returns_full_color);
    RUN_TEST(test_ease_level_50_returns_gaussian_blend);
    RUN_TEST(test_ease_default_colors_level_0);
    RUN_TEST(test_ease_default_colors_level_100);
    RUN_TEST(test_ease_same_color);
    RUN_TEST(test_ease_level_above_100_clamps);
    RUN_TEST(test_ease_null_output_r);
    RUN_TEST(test_ease_null_output_all);
    RUN_TEST(test_ease_monotonic);

    return UNITY_END();
}
