/* See COPYING.txt for license details. */

/**
 * @file   test_clock.c
 * @brief  Host-side unit tests for m1_clock_util pure functions.
 *
 * Tests cover:
 *   - clock_is_leap_year()
 *   - clock_days_in_month()
 *   - clock_adjust_days()
 *   - clock_apply_offset()
 */

#include "unity.h"
#include "m1_clock_util.h"

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* clock_is_leap_year                                                         */
/*============================================================================*/

void test_leap_year_divisible_by_4(void)
{
    TEST_ASSERT_TRUE(clock_is_leap_year(2024));
    TEST_ASSERT_TRUE(clock_is_leap_year(2028));
}

void test_leap_year_century_not_leap(void)
{
    TEST_ASSERT_FALSE(clock_is_leap_year(1900));
    TEST_ASSERT_FALSE(clock_is_leap_year(2100));
}

void test_leap_year_400_is_leap(void)
{
    TEST_ASSERT_TRUE(clock_is_leap_year(2000));
    TEST_ASSERT_TRUE(clock_is_leap_year(2400));
}

void test_not_leap_year(void)
{
    TEST_ASSERT_FALSE(clock_is_leap_year(2023));
    TEST_ASSERT_FALSE(clock_is_leap_year(2025));
    TEST_ASSERT_FALSE(clock_is_leap_year(2026));
}

/*============================================================================*/
/* clock_days_in_month                                                        */
/*============================================================================*/

void test_days_in_month_jan(void)
{
    TEST_ASSERT_EQUAL_UINT8(31, clock_days_in_month(2026, 1));
}

void test_days_in_month_feb_normal(void)
{
    TEST_ASSERT_EQUAL_UINT8(28, clock_days_in_month(2026, 2));
}

void test_days_in_month_feb_leap(void)
{
    TEST_ASSERT_EQUAL_UINT8(29, clock_days_in_month(2024, 2));
}

void test_days_in_month_apr(void)
{
    TEST_ASSERT_EQUAL_UINT8(30, clock_days_in_month(2026, 4));
}

void test_days_in_month_dec(void)
{
    TEST_ASSERT_EQUAL_UINT8(31, clock_days_in_month(2026, 12));
}

void test_days_in_month_invalid_zero(void)
{
    /* Invalid month should return fallback 30 */
    TEST_ASSERT_EQUAL_UINT8(30, clock_days_in_month(2026, 0));
}

void test_days_in_month_invalid_13(void)
{
    TEST_ASSERT_EQUAL_UINT8(30, clock_days_in_month(2026, 13));
}

/*============================================================================*/
/* clock_adjust_days                                                          */
/*============================================================================*/

void test_adjust_days_forward_same_month(void)
{
    clock_time_t dt = { .year = 2026, .month = 4, .day = 10, .weekday = 5 };
    clock_adjust_days(&dt, 1);
    TEST_ASSERT_EQUAL_UINT8(11, dt.day);
    TEST_ASSERT_EQUAL_UINT8(4, dt.month);
    TEST_ASSERT_EQUAL_UINT8(6, dt.weekday);  /* Fri → Sat */
}

void test_adjust_days_forward_month_rollover(void)
{
    clock_time_t dt = { .year = 2026, .month = 1, .day = 31, .weekday = 6 };
    clock_adjust_days(&dt, 1);
    TEST_ASSERT_EQUAL_UINT8(1, dt.day);
    TEST_ASSERT_EQUAL_UINT8(2, dt.month);
    TEST_ASSERT_EQUAL_UINT8(7, dt.weekday);  /* Sat → Sun */
}

void test_adjust_days_forward_year_rollover(void)
{
    clock_time_t dt = { .year = 2026, .month = 12, .day = 31, .weekday = 4 };
    clock_adjust_days(&dt, 1);
    TEST_ASSERT_EQUAL_UINT8(1, dt.day);
    TEST_ASSERT_EQUAL_UINT8(1, dt.month);
    TEST_ASSERT_EQUAL_UINT16(2027, dt.year);
    TEST_ASSERT_EQUAL_UINT8(5, dt.weekday);  /* Thu → Fri */
}

void test_adjust_days_backward_same_month(void)
{
    clock_time_t dt = { .year = 2026, .month = 4, .day = 10, .weekday = 5 };
    clock_adjust_days(&dt, -1);
    TEST_ASSERT_EQUAL_UINT8(9, dt.day);
    TEST_ASSERT_EQUAL_UINT8(4, dt.month);
    TEST_ASSERT_EQUAL_UINT8(4, dt.weekday);  /* Fri → Thu */
}

