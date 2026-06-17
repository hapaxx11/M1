/**
 * @file   test_wifi_at_scan.c
 * @brief  Unit tests for wifi_at_scan.c (AT+CWLAP parser and AT+M1PMKID parser).
 *
 * Tests exercise wifi_at_cwlap_parse(), wifi_at_format_bssid(), and
 * wifi_at_pmkid_parse() with realistic dag T-800 firmware response fixtures.
 */
#include "unity.h"
#include "wifi_at_scan.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

/*===========================================================================*/
/* wifi_at_cwlap_parse                                                       */
/*===========================================================================*/

void test_cwlap_null_resp_returns_zero(void)
{
    wifi_at_ap_t out[4];
    TEST_ASSERT_EQUAL_UINT8(0, wifi_at_cwlap_parse(NULL, out, 4));
}

void test_cwlap_null_out_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, wifi_at_cwlap_parse("+CWLAP:(3,\"x\",-50,\"aa:bb:cc:dd:ee:ff\",6)\r\nOK\r\n", NULL, 4));
}

void test_cwlap_zero_max_returns_zero(void)
{
    wifi_at_ap_t out[4];
    TEST_ASSERT_EQUAL_UINT8(0, wifi_at_cwlap_parse("+CWLAP:(3,\"x\",-50,\"aa:bb:cc:dd:ee:ff\",6)\r\nOK\r\n", out, 0));
}

void test_cwlap_empty_response_returns_zero(void)
{
    wifi_at_ap_t out[4];
    TEST_ASSERT_EQUAL_UINT8(0, wifi_at_cwlap_parse("\r\nOK\r\n", out, 4));
}

void test_cwlap_single_ap(void)
{
    const char *resp =
        "+CWLAP:(4,\"HomeNet\",-55,\"aa:bb:cc:dd:ee:ff\",6)\r\nOK\r\n";
    wifi_at_ap_t out[4];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_EQUAL_STRING("HomeNet", out[0].ssid);
    TEST_ASSERT_EQUAL_INT8(-55, out[0].rssi);
    TEST_ASSERT_EQUAL_UINT8(6, out[0].channel);
    TEST_ASSERT_EQUAL_HEX8(0xaa, out[0].bssid[0]);
    TEST_ASSERT_EQUAL_HEX8(0xbb, out[0].bssid[1]);
    TEST_ASSERT_EQUAL_HEX8(0xcc, out[0].bssid[2]);
    TEST_ASSERT_EQUAL_HEX8(0xdd, out[0].bssid[3]);
    TEST_ASSERT_EQUAL_HEX8(0xee, out[0].bssid[4]);
    TEST_ASSERT_EQUAL_HEX8(0xff, out[0].bssid[5]);
}

void test_cwlap_multiple_aps(void)
{
    const char *resp =
        "+CWLAP:(4,\"Alpha\",-45,\"11:22:33:44:55:66\",1)\r\n"
        "+CWLAP:(3,\"Bravo\",-72,\"aa:bb:cc:dd:ee:ff\",11)\r\n"
        "+CWLAP:(2,\"Charlie\",-80,\"fe:dc:ba:98:76:54\",6)\r\n"
        "\r\nOK\r\n";
    wifi_at_ap_t out[8];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 8);
    TEST_ASSERT_EQUAL_UINT8(3, n);
    TEST_ASSERT_EQUAL_STRING("Alpha", out[0].ssid);
    TEST_ASSERT_EQUAL_UINT8(1, out[0].channel);
    TEST_ASSERT_EQUAL_STRING("Bravo", out[1].ssid);
    TEST_ASSERT_EQUAL_UINT8(11, out[1].channel);
    TEST_ASSERT_EQUAL_STRING("Charlie", out[2].ssid);
    TEST_ASSERT_EQUAL_UINT8(6, out[2].channel);
}

void test_cwlap_hidden_ssid(void)
{
    /* ESP-AT emits an empty SSID string for hidden networks */
    const char *resp =
        "+CWLAP:(2,\"\",-60,\"de:ad:be:ef:ca:fe\",13)\r\nOK\r\n";
    wifi_at_ap_t out[4];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_EQUAL_STRING("", out[0].ssid);
    TEST_ASSERT_EQUAL_UINT8(13, out[0].channel);
    TEST_ASSERT_EQUAL_HEX8(0xde, out[0].bssid[0]);
}

