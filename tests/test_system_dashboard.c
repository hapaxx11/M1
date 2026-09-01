/* See COPYING.txt for license details. */

/**
 * @file   test_system_dashboard.c
 * @brief  Host-side unit tests for pure-logic helpers in m1_system_dashboard.c.
 *
 * Tests dashboard_format_uptime() and dashboard_sd_status_text() by compiling
 * the shared m1_system_dashboard_helpers.c module directly.
 */

#include "unity.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "m1_sdcard.h"
#include "m1_system_dashboard_helpers.h"

/*============================================================================*/
/* Tests                                                                      */
/*============================================================================*/

void setUp(void) { }
void tearDown(void) { }

/* --- dashboard_format_uptime tests --- */

void test_uptime_zero(void)
{
    char buf[32];
    dashboard_format_uptime(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00:00:00", buf);
}

void test_uptime_one_second(void)
{
    char buf[32];
    dashboard_format_uptime(1000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00:00:01", buf);
}

void test_uptime_one_minute(void)
{
    char buf[32];
    dashboard_format_uptime(60 * 1000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00:01:00", buf);
}

void test_uptime_one_hour(void)
{
    char buf[32];
    dashboard_format_uptime(3600U * 1000U, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("01:00:00", buf);
}

void test_uptime_99_hours(void)
{
    char buf[32];
    /* 99h 59m 59s — still uses HH:MM:SS format */
    dashboard_format_uptime((99U * 3600U + 59U * 60U + 59U) * 1000U, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("99:59:59", buf);
}

void test_uptime_100_hours_switches_format(void)
{
    char buf[32];
    /* 100h 0m — switches to "NNNh NNm" format */
    dashboard_format_uptime(100U * 3600U * 1000U, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("100h 0m", buf);
}

void test_uptime_large(void)
{
    char buf[32];
    /* 123h 45m */
    dashboard_format_uptime((123U * 3600U + 45U * 60U) * 1000U, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("123h 45m", buf);
}

void test_uptime_mixed(void)
{
    char buf[32];
    /* 2h 30m 15s = 9015s */
    dashboard_format_uptime(9015U * 1000U, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("02:30:15", buf);
}

/* --- dashboard_sd_status_text tests --- */

void test_sd_status_ok(void)
{
    TEST_ASSERT_EQUAL_STRING("Ready", dashboard_sd_status_text(SD_access_OK));
}

void test_sd_status_no_fs(void)
{
    TEST_ASSERT_EQUAL_STRING("No FS", dashboard_sd_status_text(SD_access_NoFS));
}

void test_sd_status_unmounted(void)
{
    TEST_ASSERT_EQUAL_STRING("Unmounted", dashboard_sd_status_text(SD_access_UnMounted));
}

void test_sd_status_not_ready(void)
{
    TEST_ASSERT_EQUAL_STRING("No Card", dashboard_sd_status_text(SD_access_NotReady));
}

void test_sd_status_not_ok(void)
{
    TEST_ASSERT_EQUAL_STRING("Error", dashboard_sd_status_text(SD_access_NotOK));
}

void test_sd_status_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("Error", dashboard_sd_status_text(SD_access_EndOfStatus));
}

/* --- dashboard_split_rpc_wallclock_suffix tests (issue #719 Phase 7) --- */

/* Regression guard: the "no-reply" RPC diag line's trailing " tNs" wall-
 * clock suffix used to be drawn in-line on the dashboard and ran off the
 * right edge of the 128px display, making it unreadable. The line must be
 * split so the base and the suffix each fit on their own line. */
void test_split_wallclock_suffix_splits_no_reply_line(void)
{
    char base[40], suffix[16];
    dashboard_split_rpc_wallclock_suffix("op0103 no-reply st253 r0 p0 t10s",
                                         base, sizeof(base),
                                         suffix, sizeof(suffix));
    TEST_ASSERT_EQUAL_STRING("op0103 no-reply st253 r0 p0", base);
    TEST_ASSERT_EQUAL_STRING("t10s", suffix);
}

void test_split_wallclock_suffix_splits_short_elapsed(void)
{
    char base[40], suffix[16];
    dashboard_split_rpc_wallclock_suffix("op0103 no-reply st253 r0 p0 t1s",
                                         base, sizeof(base),
                                         suffix, sizeof(suffix));
    TEST_ASSERT_EQUAL_STRING("op0103 no-reply st253 r0 p0", base);
    TEST_ASSERT_EQUAL_STRING("t1s", suffix);
}

void test_split_wallclock_suffix_leaves_line_without_suffix_untouched(void)
{
    char base[40], suffix[16];
    dashboard_split_rpc_wallclock_suffix("op0103 ok st0 r512 p24",
                                         base, sizeof(base),
                                         suffix, sizeof(suffix));
    TEST_ASSERT_EQUAL_STRING("op0103 ok st0 r512 p24", base);
    TEST_ASSERT_EQUAL_STRING("", suffix);
}

void test_split_wallclock_suffix_no_call_yet_untouched(void)
{
    char base[40], suffix[16];
    dashboard_split_rpc_wallclock_suffix("no call yet",
                                         base, sizeof(base),
                                         suffix, sizeof(suffix));
    TEST_ASSERT_EQUAL_STRING("no call yet", base);
    TEST_ASSERT_EQUAL_STRING("", suffix);
}

void test_split_wallclock_suffix_null_line_is_safe(void)
{
    char base[40], suffix[16];
    strcpy(base, "unset");
    strcpy(suffix, "unset");
    dashboard_split_rpc_wallclock_suffix(NULL, base, sizeof(base),
                                         suffix, sizeof(suffix));
    TEST_ASSERT_EQUAL_STRING("", base);
    TEST_ASSERT_EQUAL_STRING("", suffix);
}

void test_split_wallclock_suffix_null_suffix_out_is_safe(void)
{
    char base[40];
    dashboard_split_rpc_wallclock_suffix("op0103 no-reply st253 r0 p0 t10s",
                                         base, sizeof(base), NULL, 0);
    TEST_ASSERT_EQUAL_STRING("op0103 no-reply st253 r0 p0", base);
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Uptime formatting */
    RUN_TEST(test_uptime_zero);
    RUN_TEST(test_uptime_one_second);
    RUN_TEST(test_uptime_one_minute);
    RUN_TEST(test_uptime_one_hour);
    RUN_TEST(test_uptime_99_hours);
    RUN_TEST(test_uptime_100_hours_switches_format);
    RUN_TEST(test_uptime_large);
    RUN_TEST(test_uptime_mixed);

    /* SD status text */
    RUN_TEST(test_sd_status_ok);
    RUN_TEST(test_sd_status_no_fs);
    RUN_TEST(test_sd_status_unmounted);
    RUN_TEST(test_sd_status_not_ready);
    RUN_TEST(test_sd_status_not_ok);
    RUN_TEST(test_sd_status_unknown);

    /* RPC diagnostic line wall-clock suffix splitting */
    RUN_TEST(test_split_wallclock_suffix_splits_no_reply_line);
    RUN_TEST(test_split_wallclock_suffix_splits_short_elapsed);
    RUN_TEST(test_split_wallclock_suffix_leaves_line_without_suffix_untouched);
    RUN_TEST(test_split_wallclock_suffix_no_call_yet_untouched);
    RUN_TEST(test_split_wallclock_suffix_null_line_is_safe);
    RUN_TEST(test_split_wallclock_suffix_null_suffix_out_is_safe);

    return UNITY_END();
}