void test_adjust_days_backward_month_rollover(void)
{
    clock_time_t dt = { .year = 2026, .month = 3, .day = 1, .weekday = 7 };
    clock_adjust_days(&dt, -1);
    TEST_ASSERT_EQUAL_UINT8(28, dt.day);  /* Feb 2026 = 28 days */
    TEST_ASSERT_EQUAL_UINT8(2, dt.month);
    TEST_ASSERT_EQUAL_UINT8(6, dt.weekday);  /* Sun → Sat */
}

void test_adjust_days_backward_year_rollover(void)
{
    clock_time_t dt = { .year = 2026, .month = 1, .day = 1, .weekday = 4 };
    clock_adjust_days(&dt, -1);
    TEST_ASSERT_EQUAL_UINT8(31, dt.day);
    TEST_ASSERT_EQUAL_UINT8(12, dt.month);
    TEST_ASSERT_EQUAL_UINT16(2025, dt.year);
    TEST_ASSERT_EQUAL_UINT8(3, dt.weekday);  /* Thu → Wed */
}

void test_adjust_days_weekday_wraps_forward(void)
{
    /* Sunday (7) + 1 day → Monday (1) */
    clock_time_t dt = { .year = 2026, .month = 4, .day = 5, .weekday = 7 };
    clock_adjust_days(&dt, 1);
    TEST_ASSERT_EQUAL_UINT8(1, dt.weekday);
}

void test_adjust_days_weekday_wraps_backward(void)
{
    /* Monday (1) - 1 day → Sunday (7) */
    clock_time_t dt = { .year = 2026, .month = 4, .day = 5, .weekday = 1 };
    clock_adjust_days(&dt, -1);
    TEST_ASSERT_EQUAL_UINT8(7, dt.weekday);
}

void test_adjust_days_null_safe(void)
{
    /* Should not crash */
    clock_adjust_days(NULL, 1);
}

void test_adjust_days_zero_noop(void)
{
    clock_time_t dt = { .year = 2026, .month = 4, .day = 10, .weekday = 5 };
    clock_adjust_days(&dt, 0);
    TEST_ASSERT_EQUAL_UINT8(10, dt.day);
}

/*============================================================================*/
/* clock_apply_offset                                                         */
/*============================================================================*/

void test_apply_offset_zero(void)
{
    clock_time_t src = { .year = 2026, .month = 4, .day = 10,
                         .hour = 15, .minute = 30, .second = 45, .weekday = 5 };
    clock_time_t dst;
    clock_apply_offset(&src, 0, &dst);
    TEST_ASSERT_EQUAL_UINT8(15, dst.hour);
    TEST_ASSERT_EQUAL_UINT8(10, dst.day);
}

void test_apply_offset_positive_no_rollover(void)
{
    clock_time_t src = { .year = 2026, .month = 4, .day = 10,
                         .hour = 10, .minute = 0, .second = 0, .weekday = 5 };
    clock_time_t dst;
    clock_apply_offset(&src, 5, &dst);
    TEST_ASSERT_EQUAL_UINT8(15, dst.hour);
    TEST_ASSERT_EQUAL_UINT8(10, dst.day);
}

void test_apply_offset_positive_day_rollover(void)
{
    clock_time_t src = { .year = 2026, .month = 4, .day = 10,
                         .hour = 22, .minute = 0, .second = 0, .weekday = 5 };
    clock_time_t dst;
    clock_apply_offset(&src, 5, &dst);
    TEST_ASSERT_EQUAL_UINT8(3, dst.hour);
    TEST_ASSERT_EQUAL_UINT8(11, dst.day);
    TEST_ASSERT_EQUAL_UINT8(6, dst.weekday);  /* Fri → Sat */
}

void test_apply_offset_negative_no_rollover(void)
{
    clock_time_t src = { .year = 2026, .month = 4, .day = 10,
                         .hour = 15, .minute = 0, .second = 0, .weekday = 5 };
    clock_time_t dst;
    clock_apply_offset(&src, -5, &dst);
    TEST_ASSERT_EQUAL_UINT8(10, dst.hour);
    TEST_ASSERT_EQUAL_UINT8(10, dst.day);
}

