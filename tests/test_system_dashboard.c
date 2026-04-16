/* See COPYING.txt for license details. */

/**
 * @file   test_system_dashboard.c
 * @brief  Host-side unit tests for pure-logic helpers in m1_system_dashboard.c.
 *
 * Tests dashboard_format_uptime() and dashboard_sd_status_text() via
 * standalone copies (the production functions are static).
 */

#include "unity.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "m1_sdcard.h"

/*============================================================================*/
/* Standalone copies of static functions from m1_system_dashboard.c           */
/*============================================================================*/

static void dashboard_format_uptime(uint32_t uptime_ms, char *out, size_t out_len)
{
    uint32_t total_sec = uptime_ms / 1000U;
    uint32_t hours = total_sec / 3600U;
    uint32_t minutes = (total_sec / 60U) % 60U;
    uint32_t seconds = total_sec % 60U;

    if (hours >= 100U)
    {
        snprintf(out, out_len, "%luh %lum",
                 (unsigned long)hours, (unsigned long)minutes);
    }
    else
    {
        snprintf(out, out_len, "%02lu:%02lu:%02lu",
                 (unsigned long)hours,
                 (unsigned long)minutes,
                 (unsigned long)seconds);
    }
}

static const char *dashboard_sd_status_text(S_M1_SDCard_Access_Status status)
{
    switch (status)
    {
        case SD_access_OK:
            return "Ready";
        case SD_access_NoFS:
            return "No FS";
        case SD_access_UnMounted:
            return "Unmounted";
        case SD_access_NotReady:
            return "No Card";
        case SD_access_NotOK:
        default:
            return "Error";
    }
}

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

    return UNITY_END();
}
