/* See COPYING.txt for license details. */

/*
 * test_wifi_multi_target.c
 *
 * Pure-logic unit tests for wifi_multi_target_build() (Phase 4 of #680).
 *
 * wifi_multi_target_build() builds a flat target list from the selected
 * entries of an AP list — no HAL or RTOS dependencies.
 */

#include "unity.h"
#include "wifi_multi_target.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*--------------------------------------------------------------------------*/
/* Helpers                                                                  */
/*--------------------------------------------------------------------------*/

static wifi_ap_t make_ap(const char *ssid, bool selected)
{
    wifi_ap_t ap;
    memset(&ap, 0, sizeof(ap));
    strncpy(ap.ssid, ssid, sizeof(ap.ssid) - 1);
    ap.selected = selected;
    return ap;
}

/*--------------------------------------------------------------------------*/
/* 1. NULL/empty inputs return 0                                            */
/*--------------------------------------------------------------------------*/

void test_null_list_returns_zero(void)
{
    wifi_ap_t out[4];
    TEST_ASSERT_EQUAL_UINT16(0, wifi_multi_target_build(NULL, 4, out, 4));
}

void test_null_out_returns_zero(void)
{
    wifi_ap_t list[2] = { make_ap("SSID1", true), make_ap("SSID2", false) };
    TEST_ASSERT_EQUAL_UINT16(0, wifi_multi_target_build(list, 2, NULL, 4));
}

void test_zero_out_max_returns_zero(void)
{
    wifi_ap_t list[1] = { make_ap("SSID1", true) };
    wifi_ap_t out[1];
    TEST_ASSERT_EQUAL_UINT16(0, wifi_multi_target_build(list, 1, out, 0));
}

void test_empty_list_returns_zero(void)
{
    wifi_ap_t out[4];
    TEST_ASSERT_EQUAL_UINT16(0, wifi_multi_target_build(NULL, 0, out, 4));
}

/*--------------------------------------------------------------------------*/
/* 2. All entries unselected — returns 0, out untouched                    */
/*--------------------------------------------------------------------------*/

void test_no_selected_returns_zero(void)
{
    wifi_ap_t list[3] = {
        make_ap("A", false),
        make_ap("B", false),
        make_ap("C", false),
    };
    wifi_ap_t out[4];
    memset(out, 0xFF, sizeof(out));

    uint16_t n = wifi_multi_target_build(list, 3, out, 4);
    TEST_ASSERT_EQUAL_UINT16(0, n);
}

/*--------------------------------------------------------------------------*/
/* 3. Single selected entry is copied                                       */
/*--------------------------------------------------------------------------*/

void test_single_selected_copied(void)
{
    wifi_ap_t list[3] = {
        make_ap("Alpha", false),
        make_ap("Beta",  true),
        make_ap("Gamma", false),
    };
    wifi_ap_t out[4];

    uint16_t n = wifi_multi_target_build(list, 3, out, 4);
    TEST_ASSERT_EQUAL_UINT16(1, n);
    TEST_ASSERT_EQUAL_STRING("Beta", out[0].ssid);
    TEST_ASSERT_TRUE(out[0].selected);
}

/*--------------------------------------------------------------------------*/
/* 4. Multiple selected entries are all copied                              */
/*--------------------------------------------------------------------------*/

void test_multiple_selected_all_copied(void)
{
    wifi_ap_t list[5] = {
        make_ap("A", true),
        make_ap("B", false),
        make_ap("C", true),
        make_ap("D", false),
        make_ap("E", true),
    };
    wifi_ap_t out[8];

    uint16_t n = wifi_multi_target_build(list, 5, out, 8);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_STRING("A", out[0].ssid);
    TEST_ASSERT_EQUAL_STRING("C", out[1].ssid);
    TEST_ASSERT_EQUAL_STRING("E", out[2].ssid);
}

/*--------------------------------------------------------------------------*/
/* 5. out_max limits the number of copied entries                           */
/*--------------------------------------------------------------------------*/

void test_out_max_limits_output(void)
{
    wifi_ap_t list[4] = {
        make_ap("P", true),
        make_ap("Q", true),
        make_ap("R", true),
        make_ap("S", true),
    };
    wifi_ap_t out[2];

    uint16_t n = wifi_multi_target_build(list, 4, out, 2);
    TEST_ASSERT_EQUAL_UINT16(2, n);
    TEST_ASSERT_EQUAL_STRING("P", out[0].ssid);
    TEST_ASSERT_EQUAL_STRING("Q", out[1].ssid);
}

/*--------------------------------------------------------------------------*/
/* 6. All entries selected, exact fit                                       */
/*--------------------------------------------------------------------------*/

void test_all_selected_exact_fit(void)
{
    wifi_ap_t list[3] = {
        make_ap("X", true),
        make_ap("Y", true),
        make_ap("Z", true),
    };
    wifi_ap_t out[3];

    uint16_t n = wifi_multi_target_build(list, 3, out, 3);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_STRING("X", out[0].ssid);
    TEST_ASSERT_EQUAL_STRING("Y", out[1].ssid);
    TEST_ASSERT_EQUAL_STRING("Z", out[2].ssid);
}

/*--------------------------------------------------------------------------*/
/* 7. Field values are preserved (rssi, channel, bssid_str)                */
/*--------------------------------------------------------------------------*/

void test_field_values_preserved(void)
{
    wifi_ap_t src;
    memset(&src, 0, sizeof(src));
    strncpy(src.ssid, "TestAP", sizeof(src.ssid) - 1);
    src.rssi     = -55;
    src.channel  = 11;
    src.selected = true;
    src.bssid[0] = 0xAA;
    src.bssid[5] = 0xBB;
    memcpy(src.bssid_str, "AA:00:00:00:00:BB", 18);

    wifi_ap_t out[1];
    uint16_t n = wifi_multi_target_build(&src, 1, out, 1);
    TEST_ASSERT_EQUAL_UINT16(1, n);
    TEST_ASSERT_EQUAL_INT8(-55, out[0].rssi);
    TEST_ASSERT_EQUAL_UINT8(11, out[0].channel);
    TEST_ASSERT_EQUAL_UINT8(0xAA, out[0].bssid[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, out[0].bssid[5]);
    TEST_ASSERT_EQUAL_STRING("AA:00:00:00:00:BB", out[0].bssid_str);
    TEST_ASSERT_TRUE(out[0].selected);
}

/*--------------------------------------------------------------------------*/
/* 8. out_max == 1, multiple selected — only first is copied               */
/*--------------------------------------------------------------------------*/

void test_out_max_one_picks_first(void)
{
    wifi_ap_t list[3] = {
        make_ap("First",  true),
        make_ap("Second", true),
        make_ap("Third",  true),
    };
    wifi_ap_t out[1];

    uint16_t n = wifi_multi_target_build(list, 3, out, 1);
    TEST_ASSERT_EQUAL_UINT16(1, n);
    TEST_ASSERT_EQUAL_STRING("First", out[0].ssid);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_list_returns_zero);
    RUN_TEST(test_null_out_returns_zero);
    RUN_TEST(test_zero_out_max_returns_zero);
    RUN_TEST(test_empty_list_returns_zero);
    RUN_TEST(test_no_selected_returns_zero);
    RUN_TEST(test_single_selected_copied);
    RUN_TEST(test_multiple_selected_all_copied);
    RUN_TEST(test_out_max_limits_output);
    RUN_TEST(test_all_selected_exact_fit);
    RUN_TEST(test_field_values_preserved);
    RUN_TEST(test_out_max_one_picks_first);
    return UNITY_END();
}
