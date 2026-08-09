#include "unity.h"
#include "m1_watchdog_decision.h"

void setUp(void) {}
void tearDown(void) {}

void test_run_time_within_bounds_is_ok(void)
{
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_OK,
        m1_wdt_decide_action(50u, 10u, 100u, false));
}

void test_run_time_below_min_and_inactive_is_suspended(void)
{
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_SUSPENDED,
        m1_wdt_decide_action(0u, 10u, 100u, true));
}

void test_run_time_below_min_and_active_is_failure(void)
{
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_FAILURE,
        m1_wdt_decide_action(0u, 10u, 100u, false));
}

void test_run_time_above_max_and_active_is_failure(void)
{
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_FAILURE,
        m1_wdt_decide_action(500u, 10u, 100u, false));
}

void test_run_time_above_max_but_inactive_is_suspended(void)
{
    /* Regression guard: the "inactive" (intentionally-suspended task) branch
     * must still take priority over a plain out-of-range failure so a
     * deliberately-parked task is never reported as a WDT failure. */
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_SUSPENDED,
        m1_wdt_decide_action(500u, 10u, 100u, true));
}

void test_run_time_exactly_at_bounds_is_ok(void)
{
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_OK,
        m1_wdt_decide_action(10u, 10u, 100u, false));
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_OK,
        m1_wdt_decide_action(100u, 10u, 100u, false));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_run_time_within_bounds_is_ok);
    RUN_TEST(test_run_time_below_min_and_inactive_is_suspended);
    RUN_TEST(test_run_time_below_min_and_active_is_failure);
    RUN_TEST(test_run_time_above_max_and_active_is_failure);
    RUN_TEST(test_run_time_above_max_but_inactive_is_suspended);
    RUN_TEST(test_run_time_exactly_at_bounds_is_ok);
    return UNITY_END();
}
