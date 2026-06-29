/* See COPYING.txt for license details. */

/**
 * test_wifi_ntp_parse.c
 *
 * Unit tests for wifi_ntp_parse.h / wifi_ntp_parse.c:
 *   wifi_ntp_parse_time() — parse ESP-AT +CIPSNTPTIME response
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "wifi_ntp_parse.h"
#include <string.h>
#include <stdio.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * Basic parsing
 * =========================================================================*/

void test_parse_typical_response(void)
{
    clock_time_t t;
    const char *resp = "+CIPSNTPTIME:Thu Jun 29 01:43:16 2026\r\nOK\r\n";
    TEST_ASSERT_TRUE(wifi_ntp_parse_time(resp, &t));
    TEST_ASSERT_EQUAL_UINT16(2026, t.year);
    TEST_ASSERT_EQUAL_UINT8(6, t.month);
    TEST_ASSERT_EQUAL_UINT8(29, t.day);
    TEST_ASSERT_EQUAL_UINT8(1, t.hour);
    TEST_ASSERT_EQUAL_UINT8(43, t.minute);
    TEST_ASSERT_EQUAL_UINT8(16, t.second);
    TEST_ASSERT_EQUAL_UINT8(4, t.weekday);  /* Thursday */
}

void test_parse_monday(void)
{
    clock_time_t t;
    TEST_ASSERT_TRUE(wifi_ntp_parse_time("+CIPSNTPTIME:Mon Jan  5 00:00:00 2026", &t));
    TEST_ASSERT_EQUAL_UINT8(1, t.weekday);
    TEST_ASSERT_EQUAL_UINT8(1, t.month);
    TEST_ASSERT_EQUAL_UINT8(5, t.day);
}

void test_parse_sunday(void)
{
    clock_time_t t;
    TEST_ASSERT_TRUE(wifi_ntp_parse_time("+CIPSNTPTIME:Sun Dec 31 23:59:59 2028", &t));
    TEST_ASSERT_EQUAL_UINT8(7, t.weekday);
    TEST_ASSERT_EQUAL_UINT8(12, t.month);
    TEST_ASSERT_EQUAL_UINT8(31, t.day);
    TEST_ASSERT_EQUAL_UINT8(23, t.hour);
    TEST_ASSERT_EQUAL_UINT8(59, t.minute);
    TEST_ASSERT_EQUAL_UINT8(59, t.second);
}

void test_parse_without_prefix(void)
{
    clock_time_t t;
    TEST_ASSERT_TRUE(wifi_ntp_parse_time("Wed Mar 15 12:30:45 2025", &t));
    TEST_ASSERT_EQUAL_UINT16(2025, t.year);
    TEST_ASSERT_EQUAL_UINT8(3, t.month);
    TEST_ASSERT_EQUAL_UINT8(3, t.weekday);  /* Wednesday */
}

void test_parse_all_months(void)
{
    clock_time_t t;
    const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                            "Jul","Aug","Sep","Oct","Nov","Dec"};
    char buf[64];
    for (uint8_t i = 0; i < 12; i++)
    {
        snprintf(buf, sizeof(buf), "Mon %s 15 12:00:00 2025", months[i]);
        TEST_ASSERT_TRUE(wifi_ntp_parse_time(buf, &t));
        TEST_ASSERT_EQUAL_UINT8(i + 1, t.month);
    }
}

/* =========================================================================
 * Rejection cases
 * =========================================================================*/

void test_reject_epoch_1970(void)
{
    clock_time_t t;
    TEST_ASSERT_FALSE(wifi_ntp_parse_time("+CIPSNTPTIME:Thu Jan  1 00:00:00 1970", &t));
}

void test_reject_null_response(void)
{
    clock_time_t t;
    TEST_ASSERT_FALSE(wifi_ntp_parse_time(NULL, &t));
}

void test_reject_null_output(void)
{
    TEST_ASSERT_FALSE(wifi_ntp_parse_time("+CIPSNTPTIME:Thu Jun 29 01:43:16 2026", NULL));
}

void test_reject_garbage(void)
{
    clock_time_t t;
    TEST_ASSERT_FALSE(wifi_ntp_parse_time("ERROR\r\n", &t));
}

void test_reject_invalid_month(void)
{
    clock_time_t t;
    TEST_ASSERT_FALSE(wifi_ntp_parse_time("Mon Foo 15 12:00:00 2025", &t));
}

void test_reject_invalid_day_name(void)
{
    clock_time_t t;
    TEST_ASSERT_FALSE(wifi_ntp_parse_time("Xyz Jan 15 12:00:00 2025", &t));
}

void test_reject_year_out_of_range(void)
{
    clock_time_t t;
    TEST_ASSERT_FALSE(wifi_ntp_parse_time("Mon Jan 15 12:00:00 1999", &t));
}

void test_reject_hour_out_of_range(void)
{
    clock_time_t t;
    TEST_ASSERT_FALSE(wifi_ntp_parse_time("Mon Jan 15 25:00:00 2025", &t));
}

/* =========================================================================
 * Edge cases
 * =========================================================================*/

void test_parse_embedded_in_larger_response(void)
{
    clock_time_t t;
    const char *resp =
        "AT+CIPSNTPTIME?\r\n"
        "+CIPSNTPTIME:Sat Feb  1 08:15:30 2025\r\n"
        "\r\nOK\r\n";
    TEST_ASSERT_TRUE(wifi_ntp_parse_time(resp, &t));
    TEST_ASSERT_EQUAL_UINT16(2025, t.year);
    TEST_ASSERT_EQUAL_UINT8(2, t.month);
    TEST_ASSERT_EQUAL_UINT8(6, t.weekday);  /* Saturday */
}

/* =========================================================================
 * Test runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_parse_typical_response);
    RUN_TEST(test_parse_monday);
    RUN_TEST(test_parse_sunday);
    RUN_TEST(test_parse_without_prefix);
    RUN_TEST(test_parse_all_months);

    RUN_TEST(test_reject_epoch_1970);
    RUN_TEST(test_reject_null_response);
    RUN_TEST(test_reject_null_output);
    RUN_TEST(test_reject_garbage);
    RUN_TEST(test_reject_invalid_month);
    RUN_TEST(test_reject_invalid_day_name);
    RUN_TEST(test_reject_year_out_of_range);
    RUN_TEST(test_reject_hour_out_of_range);

    RUN_TEST(test_parse_embedded_in_larger_response);

    return UNITY_END();
}