void test_cwlap_max_cap(void)
{
    /* More APs than max_out — result must be capped */
    const char *resp =
        "+CWLAP:(4,\"A\",-40,\"01:02:03:04:05:06\",1)\r\n"
        "+CWLAP:(4,\"B\",-41,\"01:02:03:04:05:07\",6)\r\n"
        "+CWLAP:(4,\"C\",-42,\"01:02:03:04:05:08\",11)\r\n";
    wifi_at_ap_t out[2];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 2);
    TEST_ASSERT_EQUAL_UINT8(2, n);
    TEST_ASSERT_EQUAL_STRING("A", out[0].ssid);
    TEST_ASSERT_EQUAL_STRING("B", out[1].ssid);
}

void test_cwlap_extra_fields_ignored(void)
{
    /* Real T-800 AT+CWLAP often includes extra fields beyond channel */
    const char *resp =
        "+CWLAP:(4,\"Office\",-67,\"aa:bb:cc:dd:ee:01\",6,0,0,3,4,7,0)\r\nOK\r\n";
    wifi_at_ap_t out[4];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_EQUAL_STRING("Office", out[0].ssid);
    TEST_ASSERT_EQUAL_UINT8(6, out[0].channel);
    TEST_ASSERT_EQUAL_INT8(-67, out[0].rssi);
}

void test_cwlap_ssid_at_32_chars(void)
{
    /* 32-char SSID — maximum allowed; must not overflow */
    const char *resp =
        "+CWLAP:(3,\"12345678901234567890123456789012\",-50,\"aa:bb:cc:dd:ee:ff\",6)\r\nOK\r\n";
    wifi_at_ap_t out[4];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_EQUAL_INT(32, (int)strlen(out[0].ssid));
    /* Null terminator must be present */
    TEST_ASSERT_EQUAL_CHAR('\0', out[0].ssid[32]);
}

void test_cwlap_malformed_bssid_skipped(void)
{
    /* Entry with invalid MAC is skipped; valid entry is still parsed */
    const char *resp =
        "+CWLAP:(4,\"Bad\",-55,\"not:a:mac\",6)\r\n"
        "+CWLAP:(4,\"Good\",-55,\"01:02:03:04:05:06\",11)\r\n"
        "\r\nOK\r\n";
    wifi_at_ap_t out[4];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_EQUAL_STRING("Good", out[0].ssid);
}

void test_cwlap_ssid_with_closing_paren(void)
{
    /* SSID containing ')' must not cause the entry-advance logic to cut
     * the current entry short and skip the following entry. */
    const char *resp =
        "+CWLAP:(4,\"Net)work\",-55,\"aa:bb:cc:dd:ee:ff\",6)\r\n"
        "+CWLAP:(3,\"Second\",-70,\"11:22:33:44:55:66\",11)\r\n"
        "\r\nOK\r\n";
    wifi_at_ap_t out[4];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 4);
    TEST_ASSERT_EQUAL_UINT8(2, n);
    TEST_ASSERT_EQUAL_STRING("Net)work", out[0].ssid);
    TEST_ASSERT_EQUAL_UINT8(6, out[0].channel);
    TEST_ASSERT_EQUAL_STRING("Second", out[1].ssid);
    TEST_ASSERT_EQUAL_UINT8(11, out[1].channel);
}

void test_cwlap_oversized_mac_octet_skipped(void)
{
    /* A malformed response where a MAC octet value would overflow uint8_t
     * (e.g. "1ff") must be rejected; the following valid entry is parsed. */
    const char *resp =
        "+CWLAP:(4,\"Bad\",-55,\"1ff:bb:cc:dd:ee:ff\",6)\r\n"
        "+CWLAP:(4,\"Good\",-55,\"01:02:03:04:05:06\",11)\r\n"
        "\r\nOK\r\n";
    wifi_at_ap_t out[4];
    memset(out, 0, sizeof(out));

    uint8_t n = wifi_at_cwlap_parse(resp, out, 4);
    TEST_ASSERT_EQUAL_UINT8(1, n);
    TEST_ASSERT_EQUAL_STRING("Good", out[0].ssid);
}

/*===========================================================================*/
/* wifi_at_format_bssid                                                      */
/*===========================================================================*/

