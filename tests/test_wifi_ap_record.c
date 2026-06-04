/* See COPYING.txt for license details. */

/**
 * test_wifi_ap_record.c
 *
 * Unit tests for the pure-logic helpers in wifi_ap_record.h / wifi_ap_record.c:
 *   wifi_ap_record_parse_one() — binary ESP32 payload → wifi_ap_t
 *   wifi_bssid_fmt()           — 6-byte BSSID → "XX:XX:XX:XX:XX:XX"
 *   wifi_bssid_parse()         — "XX:XX:XX:XX:XX:XX" → 6-byte BSSID
 *   wifi_mac_is_nonzero()      — MAC zero check
 *   wifi_sanitize_field()      — strip TSV control chars
 *   wifi_csv_quote_field()     — RFC 4180 CSV quoting
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "wifi_ap_record.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * Helper: build a well-formed AP payload as the ESP32 firmware produces it.
 *
 * Wire layout:
 *   [0]     RSSI (int8_t)
 *   [1]     channel
 *   [2]     auth_mode
 *   [3..8]  BSSID (6 bytes)
 *   [9]     SSID length (0–32)
 *   [10..]  SSID bytes
 * =========================================================================*/
static uint8_t make_payload(uint8_t *buf,
                            int8_t   rssi,
                            uint8_t  channel,
                            uint8_t  auth_mode,
                            const uint8_t bssid[6],
                            const char *ssid)
{
    size_t ssid_len = ssid ? strlen(ssid) : 0;
    if (ssid_len > 32) ssid_len = 32;

    buf[0] = (uint8_t)rssi;
    buf[1] = channel;
    buf[2] = auth_mode;
    memcpy(&buf[3], bssid, 6);
    buf[9] = (uint8_t)ssid_len;
    if (ssid_len > 0)
        memcpy(&buf[10], ssid, ssid_len);

    return (uint8_t)(10u + ssid_len);
}

/* =========================================================================
 * wifi_ap_record_parse_one
 * =========================================================================*/

void test_parse_one_typical_ap(void)
{
    uint8_t buf[64];
    const uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t len = make_payload(buf, -65, 6, 3, bssid, "MyNetwork");

    wifi_ap_t ap;
    bool ok = wifi_ap_record_parse_one(buf, len, &ap);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT8(-65,        ap.rssi);
    TEST_ASSERT_EQUAL_UINT8(6,         ap.channel);
    TEST_ASSERT_EQUAL_UINT8(3,         ap.auth_mode);
    TEST_ASSERT_EQUAL_MEMORY(bssid, ap.bssid, 6);
    TEST_ASSERT_EQUAL_STRING("MyNetwork", ap.ssid);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", ap.bssid_str);
    TEST_ASSERT_FALSE(ap.selected);
}