void test_apply_offset_negative_day_rollover(void)
{
    clock_time_t src = { .year = 2026, .month = 4, .day = 10,
                         .hour = 2, .minute = 0, .second = 0, .weekday = 5 };
    clock_time_t dst;
    clock_apply_offset(&src, -5, &dst);
    TEST_ASSERT_EQUAL_UINT8(21, dst.hour);
    TEST_ASSERT_EQUAL_UINT8(9, dst.day);
    TEST_ASSERT_EQUAL_UINT8(4, dst.weekday);  /* Fri → Thu */
}

void test_apply_offset_preserves_minutes_seconds(void)
{
    clock_time_t src = { .year = 2026, .month = 4, .day = 10,
                         .hour = 10, .minute = 42, .second = 17, .weekday = 5 };
    clock_time_t dst;
    clock_apply_offset(&src, 9, &dst);
    TEST_ASSERT_EQUAL_UINT8(19, dst.hour);
    TEST_ASSERT_EQUAL_UINT8(42, dst.minute);
    TEST_ASSERT_EQUAL_UINT8(17, dst.second);
}

void test_apply_offset_null_safe(void)
{
    clock_time_t src = { 0 };
    clock_time_t dst;
    /* Should not crash */
    clock_apply_offset(NULL, 5, &dst);
    clock_apply_offset(&src, 5, NULL);
}

void test_apply_offset_midnight_boundary(void)
{
    /* Exactly midnight → offset should give exact hour */
    clock_time_t src = { .year = 2026, .month = 4, .day = 10,
                         .hour = 0, .minute = 0, .second = 0, .weekday = 5 };
    clock_time_t dst;
    clock_apply_offset(&src, -1, &dst);
    TEST_ASSERT_EQUAL_UINT8(23, dst.hour);
    TEST_ASSERT_EQUAL_UINT8(9, dst.day);
}

void test_apply_offset_feb_leap_year_rollover(void)
{
    /* Feb 28 leap year + offset that crosses midnight */
    clock_time_t src = { .year = 2024, .month = 2, .day = 28,
                         .hour = 23, .minute = 0, .second = 0, .weekday = 3 };
    clock_time_t dst;
    clock_apply_offset(&src, 5, &dst);
    TEST_ASSERT_EQUAL_UINT8(4, dst.hour);
    TEST_ASSERT_EQUAL_UINT8(29, dst.day);   /* Feb 29 in leap year */
    TEST_ASSERT_EQUAL_UINT8(2, dst.month);
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Leap year */
    RUN_TEST(test_leap_year_divisible_by_4);
    RUN_TEST(test_leap_year_century_not_leap);
    RUN_TEST(test_leap_year_400_is_leap);
    RUN_TEST(test_not_leap_year);

    /* Days in month */
    RUN_TEST(test_days_in_month_jan);
    RUN_TEST(test_days_in_month_feb_normal);
    RUN_TEST(test_days_in_month_feb_leap);
    RUN_TEST(test_days_in_month_apr);
    RUN_TEST(test_days_in_month_dec);
    RUN_TEST(test_days_in_month_invalid_zero);
    RUN_TEST(test_days_in_month_invalid_13);

    /* Adjust days */
    RUN_TEST(test_adjust_days_forward_same_month);
    RUN_TEST(test_adjust_days_forward_month_rollover);
    RUN_TEST(test_adjust_days_forward_year_rollover);
    RUN_TEST(test_adjust_days_backward_same_month);
    RUN_TEST(test_adjust_days_backward_month_rollover);
    RUN_TEST(test_adjust_days_backward_year_rollover);
    RUN_TEST(test_adjust_days_weekday_wraps_forward);
    RUN_TEST(test_adjust_days_weekday_wraps_backward);
    RUN_TEST(test_adjust_days_null_safe);
    RUN_TEST(test_adjust_days_zero_noop);

    /* Apply offset */
    RUN_TEST(test_apply_offset_zero);
    RUN_TEST(test_apply_offset_positive_no_rollover);
    RUN_TEST(test_apply_offset_positive_day_rollover);
    RUN_TEST(test_apply_offset_negative_no_rollover);
    RUN_TEST(test_apply_offset_negative_day_rollover);
    RUN_TEST(test_apply_offset_preserves_minutes_seconds);
    RUN_TEST(test_apply_offset_null_safe);
    RUN_TEST(test_apply_offset_midnight_boundary);
    RUN_TEST(test_apply_offset_feb_leap_year_rollover);

    return UNITY_END();
}
