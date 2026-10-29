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

void test_short_suspend_resume_cycles_preserve_accumulated_runtime(void)
{
    S_M1_WDT_Report report = {
        .report_id = M1_REPORT_ID_RPC_TASK,
        .inactive = false,
        .run_time = 7900u,
    };

    for (uint32_t i = 0u; i < 20u; i++)
    {
        m1_wdt_set_task_inactive(&report, true);
        m1_wdt_set_task_inactive(&report, false);
    }

    TEST_ASSERT_EQUAL_UINT32(7900u, report.run_time);
    TEST_ASSERT_FALSE(report.inactive);
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_OK,
        m1_wdt_decide_action(report.run_time, 2000u, 20000u,
                             report.inactive));
}

void test_wall_time_accounting_handles_fast_and_slow_iterations(void)
{
    uint32_t last_report = 100u;
    uint32_t run_time = 0u;

    for (uint32_t i = 0u; i < 1000u; i++)
        TEST_ASSERT_FALSE(m1_wdt_heartbeat_due(last_report, last_report));

    for (uint32_t i = 0u; i < 240u; i++)
    {
        uint32_t now = last_report + 33u;
        TEST_ASSERT_TRUE(m1_wdt_heartbeat_due(now, last_report));
        run_time += now - last_report;
        last_report = now;
    }
    TEST_ASSERT_EQUAL_UINT32(7920u, run_time);

    uint32_t slow_now = last_report + 250u;
    run_time += slow_now - last_report;
    TEST_ASSERT_EQUAL_UINT32(8170u, run_time);
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_OK,
        m1_wdt_decide_action(run_time, 2000u, 20000u, false));
}

void test_resume_grace_allows_a_full_window_after_a_long_probe(void)
{
    S_M1_WDT_Report report = {
        .report_id = M1_REPORT_ID_RPC_TASK,
        .inactive = true,
        .run_time = 0u,
    };

    m1_wdt_set_task_inactive(&report, false);
    report.run_time += 100u;
    TEST_ASSERT_EQUAL(M1_WDT_ACTION_SUSPENDED,
        m1_wdt_decide_action_with_grace(report.run_time, 2000u, 20000u,
                                        report.inactive, report.resume_grace));

    report.run_time = 0u;
    report.resume_grace = false;
    for (uint32_t i = 0u; i < 80u; i++)
        report.run_time += 100u;

    TEST_ASSERT_EQUAL(M1_WDT_ACTION_OK,
        m1_wdt_decide_action_with_grace(report.run_time, 2000u, 20000u,
                                        report.inactive, report.resume_grace));
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
    RUN_TEST(test_short_suspend_resume_cycles_preserve_accumulated_runtime);
    RUN_TEST(test_wall_time_accounting_handles_fast_and_slow_iterations);
    RUN_TEST(test_resume_grace_allows_a_full_window_after_a_long_probe);
    return UNITY_END();
}
