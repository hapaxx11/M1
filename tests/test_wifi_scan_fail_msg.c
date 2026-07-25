/* See COPYING.txt for license details. */

/**
 * test_wifi_scan_fail_msg.c
 *
 * Regression tests for the WiFi access-point scan-failure screen wording
 * (wifi_scan_fail_msg.h).  The screen previously read the broken-English
 * "Scan AP" / "Failed. Let retry!".  It now reads the grammatical
 * "AP scan failed." / "Please try again.".
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "wifi_scan_fail_msg.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* The two rows must read the corrected, grammatical wording. */
static void test_scan_fail_lines_are_grammatical(void)
{
    TEST_ASSERT_EQUAL_STRING("AP scan failed.", M1_WIFI_SCAN_FAIL_LINE_1);
    TEST_ASSERT_EQUAL_STRING("Please try again.", M1_WIFI_SCAN_FAIL_LINE_2);
}

/* The broken-English fragments must be gone. */
static void test_scan_fail_lines_have_no_broken_english(void)
{
    TEST_ASSERT_NULL(strstr(M1_WIFI_SCAN_FAIL_LINE_1, "Let retry"));
    TEST_ASSERT_NULL(strstr(M1_WIFI_SCAN_FAIL_LINE_2, "Let retry"));
}

/* Main-menu font renders ~21 chars on the 128px display. */
static void test_scan_fail_lines_fit_display(void)
{
    TEST_ASSERT_LESS_OR_EQUAL_UINT(21u, (unsigned)strlen(M1_WIFI_SCAN_FAIL_LINE_1));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(21u, (unsigned)strlen(M1_WIFI_SCAN_FAIL_LINE_2));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_scan_fail_lines_are_grammatical);
    RUN_TEST(test_scan_fail_lines_have_no_broken_english);
    RUN_TEST(test_scan_fail_lines_fit_display);
    return UNITY_END();
}
