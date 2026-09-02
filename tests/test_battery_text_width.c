/* See COPYING.txt for license details. */

/*
 * test_battery_text_width.c
 *
 * Host-side regression check for the battery percentage text width used by
 * m1_draw_battery_indicator(). The bug was a narrow `uint8_t txt_w` assigned
 * from u8g2_GetStrWidth(), which truncates widths above 255 px and can shift
 * the percentage text left by a large amount. Keep the width in `u8g2_uint_t`
 * and verify that large values remain intact.
 */

#include "unity.h"
#include "m1_display_util.h"
#include <stdint.h>
#include <string.h>

u8g2_uint_t u8g2_GetStrWidth(u8g2_t *u8g2, const char *s)
{
    (void)u8g2;
    return (u8g2_uint_t)(strlen(s) * 70u);
}

void setUp(void) {}
void tearDown(void) {}

void test_battery_pct_width_uses_wide_type(void)
{
    u8g2_t u8g2 = {0};
    u8g2_uint_t width = m1_battery_pct_text_width(&u8g2, "100%");

    /* 4 chars * 70 px/char = 280, larger than uint8_t max. This would have
       wrapped if the width were stored in a uint8_t. */
    TEST_ASSERT_EQUAL_UINT16(280U, width);
    TEST_ASSERT_TRUE(width > UINT8_MAX);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_battery_pct_width_uses_wide_type);
    return UNITY_END();
}