void test_format_bssid_basic(void)
{
    const uint8_t mac[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    char dst[18];
    wifi_at_format_bssid(mac, dst);
    TEST_ASSERT_EQUAL_STRING("aa:bb:cc:dd:ee:ff", dst);
}

void test_format_bssid_zeros(void)
{
    const uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    char dst[18];
    wifi_at_format_bssid(mac, dst);
    TEST_ASSERT_EQUAL_STRING("00:00:00:00:00:00", dst);
}

void test_format_bssid_mixed(void)
{
    const uint8_t mac[6] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab};
    char dst[18];
    wifi_at_format_bssid(mac, dst);
    TEST_ASSERT_EQUAL_STRING("01:23:45:67:89:ab", dst);
}

/*===========================================================================*/
/* wifi_at_pmkid_parse                                                       */
/*===========================================================================*/

void test_pmkid_parse_null_resp_returns_false(void)
{
    uint8_t bssid[6] = {0}, pmkid[16] = {0};
    TEST_ASSERT_FALSE(wifi_at_pmkid_parse(NULL, bssid, pmkid));
}

void test_pmkid_parse_null_bssid_out_returns_false(void)
{
    uint8_t pmkid[16] = {0};
    TEST_ASSERT_FALSE(wifi_at_pmkid_parse(
        "+M1PMKID:aa:bb:cc:dd:ee:ff,aabbccddeeff00112233445566778899\r\nOK\r\n",
        NULL, pmkid));
}

void test_pmkid_parse_null_pmkid_out_returns_false(void)
{
    uint8_t bssid[6] = {0};
    TEST_ASSERT_FALSE(wifi_at_pmkid_parse(
        "+M1PMKID:aa:bb:cc:dd:ee:ff,aabbccddeeff00112233445566778899\r\nOK\r\n",
        bssid, NULL));
}

