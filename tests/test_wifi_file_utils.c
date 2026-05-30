/* See COPYING.txt for license details. */

/**
 * test_wifi_file_utils.c
 *
 * Unit tests for the pure-logic helpers in wifi_file_utils.h / wifi_file_utils.c:
 *   wifi_ascii_lower()        — single-char case conversion
 *   wifi_ext_is_ap_cache()    — .tsv/.txt extension check
 *   wifi_ext_is_html()        — .html/.htm extension check
 *   wifi_ext_is_ssid_list()   — .txt/.lst extension check
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "wifi_file_utils.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * wifi_ascii_lower
 * =========================================================================*/

static void test_lower_uppercase_a(void)
{
    TEST_ASSERT_EQUAL_UINT8('a', wifi_ascii_lower('A'));
}

static void test_lower_uppercase_z(void)
{
    TEST_ASSERT_EQUAL_UINT8('z', wifi_ascii_lower('Z'));
}

static void test_lower_already_lowercase(void)
{
    TEST_ASSERT_EQUAL_UINT8('m', wifi_ascii_lower('m'));
}

static void test_lower_digit_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT8('5', wifi_ascii_lower('5'));
}

static void test_lower_dot_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT8('.', wifi_ascii_lower('.'));
}

/* =========================================================================
 * wifi_ext_is_ap_cache
 * =========================================================================*/

static void test_ap_cache_tsv(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_ap_cache("aps.tsv"));
}

static void test_ap_cache_txt(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_ap_cache("data.txt"));
}

static void test_ap_cache_TSV_upper(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_ap_cache("APS.TSV"));
}

static void test_ap_cache_TXT_mixed(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_ap_cache("File.TxT"));
}

static void test_ap_cache_csv_rejected(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_ap_cache("data.csv"));
}

static void test_ap_cache_html_rejected(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_ap_cache("page.html"));
}

static void test_ap_cache_no_dot(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_ap_cache("noextension"));
}

static void test_ap_cache_null(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_ap_cache(NULL));
}

static void test_ap_cache_dot_only(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_ap_cache("."));
}

static void test_ap_cache_path_with_tsv(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_ap_cache("wifi/scan_results.tsv"));
}

/* =========================================================================
 * wifi_ext_is_html
 * =========================================================================*/

static void test_html_lowercase(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_html("index.html"));
}

static void test_html_uppercase(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_html("PAGE.HTML"));
}

static void test_htm_lowercase(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_html("page.htm"));
}

static void test_htm_uppercase(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_html("PAGE.HTM"));
}

static void test_html_txt_rejected(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_html("page.txt"));
}

static void test_html_no_dot(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_html("noext"));
}

static void test_html_null(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_html(NULL));
}

static void test_html_path_with_ext(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_html("portal/evil.html"));
}

/* =========================================================================
 * wifi_ext_is_ssid_list
 * =========================================================================*/

static void test_ssid_list_txt(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_ssid_list("ssids.txt"));
}

static void test_ssid_list_lst(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_ssid_list("beacons.lst"));
}

static void test_ssid_list_LST_upper(void)
{
    TEST_ASSERT_TRUE(wifi_ext_is_ssid_list("FILE.LST"));
}

static void test_ssid_list_csv_rejected(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_ssid_list("data.csv"));
}

static void test_ssid_list_html_rejected(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_ssid_list("page.html"));
}

static void test_ssid_list_null(void)
{
    TEST_ASSERT_FALSE(wifi_ext_is_ssid_list(NULL));
}

/* =========================================================================
 * Runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* wifi_ascii_lower */
    RUN_TEST(test_lower_uppercase_a);
    RUN_TEST(test_lower_uppercase_z);
    RUN_TEST(test_lower_already_lowercase);
    RUN_TEST(test_lower_digit_unchanged);
    RUN_TEST(test_lower_dot_unchanged);

    /* wifi_ext_is_ap_cache */
    RUN_TEST(test_ap_cache_tsv);
    RUN_TEST(test_ap_cache_txt);
    RUN_TEST(test_ap_cache_TSV_upper);
    RUN_TEST(test_ap_cache_TXT_mixed);
    RUN_TEST(test_ap_cache_csv_rejected);
    RUN_TEST(test_ap_cache_html_rejected);
    RUN_TEST(test_ap_cache_no_dot);
    RUN_TEST(test_ap_cache_null);
    RUN_TEST(test_ap_cache_dot_only);
    RUN_TEST(test_ap_cache_path_with_tsv);

    /* wifi_ext_is_html */
    RUN_TEST(test_html_lowercase);
    RUN_TEST(test_html_uppercase);
    RUN_TEST(test_htm_lowercase);
    RUN_TEST(test_htm_uppercase);
    RUN_TEST(test_html_txt_rejected);
    RUN_TEST(test_html_no_dot);
    RUN_TEST(test_html_null);
    RUN_TEST(test_html_path_with_ext);

    /* wifi_ext_is_ssid_list */
    RUN_TEST(test_ssid_list_txt);
    RUN_TEST(test_ssid_list_lst);
    RUN_TEST(test_ssid_list_LST_upper);
    RUN_TEST(test_ssid_list_csv_rejected);
    RUN_TEST(test_ssid_list_html_rejected);
    RUN_TEST(test_ssid_list_null);

    return UNITY_END();
}