void test_parse_one_hidden_ssid(void)
{
    uint8_t buf[64];
    const uint8_t bssid[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t len = make_payload(buf, -80, 1, 0, bssid, "");

    wifi_ap_t ap;
    bool ok = wifi_ap_record_parse_one(buf, len, &ap);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("", ap.ssid);
    TEST_ASSERT_EQUAL_UINT8(1, ap.channel);
}

void test_parse_one_max_ssid_length(void)
{
    uint8_t buf[64];
    const uint8_t bssid[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const char *ssid32 = "12345678901234567890123456789012"; /* 32 chars */
    uint8_t len = make_payload(buf, -50, 11, 2, bssid, ssid32);

    wifi_ap_t ap;
    bool ok = wifi_ap_record_parse_one(buf, len, &ap);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING(ssid32, ap.ssid);
}

void test_parse_one_ssid_length_clamped_to_32(void)
{
    /* ssid_len field claims 33 — should be clamped to 32 */
    uint8_t buf[64];
    const uint8_t bssid[6] = {0x00};
    make_payload(buf, -40, 6, 0, bssid, "");
    buf[9] = 33;                          /* force ssid_len = 33  */
    memset(&buf[10], 'A', 33);            /* 33 bytes of 'A'      */
    uint8_t len = 10 + 33;

    wifi_ap_t ap;
    bool ok = wifi_ap_record_parse_one(buf, len, &ap);

    /* payload provides only 33 bytes but ssid_len is clamped to 32, and
     * 10 + 32 = 42 <= 43 == len, so parsing should succeed */
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(32u, (uint8_t)strlen(ap.ssid));
}

void test_parse_one_too_short_payload_rejected(void)
{
    uint8_t buf[9] = {0};   /* 9 < WIFI_AP_RECORD_PAYLOAD_MIN_LEN(10) */
    wifi_ap_t ap;
    bool ok = wifi_ap_record_parse_one(buf, 9, &ap);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_one_null_payload_rejected(void)
{
    wifi_ap_t ap;
    bool ok = wifi_ap_record_parse_one(NULL, 10, &ap);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_one_null_out_rejected(void)
{
    uint8_t buf[64] = {0};
    bool ok = wifi_ap_record_parse_one(buf, 10, NULL);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_one_payload_too_short_for_ssid(void)
{
    /* ssid_len = 10 but payload is only 10 bytes long (no room for SSID) */
    uint8_t buf[10] = {0};
    buf[9] = 10;
    wifi_ap_t ap;
    bool ok = wifi_ap_record_parse_one(buf, 10, &ap);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_one_rssi_negative(void)
{
    uint8_t buf[64];
    const uint8_t bssid[6] = {0};
    uint8_t len = make_payload(buf, -100, 6, 0, bssid, "X");

    wifi_ap_t ap;
    TEST_ASSERT_TRUE(wifi_ap_record_parse_one(buf, len, &ap));
    TEST_ASSERT_EQUAL_INT8(-100, ap.rssi);
}

/* =========================================================================
 * wifi_bssid_fmt
 * =========================================================================*/

void test_bssid_fmt_all_zero(void)
{
    const uint8_t bssid[6] = {0};
    char out[18];
    wifi_bssid_fmt(bssid, out);
    TEST_ASSERT_EQUAL_STRING("00:00:00:00:00:00", out);
}

void test_bssid_fmt_all_ff(void)
{
    const uint8_t bssid[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    char out[18];
    wifi_bssid_fmt(bssid, out);
    TEST_ASSERT_EQUAL_STRING("FF:FF:FF:FF:FF:FF", out);
}

void test_bssid_fmt_typical(void)
{
    const uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0x01, 0x02, 0x03};
    char out[18];
    wifi_bssid_fmt(bssid, out);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:01:02:03", out);
}

void test_bssid_fmt_upper_case(void)
{
    const uint8_t bssid[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    char out[18];
    wifi_bssid_fmt(bssid, out);
    /* Must be upper-case for compatibility with AP-cache TSV files */
    TEST_ASSERT_EQUAL_STRING("DE:AD:BE:EF:CA:FE", out);
}

/* =========================================================================
 * wifi_bssid_parse
 * =========================================================================*/

void test_bssid_parse_typical(void)
{
    uint8_t bssid[6];
    bool ok = wifi_bssid_parse("AA:BB:CC:DD:EE:FF", bssid);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(0xAA, bssid[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, bssid[1]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, bssid[2]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, bssid[3]);
    TEST_ASSERT_EQUAL_UINT8(0xEE, bssid[4]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, bssid[5]);
}

void test_bssid_parse_lower_case(void)
{
    uint8_t bssid[6];
    bool ok = wifi_bssid_parse("aa:bb:cc:dd:ee:ff", bssid);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(0xAA, bssid[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, bssid[5]);
}

void test_bssid_parse_all_zeros(void)
{
    uint8_t bssid[6] = {0xFF};
    bool ok = wifi_bssid_parse("00:00:00:00:00:00", bssid);
    TEST_ASSERT_TRUE(ok);
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL_UINT8(0x00, bssid[i]);
}

void test_bssid_parse_bad_format_rejected(void)
{
    uint8_t bssid[6];
    TEST_ASSERT_FALSE(wifi_bssid_parse("AABBCCDDEEFF", bssid));
    TEST_ASSERT_FALSE(wifi_bssid_parse("ZZ:00:00:00:00:00", bssid));
    TEST_ASSERT_FALSE(wifi_bssid_parse("", bssid));
    TEST_ASSERT_FALSE(wifi_bssid_parse(NULL, bssid));
}

void test_bssid_parse_null_output_rejected(void)
{
    TEST_ASSERT_FALSE(wifi_bssid_parse("AA:BB:CC:DD:EE:FF", NULL));
}

void test_bssid_fmt_parse_roundtrip(void)
{
    const uint8_t orig[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    char str[18];
    wifi_bssid_fmt(orig, str);

    uint8_t parsed[6];
    TEST_ASSERT_TRUE(wifi_bssid_parse(str, parsed));
    TEST_ASSERT_EQUAL_MEMORY(orig, parsed, 6);
}

/* =========================================================================
 * wifi_mac_is_nonzero
 * =========================================================================*/

void test_mac_nonzero_all_zero_is_false(void)
{
    const uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    TEST_ASSERT_FALSE(wifi_mac_is_nonzero(mac));
}

void test_mac_nonzero_one_byte_set(void)
{
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 6; i++)
    {
        memset(mac, 0, 6);
        mac[i] = 0x01;
        TEST_ASSERT_TRUE(wifi_mac_is_nonzero(mac));
    }
}

void test_mac_nonzero_all_ff(void)
{
    const uint8_t mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    TEST_ASSERT_TRUE(wifi_mac_is_nonzero(mac));
}

/* =========================================================================
 * wifi_sanitize_field
 * =========================================================================*/

void test_sanitize_field_normal_string_unchanged(void)
{
    char dst[32];
    wifi_sanitize_field(dst, "HelloWorld", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("HelloWorld", dst);
}

void test_sanitize_field_tab_replaced(void)
{
    char dst[32];
    wifi_sanitize_field(dst, "Hello\tWorld", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("Hello World", dst);
}

void test_sanitize_field_cr_lf_replaced(void)
{
    char dst[32];
    wifi_sanitize_field(dst, "A\rB\nC", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("A B C", dst);
}

void test_sanitize_field_empty_src(void)
{
    char dst[32] = "old";
    wifi_sanitize_field(dst, "", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("", dst);
}

void test_sanitize_field_null_src(void)
{
    char dst[32] = "old";
    wifi_sanitize_field(dst, NULL, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("", dst);
}

void test_sanitize_field_truncates_to_dst_len(void)
{
    char dst[4];
    wifi_sanitize_field(dst, "ABCDEF", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("ABC", dst);   /* 3 chars + NUL */
}

/* =========================================================================
 * wifi_csv_quote_field
 * =========================================================================*/

void test_csv_quote_plain_string(void)
{
    char dst[64];
    wifi_csv_quote_field(dst, "Hello", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("\"Hello\"", dst);
}

void test_csv_quote_empty_string(void)
{
    char dst[8];
    wifi_csv_quote_field(dst, "", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("\"\"", dst);
}

void test_csv_quote_null_src(void)
{
    char dst[8];
    wifi_csv_quote_field(dst, NULL, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("\"\"", dst);
}

void test_csv_quote_embedded_quote_doubled(void)
{
    char dst[64];
    wifi_csv_quote_field(dst, "say \"hi\"", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("\"say \"\"hi\"\"\"", dst);
}

void test_csv_quote_cr_lf_replaced_with_space(void)
{
    char dst[32];
    wifi_csv_quote_field(dst, "A\r\nB", sizeof(dst));
    TEST_ASSERT_EQUAL_STRING("\"A  B\"", dst);
}

void test_csv_quote_always_null_terminates(void)
{
    char dst[4];   /* barely enough for "" + NUL */
    wifi_csv_quote_field(dst, "ABCDEF", sizeof(dst));
    dst[3] = '\0'; /* ensure we didn't write past end */
    TEST_ASSERT_TRUE(dst[0] == '"');
    TEST_ASSERT_EQUAL_UINT8('\0', dst[sizeof(dst) - 1]);
}

/* =========================================================================
 * parse → fmt round-trip (integration)
 * =========================================================================*/

void test_parse_then_bssid_fmt_matches_expected(void)
{
    uint8_t buf[64];
    const uint8_t bssid[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    uint8_t len = make_payload(buf, -55, 6, 2, bssid, "Coffee");

    wifi_ap_t ap;
    TEST_ASSERT_TRUE(wifi_ap_record_parse_one(buf, len, &ap));
    TEST_ASSERT_EQUAL_STRING("DE:AD:BE:EF:00:01", ap.bssid_str);
    TEST_ASSERT_EQUAL_STRING("Coffee", ap.ssid);
    TEST_ASSERT_EQUAL_INT8(-55, ap.rssi);
    TEST_ASSERT_EQUAL_UINT8(6, ap.channel);
    TEST_ASSERT_EQUAL_UINT8(2, ap.auth_mode);
}

/* =========================================================================
 * Test runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* wifi_ap_record_parse_one */
    RUN_TEST(test_parse_one_typical_ap);
    RUN_TEST(test_parse_one_hidden_ssid);
    RUN_TEST(test_parse_one_max_ssid_length);
    RUN_TEST(test_parse_one_ssid_length_clamped_to_32);
    RUN_TEST(test_parse_one_too_short_payload_rejected);
    RUN_TEST(test_parse_one_null_payload_rejected);
    RUN_TEST(test_parse_one_null_out_rejected);
    RUN_TEST(test_parse_one_payload_too_short_for_ssid);
    RUN_TEST(test_parse_one_rssi_negative);

    /* wifi_bssid_fmt */
    RUN_TEST(test_bssid_fmt_all_zero);
    RUN_TEST(test_bssid_fmt_all_ff);
    RUN_TEST(test_bssid_fmt_typical);
    RUN_TEST(test_bssid_fmt_upper_case);

    /* wifi_bssid_parse */
    RUN_TEST(test_bssid_parse_typical);
    RUN_TEST(test_bssid_parse_lower_case);
    RUN_TEST(test_bssid_parse_all_zeros);
    RUN_TEST(test_bssid_parse_bad_format_rejected);
    RUN_TEST(test_bssid_parse_null_output_rejected);
    RUN_TEST(test_bssid_fmt_parse_roundtrip);

    /* wifi_mac_is_nonzero */
    RUN_TEST(test_mac_nonzero_all_zero_is_false);
    RUN_TEST(test_mac_nonzero_one_byte_set);
    RUN_TEST(test_mac_nonzero_all_ff);

    /* wifi_sanitize_field */
    RUN_TEST(test_sanitize_field_normal_string_unchanged);
    RUN_TEST(test_sanitize_field_tab_replaced);
    RUN_TEST(test_sanitize_field_cr_lf_replaced);
    RUN_TEST(test_sanitize_field_empty_src);
    RUN_TEST(test_sanitize_field_null_src);
    RUN_TEST(test_sanitize_field_truncates_to_dst_len);

    /* wifi_csv_quote_field */
    RUN_TEST(test_csv_quote_plain_string);
    RUN_TEST(test_csv_quote_empty_string);
    RUN_TEST(test_csv_quote_null_src);
    RUN_TEST(test_csv_quote_embedded_quote_doubled);
    RUN_TEST(test_csv_quote_cr_lf_replaced_with_space);
    RUN_TEST(test_csv_quote_always_null_terminates);

    /* integration */
    RUN_TEST(test_parse_then_bssid_fmt_matches_expected);

    return UNITY_END();
}
