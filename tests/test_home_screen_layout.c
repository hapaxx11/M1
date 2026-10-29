#include "unity.h"
#include "home_screen_layout.h"
#include "m1_watchdog_boot_loop.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_status_x_centers_short_text_with_existing_margin(void)
{
    TEST_ASSERT_EQUAL_UINT8(22, home_screen_status_x(128u, 6u, 12u));
}

void test_status_x_clamps_oversized_text_instead_of_underflowing(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, home_screen_status_x(128u, 6u, 24u));
}

void test_watchdog_recovery_message_fits_and_is_visible(void)
{
    size_t len = strlen(M1_WDT_RECOVERY_TEXT);
    uint8_t x;

    TEST_ASSERT_LESS_THAN(128u / 6u, len + 1u);
    x = home_screen_status_x(128u, 6u, len);
    TEST_ASSERT_EQUAL_UINT8(1, x);
    TEST_ASSERT_LESS_OR_EQUAL(128u, (uint16_t)x + (uint16_t)len * 6u);
}

void test_status_baseline_clears_bottom_icon(void)
{
    TEST_ASSERT_EQUAL_UINT8(64, HOME_SCREEN_STATUS_BASELINE_Y);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_status_x_centers_short_text_with_existing_margin);
    RUN_TEST(test_status_x_clamps_oversized_text_instead_of_underflowing);
    RUN_TEST(test_watchdog_recovery_message_fits_and_is_visible);
    RUN_TEST(test_status_baseline_clears_bottom_icon);
    return UNITY_END();
}
