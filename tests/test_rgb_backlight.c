#include "unity.h"
#include "m1_rgb_backlight.h"

void setUp(void) {}
void tearDown(void) {}

void test_hsv_to_rgb_primary_colors(void)
{
    uint8_t r, g, b;

    rgb_backlight_hsv_to_rgb(0, 255, 255, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT8(255, r);
    TEST_ASSERT_EQUAL_UINT8(0, g);
    TEST_ASSERT_EQUAL_UINT8(0, b);

    rgb_backlight_hsv_to_rgb(120, 255, 255, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT8(0, r);
    TEST_ASSERT_EQUAL_UINT8(255, g);
    TEST_ASSERT_EQUAL_UINT8(0, b);

    rgb_backlight_hsv_to_rgb(240, 255, 255, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT8(0, r);
    TEST_ASSERT_EQUAL_UINT8(0, g);
    TEST_ASSERT_EQUAL_UINT8(255, b);
}

void test_rgb_to_grb_packing(void)
{
    uint32_t grb = rgb_backlight_rgb_to_grb(0x12, 0x34, 0x56);
    TEST_ASSERT_EQUAL_HEX32(0x00341256, grb);
}

void test_breathing_profile_endpoints(void)
{
    const uint16_t period = 1000;
    const uint8_t max_brightness = 200;

    TEST_ASSERT_EQUAL_UINT8(0,
        rgb_backlight_breathing_brightness(0, period, max_brightness));
    TEST_ASSERT_EQUAL_UINT8(max_brightness,
        rgb_backlight_breathing_brightness(period / 2, period, max_brightness));
    TEST_ASSERT_EQUAL_UINT8(0,
        rgb_backlight_breathing_brightness(period, period, max_brightness));
}

void test_rainbow_color_advances_with_phase(void)
{
    uint8_t r0, g0, b0;
    uint8_t r1, g1, b1;

    rgb_backlight_rainbow_color(0, 3000, 255, 255, &r0, &g0, &b0);
    rgb_backlight_rainbow_color(1500, 3000, 255, 255, &r1, &g1, &b1);

    TEST_ASSERT_FALSE((r0 == r1) && (g0 == g1) && (b0 == b1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hsv_to_rgb_primary_colors);
    RUN_TEST(test_rgb_to_grb_packing);
    RUN_TEST(test_breathing_profile_endpoints);
    RUN_TEST(test_rainbow_color_advances_with_phase);
    return UNITY_END();
}