void test_pmkid_parse_success(void)
{
    /* Realistic T-800 response: BSSID + 32-hex PMKID */
    const char *resp =
        "+M1PMKID:aa:bb:cc:dd:ee:ff,aabbccddeeff00112233445566778899\r\nOK\r\n";
    uint8_t bssid[6] = {0};
    uint8_t pmkid[16] = {0};

    TEST_ASSERT_TRUE(wifi_at_pmkid_parse(resp, bssid, pmkid));

    TEST_ASSERT_EQUAL_HEX8(0xaa, bssid[0]);
    TEST_ASSERT_EQUAL_HEX8(0xbb, bssid[1]);
    TEST_ASSERT_EQUAL_HEX8(0xcc, bssid[2]);
    TEST_ASSERT_EQUAL_HEX8(0xdd, bssid[3]);
    TEST_ASSERT_EQUAL_HEX8(0xee, bssid[4]);
    TEST_ASSERT_EQUAL_HEX8(0xff, bssid[5]);

    /* PMKID bytes: aabbccdd eeff0011 22334455 66778899 */
    TEST_ASSERT_EQUAL_HEX8(0xaa, pmkid[0]);
    TEST_ASSERT_EQUAL_HEX8(0xbb, pmkid[1]);
    TEST_ASSERT_EQUAL_HEX8(0xcc, pmkid[2]);
    TEST_ASSERT_EQUAL_HEX8(0xdd, pmkid[3]);
    TEST_ASSERT_EQUAL_HEX8(0xee, pmkid[4]);
    TEST_ASSERT_EQUAL_HEX8(0xff, pmkid[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00, pmkid[6]);
    TEST_ASSERT_EQUAL_HEX8(0x11, pmkid[7]);
    TEST_ASSERT_EQUAL_HEX8(0x22, pmkid[8]);
    TEST_ASSERT_EQUAL_HEX8(0x33, pmkid[9]);
    TEST_ASSERT_EQUAL_HEX8(0x44, pmkid[10]);
    TEST_ASSERT_EQUAL_HEX8(0x55, pmkid[11]);
    TEST_ASSERT_EQUAL_HEX8(0x66, pmkid[12]);
    TEST_ASSERT_EQUAL_HEX8(0x77, pmkid[13]);
    TEST_ASSERT_EQUAL_HEX8(0x88, pmkid[14]);
    TEST_ASSERT_EQUAL_HEX8(0x99, pmkid[15]);
}

void test_pmkid_parse_error_response_returns_false(void)
{
    /* T-800 returns ERR when AP doesn't respond / bad params */
    const char *resp = "+M1PMKID:ERR:BAD_CH\r\nERROR\r\n";
    uint8_t bssid[6] = {0}, pmkid[16] = {0};
    TEST_ASSERT_FALSE(wifi_at_pmkid_parse(resp, bssid, pmkid));
}

void test_pmkid_parse_error_parse_bssid_returns_false(void)
{
    const char *resp = "+M1PMKID:ERR:PARSE_BSSID\r\nERROR\r\n";
    uint8_t bssid[6] = {0}, pmkid[16] = {0};
    TEST_ASSERT_FALSE(wifi_at_pmkid_parse(resp, bssid, pmkid));
}

void test_pmkid_parse_no_prefix_returns_false(void)
{
    const char *resp = "OK\r\n";
    uint8_t bssid[6] = {0}, pmkid[16] = {0};
    TEST_ASSERT_FALSE(wifi_at_pmkid_parse(resp, bssid, pmkid));
}

void test_pmkid_parse_short_hex_returns_false(void)
{
    /* Only 30 hex chars instead of 32 */
    const char *resp =
        "+M1PMKID:aa:bb:cc:dd:ee:ff,aabbccddeeff0011223344556677\r\nOK\r\n";
    uint8_t bssid[6] = {0}, pmkid[16] = {0};
    TEST_ASSERT_FALSE(wifi_at_pmkid_parse(resp, bssid, pmkid));
}

void test_pmkid_parse_uppercase_hex_accepted(void)
{
    /* Some AP firmware may return uppercase hex */
    const char *resp =
        "+M1PMKID:AA:BB:CC:DD:EE:FF,AABBCCDDEEFF00112233445566778899\r\nOK\r\n";
    uint8_t bssid[6] = {0}, pmkid[16] = {0};
    TEST_ASSERT_TRUE(wifi_at_pmkid_parse(resp, bssid, pmkid));
    TEST_ASSERT_EQUAL_HEX8(0xAA, bssid[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, pmkid[0]);
}

void test_pmkid_parse_oversized_mac_octet_returns_false(void)
{
    /* A MAC octet value that overflows uint8_t (e.g. "1ff") must be rejected */
    const char *resp =
        "+M1PMKID:1ff:bb:cc:dd:ee:ff,aabbccddeeff00112233445566778899\r\nOK\r\n";
    uint8_t bssid[6] = {0}, pmkid[16] = {0};
    TEST_ASSERT_FALSE(wifi_at_pmkid_parse(resp, bssid, pmkid));
}

/*===========================================================================*/
/* main                                                                      */
/*===========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* wifi_at_cwlap_parse */
    RUN_TEST(test_cwlap_null_resp_returns_zero);
    RUN_TEST(test_cwlap_null_out_returns_zero);
    RUN_TEST(test_cwlap_zero_max_returns_zero);
    RUN_TEST(test_cwlap_empty_response_returns_zero);
    RUN_TEST(test_cwlap_single_ap);
    RUN_TEST(test_cwlap_multiple_aps);
    RUN_TEST(test_cwlap_hidden_ssid);
    RUN_TEST(test_cwlap_max_cap);
    RUN_TEST(test_cwlap_extra_fields_ignored);
    RUN_TEST(test_cwlap_ssid_at_32_chars);
    RUN_TEST(test_cwlap_malformed_bssid_skipped);
    RUN_TEST(test_cwlap_ssid_with_closing_paren);
    RUN_TEST(test_cwlap_oversized_mac_octet_skipped);

    /* wifi_at_format_bssid */
    RUN_TEST(test_format_bssid_basic);
    RUN_TEST(test_format_bssid_zeros);
    RUN_TEST(test_format_bssid_mixed);

    /* wifi_at_pmkid_parse */
    RUN_TEST(test_pmkid_parse_null_resp_returns_false);
    RUN_TEST(test_pmkid_parse_null_bssid_out_returns_false);
    RUN_TEST(test_pmkid_parse_null_pmkid_out_returns_false);
    RUN_TEST(test_pmkid_parse_success);
    RUN_TEST(test_pmkid_parse_error_response_returns_false);
    RUN_TEST(test_pmkid_parse_error_parse_bssid_returns_false);
    RUN_TEST(test_pmkid_parse_no_prefix_returns_false);
    RUN_TEST(test_pmkid_parse_short_hex_returns_false);
    RUN_TEST(test_pmkid_parse_uppercase_hex_accepted);
    RUN_TEST(test_pmkid_parse_oversized_mac_octet_returns_false);

    return UNITY_END();
}
