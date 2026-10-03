/* See COPYING.txt for license details. */

/*
 * test_lcd_saver.c
 *
 * Host-side unit tests for the LCD backlight screen-saver decision state
 * machine (m1_csrc/lcd_saver.c). Pure logic, no HAL/RTOS dependencies.
 *
 * Includes the regression test for the NFC/RFID result-screen backlight bug:
 * a long scan with no button press dims the backlight, so the successful-read
 * info screen would appear dark; restarting the inactivity timer (as
 * m1_lcd_wake_restart_timer() does on the firmware side) must wake the
 * backlight and keep it on for a full timeout window.
 */

#include "unity.h"
#include "lcd_saver.h"

#define TIMEOUT_MS  30000u   /* default M1 inactivity window (30 s) */

void setUp(void) { }
void tearDown(void) { }

/* --- init ---------------------------------------------------------------- */

void test_ctx_init_backlight_on(void)
{
    lcd_saver_ctx_t ctx;
    ctx.backlight_off = true;
    lcd_saver_ctx_init(&ctx);
    TEST_ASSERT_FALSE(ctx.backlight_off);
}

void test_null_ctx_is_safe(void)
{
    lcd_saver_ctx_init(NULL);  /* must not crash */
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_NONE,
                      lcd_saver_poll(NULL, 0u, 0u, TIMEOUT_MS));
}

/* --- basic dim / wake cycle ---------------------------------------------- */

void test_stays_on_within_window(void)
{
    lcd_saver_ctx_t ctx;
    lcd_saver_ctx_init(&ctx);
    /* active at t=0; polls inside the window produce no transition. */
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_NONE, lcd_saver_poll(&ctx, 1u, 0u, TIMEOUT_MS));
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_NONE, lcd_saver_poll(&ctx, TIMEOUT_MS - 1u, 0u, TIMEOUT_MS));
    TEST_ASSERT_FALSE(ctx.backlight_off);
}

void test_dims_at_timeout(void)
{
    lcd_saver_ctx_t ctx;
    lcd_saver_ctx_init(&ctx);
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_OFF, lcd_saver_poll(&ctx, TIMEOUT_MS, 0u, TIMEOUT_MS));
    TEST_ASSERT_TRUE(ctx.backlight_off);
    /* Once off, further stale polls produce no further transition. */
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_NONE, lcd_saver_poll(&ctx, TIMEOUT_MS + 5000u, 0u, TIMEOUT_MS));
}

void test_wakes_when_activity_fresh_again(void)
{
    lcd_saver_ctx_t ctx;
    lcd_saver_ctx_init(&ctx);
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_OFF, lcd_saver_poll(&ctx, TIMEOUT_MS, 0u, TIMEOUT_MS));
    /* New activity at t=TIMEOUT_MS: poll with a fresh timestamp turns it on. */
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_ON,
                      lcd_saver_poll(&ctx, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS));
    TEST_ASSERT_FALSE(ctx.backlight_off);
}

/* --- never-dim (timeout == 0) -------------------------------------------- */

void test_timeout_zero_never_dims(void)
{
    lcd_saver_ctx_t ctx;
    lcd_saver_ctx_init(&ctx);
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_NONE, lcd_saver_poll(&ctx, 1000000u, 0u, 0u));
    TEST_ASSERT_FALSE(ctx.backlight_off);
}

void test_timeout_zero_wakes_if_off(void)
{
    lcd_saver_ctx_t ctx;
    lcd_saver_ctx_init(&ctx);
    ctx.backlight_off = true;  /* was dimmed under a previous finite timeout */
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_ON, lcd_saver_poll(&ctx, 0u, 0u, 0u));
    TEST_ASSERT_FALSE(ctx.backlight_off);
}

/* --- tick wrap-around ---------------------------------------------------- */

void test_tick_wraparound_dims(void)
{
    lcd_saver_ctx_t ctx;
    lcd_saver_ctx_init(&ctx);
    uint32_t active = 0xFFFFFFFFu - 1000u;   /* activity just before wrap */
    uint32_t now    = active + TIMEOUT_MS;   /* wraps past zero */
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_OFF, lcd_saver_poll(&ctx, now, active, TIMEOUT_MS));
}

/* --- regression: NFC/RFID result screen must not appear dark -------------- */

void test_regression_result_screen_restarts_timer(void)
{
    lcd_saver_ctx_t ctx;
    lcd_saver_ctx_init(&ctx);

    /* A long scan: no button press, so active_timestamp stays at 0 while the
     * tick advances past the timeout. The saver dims the backlight. */
    uint32_t scan_done = TIMEOUT_MS + 10000u;
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_OFF, lcd_saver_poll(&ctx, TIMEOUT_MS, 0u, TIMEOUT_MS));
    TEST_ASSERT_TRUE(ctx.backlight_off);  /* bug: result screen would be dark */

    /* Fix: on showing the read-complete info screen the firmware calls
     * m1_lcd_wake_restart_timer(), which reseats active_timestamp to "now"
     * and runs the saver. Model that by polling with active == now. */
    uint32_t woke_at = scan_done;
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_ON, lcd_saver_poll(&ctx, woke_at, woke_at, TIMEOUT_MS));
    TEST_ASSERT_FALSE(ctx.backlight_off);

    /* And it stays on for a full fresh window, not just the first tick. */
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_NONE,
                      lcd_saver_poll(&ctx, woke_at + TIMEOUT_MS - 1u, woke_at, TIMEOUT_MS));
    TEST_ASSERT_FALSE(ctx.backlight_off);

    /* Only after the fresh window elapses does it dim again. */
    TEST_ASSERT_EQUAL(LCD_SAVER_ACTION_OFF,
                      lcd_saver_poll(&ctx, woke_at + TIMEOUT_MS, woke_at, TIMEOUT_MS));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ctx_init_backlight_on);
    RUN_TEST(test_null_ctx_is_safe);
    RUN_TEST(test_stays_on_within_window);
    RUN_TEST(test_dims_at_timeout);
    RUN_TEST(test_wakes_when_activity_fresh_again);
    RUN_TEST(test_timeout_zero_never_dims);
    RUN_TEST(test_timeout_zero_wakes_if_off);
    RUN_TEST(test_tick_wraparound_dims);
    RUN_TEST(test_regression_result_screen_restarts_timer);
    return UNITY_END();
}
