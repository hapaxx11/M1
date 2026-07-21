/* See COPYING.txt for license details. */

/*
 * test_esp32_idle.c
 *
 * Host-side unit tests for the ESP32-C6 idle auto power-off state machine
 * (m1_csrc/esp32_idle.c).  Pure logic, no HAL/RTOS dependencies.
 */

#include "unity.h"
#include "esp32_idle.h"

void setUp(void) { }
void tearDown(void) { }

/* --- init ---------------------------------------------------------------- */

void test_ctx_init_sets_timeout_and_clears_window(void)
{
    esp32_idle_ctx_t ctx;
    esp32_idle_ctx_init(&ctx, ESP32_IDLE_POWER_OFF_MS);
    TEST_ASSERT_EQUAL_UINT32(ESP32_IDLE_POWER_OFF_MS, ctx.timeout_ms);
    TEST_ASSERT_FALSE(ctx.idle_active);
}

void test_ctx_init_null_is_safe(void)
{
    esp32_idle_ctx_init(NULL, 1000u);   /* must not crash */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE,
                      esp32_idle_poll(NULL, true, false, 0u));
}

/* --- not powered / busy never powers off --------------------------------- */

void test_not_powered_never_powers_off(void)
{
    esp32_idle_ctx_t ctx;
    esp32_idle_ctx_init(&ctx, 1000u);
    /* Powered off the whole time: no action even far past the window. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, false, false, 0u));
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, false, false, 5000u));
    TEST_ASSERT_FALSE(ctx.idle_active);
}

void test_busy_resets_idle_window(void)
{
    esp32_idle_ctx_t ctx;
    esp32_idle_ctx_init(&ctx, 1000u);
    /* Open the window while idle... */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, false, 0u));
    TEST_ASSERT_TRUE(ctx.idle_active);
    /* ...then go busy: window must close. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, true, 500u));
    TEST_ASSERT_FALSE(ctx.idle_active);
    /* Even long after, no power-off because busy kept resetting it. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, true, 100000u));
}

/* --- normal power-off after timeout -------------------------------------- */

void test_power_off_after_timeout(void)
{
    esp32_idle_ctx_t ctx;
    esp32_idle_ctx_init(&ctx, 1000u);
    /* First idle poll opens the window at t=0. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, false, 0u));
    /* Just before the window: still nothing. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, false, 999u));
    /* At the window boundary: power off. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_POWER_OFF, esp32_idle_poll(&ctx, true, false, 1000u));
}

void test_power_off_fires_only_once(void)
{
    esp32_idle_ctx_t ctx;
    esp32_idle_ctx_init(&ctx, 1000u);
    esp32_idle_poll(&ctx, true, false, 0u);
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_POWER_OFF, esp32_idle_poll(&ctx, true, false, 2000u));
    /* If the caller has not yet dropped power, we must not spam POWER_OFF:
     * the window was consumed, so the next poll re-opens it instead. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, false, 2001u));
    TEST_ASSERT_TRUE(ctx.idle_active);
}

void test_reuse_resets_window(void)
{
    esp32_idle_ctx_t ctx;
    esp32_idle_ctx_init(&ctx, 1000u);
    esp32_idle_poll(&ctx, true, false, 0u);      /* open window   */
    esp32_idle_poll(&ctx, true, true, 500u);     /* used again    */
    /* New idle window opens at 600, must not power off until 1600. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, false, 600u));
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, false, 1500u));
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_POWER_OFF, esp32_idle_poll(&ctx, true, false, 1600u));
}

/* --- tick wrap-around ---------------------------------------------------- */

void test_tick_wraparound_is_handled(void)
{
    esp32_idle_ctx_t ctx;
    esp32_idle_ctx_init(&ctx, 1000u);
    /* Open the window close enough to the 32-bit tick max to force wrap. */
    uint32_t t0 = 0xFFFFFF00u;   /* 256 before wrap */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, true, false, t0));
    /* Wraps past zero; elapsed = 1000 exactly -> power off. */
    uint32_t t1 = (uint32_t)(t0 + 1000u);
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_POWER_OFF, esp32_idle_poll(&ctx, true, false, t1));
}

/* --- after power-off, unpowered poll clears window ------------------------ */

void test_after_power_off_unpowered_poll_is_none(void)
{
    esp32_idle_ctx_t ctx;
    esp32_idle_ctx_init(&ctx, 1000u);
    esp32_idle_poll(&ctx, true, false, 0u);
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_POWER_OFF, esp32_idle_poll(&ctx, true, false, 1000u));
    /* Caller drops power: subsequent unpowered polls are quiet. */
    TEST_ASSERT_EQUAL(ESP32_IDLE_ACTION_NONE, esp32_idle_poll(&ctx, false, false, 1001u));
    TEST_ASSERT_FALSE(ctx.idle_active);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ctx_init_sets_timeout_and_clears_window);
    RUN_TEST(test_ctx_init_null_is_safe);
    RUN_TEST(test_not_powered_never_powers_off);
    RUN_TEST(test_busy_resets_idle_window);
    RUN_TEST(test_power_off_after_timeout);
    RUN_TEST(test_power_off_fires_only_once);
    RUN_TEST(test_reuse_resets_window);
    RUN_TEST(test_tick_wraparound_is_handled);
    RUN_TEST(test_after_power_off_unpowered_poll_is_none);
    return UNITY_END();
}
