/* See COPYING.txt for license details. */

/*
 * test_wifi_ap_cycle.c
 *
 * Unit tests for the pure AP-cycling helpers (wifi_ap_cycle.[ch]) used by the
 * WiFi cleanup Phase 3 selected-network Target context ("Cycle APs").
 */

#include "unity.h"

#include <string.h>

#include "wifi_ap_record.h"
#include "wifi_ap_cycle.h"

void setUp(void) {}
void tearDown(void) {}

/* Build a minimal AP record with just an SSID and a distinguishing BSSID. */
static wifi_ap_t make_ap(const char *ssid, uint8_t bssid_last)
{
    wifi_ap_t ap;
    memset(&ap, 0, sizeof(ap));
    strncpy(ap.ssid, ssid, sizeof(ap.ssid) - 1);
    ap.bssid[5] = bssid_last;
    return ap;
}

/*--------------------------------------------------------------------------*/
/* wifi_ap_ssid_count                                                       */
/*--------------------------------------------------------------------------*/

void test_ssid_count_single_entry(void)
{
    wifi_ap_t list[1] = { make_ap("HomeNet", 0x01) };
    TEST_ASSERT_EQUAL_UINT16(1, wifi_ap_ssid_count(list, 1, 0));
}

void test_ssid_count_multiple_bssids(void)
{
    wifi_ap_t list[3] = {
        make_ap("HomeNet", 0x01),
        make_ap("OtherNet", 0x02),
        make_ap("HomeNet", 0x03),
    };
    /* Two entries share "HomeNet". */
    TEST_ASSERT_EQUAL_UINT16(2, wifi_ap_ssid_count(list, 3, 0));
    TEST_ASSERT_EQUAL_UINT16(2, wifi_ap_ssid_count(list, 3, 2));
    /* "OtherNet" is unique. */
    TEST_ASSERT_EQUAL_UINT16(1, wifi_ap_ssid_count(list, 3, 1));
}

void test_ssid_count_invalid_inputs(void)
{
    wifi_ap_t list[1] = { make_ap("HomeNet", 0x01) };
    TEST_ASSERT_EQUAL_UINT16(0, wifi_ap_ssid_count(NULL, 1, 0));
    TEST_ASSERT_EQUAL_UINT16(0, wifi_ap_ssid_count(list, 1, 5));  /* cur out of range */
    TEST_ASSERT_EQUAL_UINT16(0, wifi_ap_ssid_count(list, 0, 0));  /* count 0 */
}

/*--------------------------------------------------------------------------*/
/* wifi_ap_cycle_next                                                       */
/*--------------------------------------------------------------------------*/

void test_cycle_single_ap_stays_put(void)
{
    wifi_ap_t list[1] = { make_ap("HomeNet", 0x01) };
    TEST_ASSERT_EQUAL_UINT16(0, wifi_ap_cycle_next(list, 1, 0));
}

void test_cycle_two_bssids_toggle(void)
{
    wifi_ap_t list[2] = {
        make_ap("HomeNet", 0x01),
        make_ap("HomeNet", 0x02),
    };
    TEST_ASSERT_EQUAL_UINT16(1, wifi_ap_cycle_next(list, 2, 0));
    TEST_ASSERT_EQUAL_UINT16(0, wifi_ap_cycle_next(list, 2, 1));  /* wraps back */
}

void test_cycle_skips_other_ssids_and_wraps(void)
{
    wifi_ap_t list[4] = {
        make_ap("HomeNet", 0x01),  /* 0 */
        make_ap("OtherA",  0x02),  /* 1 */
        make_ap("HomeNet", 0x03),  /* 2 */
        make_ap("OtherB",  0x04),  /* 3 */
    };
    /* From 0, the next HomeNet is index 2 (skips OtherA). */
    TEST_ASSERT_EQUAL_UINT16(2, wifi_ap_cycle_next(list, 4, 0));
    /* From 2, forward search wraps past OtherB back to index 0. */
    TEST_ASSERT_EQUAL_UINT16(0, wifi_ap_cycle_next(list, 4, 2));
}

void test_cycle_unique_ssid_stays_put(void)
{
    wifi_ap_t list[3] = {
        make_ap("HomeNet", 0x01),
        make_ap("OtherA",  0x02),
        make_ap("HomeNet", 0x03),
    };
    /* "OtherA" has no sibling → cur is returned unchanged. */
    TEST_ASSERT_EQUAL_UINT16(1, wifi_ap_cycle_next(list, 3, 1));
}

void test_cycle_invalid_inputs(void)
{
    wifi_ap_t list[2] = {
        make_ap("HomeNet", 0x01),
        make_ap("HomeNet", 0x02),
    };
    TEST_ASSERT_EQUAL_UINT16(0, wifi_ap_cycle_next(NULL, 2, 0));
    TEST_ASSERT_EQUAL_UINT16(9, wifi_ap_cycle_next(list, 2, 9));  /* cur out of range */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ssid_count_single_entry);
    RUN_TEST(test_ssid_count_multiple_bssids);
    RUN_TEST(test_ssid_count_invalid_inputs);
    RUN_TEST(test_cycle_single_ap_stays_put);
    RUN_TEST(test_cycle_two_bssids_toggle);
    RUN_TEST(test_cycle_skips_other_ssids_and_wraps);
    RUN_TEST(test_cycle_unique_ssid_stays_put);
    RUN_TEST(test_cycle_invalid_inputs);
    return UNITY_END();
}
