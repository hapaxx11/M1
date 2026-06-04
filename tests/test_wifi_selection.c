/* See COPYING.txt for license details. */

/**
 * test_wifi_selection.c
 *
 * Unit tests for the pure-logic helpers in wifi_selection.h / wifi_selection.c:
 *   wifi_selected_ap_count() — count selected entries in a wifi_ap_t array
 *   wifi_selected_sta_count() — count selected entries in a wifi_sta_t array
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "wifi_selection.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * Helpers
 * ========================================================================= */

static wifi_ap_t make_ap(bool selected)
{
    wifi_ap_t ap;
    memset(&ap, 0, sizeof(ap));
    ap.selected = selected;
    ap.channel  = 6;
    return ap;
}

static wifi_sta_t make_sta(bool selected)
{
    wifi_sta_t sta;
    memset(&sta, 0, sizeof(sta));
    sta.selected = selected;
    sta.channel  = 1;
    return sta;
}

/* =========================================================================
 * wifi_selected_ap_count — null / empty
 * ========================================================================= */

static void test_ap_null_list_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_ap_count(NULL, 5));
}

static void test_ap_empty_list_returns_zero(void)
{
    wifi_ap_t ap = make_ap(true);
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_ap_count(&ap, 0));
}

/* =========================================================================
 * wifi_selected_ap_count — none / some / all selected
 * ========================================================================= */

static void test_ap_none_selected(void)
{
    wifi_ap_t list[3];
    list[0] = make_ap(false);
    list[1] = make_ap(false);
    list[2] = make_ap(false);
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_ap_count(list, 3));
}

static void test_ap_one_of_three_selected(void)
{
    wifi_ap_t list[3];
    list[0] = make_ap(false);
    list[1] = make_ap(true);
    list[2] = make_ap(false);
    TEST_ASSERT_EQUAL_UINT16(1, wifi_selected_ap_count(list, 3));
}

static void test_ap_all_selected(void)
{
    wifi_ap_t list[4];
    list[0] = make_ap(true);
    list[1] = make_ap(true);
    list[2] = make_ap(true);
    list[3] = make_ap(true);
    TEST_ASSERT_EQUAL_UINT16(4, wifi_selected_ap_count(list, 4));
}

static void test_ap_first_selected_only(void)
{
    wifi_ap_t list[3];
    list[0] = make_ap(true);
    list[1] = make_ap(false);
    list[2] = make_ap(false);
    TEST_ASSERT_EQUAL_UINT16(1, wifi_selected_ap_count(list, 3));
}

static void test_ap_last_selected_only(void)
{
    wifi_ap_t list[3];
    list[0] = make_ap(false);
    list[1] = make_ap(false);
    list[2] = make_ap(true);
    TEST_ASSERT_EQUAL_UINT16(1, wifi_selected_ap_count(list, 3));
}

static void test_ap_count_respects_len_param(void)
{
    /* Pass only the first 2 entries from a 3-entry array */
    wifi_ap_t list[3];
    list[0] = make_ap(false);
    list[1] = make_ap(false);
    list[2] = make_ap(true);  /* should be invisible */
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_ap_count(list, 2));
}

/* =========================================================================
 * wifi_selected_sta_count — null / empty
 * ========================================================================= */

static void test_sta_null_list_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_sta_count(NULL, 5));
}

static void test_sta_empty_list_returns_zero(void)
{
    wifi_sta_t sta = make_sta(true);
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_sta_count(&sta, 0));
}

/* =========================================================================
 * wifi_selected_sta_count — none / some / all selected
 * ========================================================================= */

static void test_sta_none_selected(void)
{
    wifi_sta_t list[3];
    list[0] = make_sta(false);
    list[1] = make_sta(false);
    list[2] = make_sta(false);
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_sta_count(list, 3));
}

static void test_sta_two_of_four_selected(void)
{
    wifi_sta_t list[4];
    list[0] = make_sta(true);
    list[1] = make_sta(false);
    list[2] = make_sta(true);
    list[3] = make_sta(false);
    TEST_ASSERT_EQUAL_UINT16(2, wifi_selected_sta_count(list, 4));
}

static void test_sta_all_selected(void)
{
    wifi_sta_t list[3];
    list[0] = make_sta(true);
    list[1] = make_sta(true);
    list[2] = make_sta(true);
    TEST_ASSERT_EQUAL_UINT16(3, wifi_selected_sta_count(list, 3));
}

static void test_sta_count_respects_len_param(void)
{
    wifi_sta_t list[3];
    list[0] = make_sta(false);
    list[1] = make_sta(false);
    list[2] = make_sta(true);  /* beyond visible window */
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_sta_count(list, 2));
}

/* =========================================================================
 * Single-entry edge cases
 * ========================================================================= */

static void test_ap_single_selected(void)
{
    wifi_ap_t ap = make_ap(true);
    TEST_ASSERT_EQUAL_UINT16(1, wifi_selected_ap_count(&ap, 1));
}

static void test_ap_single_not_selected(void)
{
    wifi_ap_t ap = make_ap(false);
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_ap_count(&ap, 1));
}

static void test_sta_single_selected(void)
{
    wifi_sta_t sta = make_sta(true);
    TEST_ASSERT_EQUAL_UINT16(1, wifi_selected_sta_count(&sta, 1));
}

static void test_sta_single_not_selected(void)
{
    wifi_sta_t sta = make_sta(false);
    TEST_ASSERT_EQUAL_UINT16(0, wifi_selected_sta_count(&sta, 1));
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ap_null_list_returns_zero);
    RUN_TEST(test_ap_empty_list_returns_zero);
    RUN_TEST(test_ap_none_selected);
    RUN_TEST(test_ap_one_of_three_selected);
    RUN_TEST(test_ap_all_selected);
    RUN_TEST(test_ap_first_selected_only);
    RUN_TEST(test_ap_last_selected_only);
    RUN_TEST(test_ap_count_respects_len_param);

    RUN_TEST(test_sta_null_list_returns_zero);
    RUN_TEST(test_sta_empty_list_returns_zero);
    RUN_TEST(test_sta_none_selected);
    RUN_TEST(test_sta_two_of_four_selected);
    RUN_TEST(test_sta_all_selected);
    RUN_TEST(test_sta_count_respects_len_param);

    RUN_TEST(test_ap_single_selected);
    RUN_TEST(test_ap_single_not_selected);
    RUN_TEST(test_sta_single_selected);
    RUN_TEST(test_sta_single_not_selected);

    return UNITY_END();
}
