/* See COPYING.txt for license details. */

/*
 * test_nfc_ndef_encode.c
 *
 * Host-side unit tests for nfc_ndef_encode.c — NDEF record encoding
 * for URI, Text, WiFi, and Phone records.
 */

#include "unity.h"
#include "nfc_ndef_encode.h"
#include <string.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

/*═══════════════ URI Record ═══════════════*/

void test_uri_https_www(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri(buf, sizeof(buf), NDEF_URI_HTTPS_WWW, "example.com");
    TEST_ASSERT_GREATER_THAN(0, n);

    /* TLV type = 0x03 (NDEF Message) */
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[0]);

    /* TLV length (short format) */
    uint8_t tlv_len = buf[1];
    TEST_ASSERT_GREATER_THAN(0, tlv_len);

    /* NDEF record header: MB=1,ME=1,CF=0,SR=1,IL=0,TNF=0x01 */
    TEST_ASSERT_EQUAL_HEX8(0xD1, buf[2]);

    /* Type length = 1 */
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[3]);

    /* Payload length = 1 (uri_code) + strlen("example.com") = 12 */
    TEST_ASSERT_EQUAL_HEX8(12, buf[4]);

    /* Type = 'U' */
    TEST_ASSERT_EQUAL_HEX8('U', buf[5]);

    /* URI code = 0x02 (https://www.) */
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[6]);

    /* URI data */
    TEST_ASSERT_EQUAL_MEMORY("example.com", &buf[7], 11);

    /* Terminator TLV */
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);
}

void test_uri_http(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri(buf, sizeof(buf), NDEF_URI_HTTP, "example.com/page");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[6]);  /* URI code = http:// */
}

void test_uri_tel(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri(buf, sizeof(buf), NDEF_URI_TEL, "+1234567890");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x05, buf[6]);  /* URI code = tel: */
    TEST_ASSERT_EQUAL_MEMORY("+1234567890", &buf[7], 11);
}

void test_uri_none_full(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri(buf, sizeof(buf), NDEF_URI_NONE, "custom://test");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[6]);  /* No prefix */
}

void test_uri_null_returns_zero(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri(buf, sizeof(buf), NDEF_URI_HTTP, NULL);
    TEST_ASSERT_EQUAL(0, n);
}

void test_uri_null_buffer_returns_zero(void)
{
    size_t n = ndef_encode_uri(NULL, 128, NDEF_URI_HTTP, "test");
    TEST_ASSERT_EQUAL(0, n);
}

void test_uri_buffer_too_small(void)
{
    uint8_t buf[5];
    size_t n = ndef_encode_uri(buf, sizeof(buf), NDEF_URI_HTTP, "example.com");
    TEST_ASSERT_EQUAL(0, n);
}

void test_uri_empty_string(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri(buf, sizeof(buf), NDEF_URI_HTTPS, "");
    TEST_ASSERT_GREATER_THAN(0, n);
    /* Payload should just be the URI code byte */
    TEST_ASSERT_EQUAL_HEX8(1, buf[4]);  /* payload_len = 1 */
}

/*═══════════════ URI Auto-Detect ═══════════════*/

void test_uri_auto_https_www(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri_auto(buf, sizeof(buf), "https://www.example.com");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[6]);  /* NDEF_URI_HTTPS_WWW */
    TEST_ASSERT_EQUAL_MEMORY("example.com", &buf[7], 11);
}

void test_uri_auto_http_www(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri_auto(buf, sizeof(buf), "http://www.google.com");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[6]);  /* NDEF_URI_HTTP_WWW */
}

void test_uri_auto_https(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri_auto(buf, sizeof(buf), "https://api.test.io");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x04, buf[6]);  /* NDEF_URI_HTTPS */
}

void test_uri_auto_http(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri_auto(buf, sizeof(buf), "http://192.168.1.1");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[6]);  /* NDEF_URI_HTTP */
}

void test_uri_auto_tel(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri_auto(buf, sizeof(buf), "tel:+1234567890");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x05, buf[6]);  /* NDEF_URI_TEL */
}

void test_uri_auto_mailto(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri_auto(buf, sizeof(buf), "mailto:test@example.com");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x06, buf[6]);  /* NDEF_URI_MAILTO */
}

void test_uri_auto_unknown_protocol(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_uri_auto(buf, sizeof(buf), "ftp://files.example.com");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[6]);  /* NDEF_URI_NONE */
}

void test_uri_auto_null(void)
{
    uint8_t buf[128];
    TEST_ASSERT_EQUAL(0, ndef_encode_uri_auto(buf, sizeof(buf), NULL));
}

/*═══════════════ Text Record ═══════════════*/

void test_text_basic(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_text(buf, sizeof(buf), "en", "Hello, World!");
    TEST_ASSERT_GREATER_THAN(0, n);

    /* TLV type */
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[0]);

    /* NDEF header: MB=1,ME=1,CF=0,SR=1,IL=0,TNF=0x01 */
    TEST_ASSERT_EQUAL_HEX8(0xD1, buf[2]);

    /* Type = 'T' */
    TEST_ASSERT_EQUAL_HEX8('T', buf[5]);

    /* Status byte: UTF-8 (bit 7 = 0), lang_len = 2 */
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[6]);

    /* Language code */
    TEST_ASSERT_EQUAL_MEMORY("en", &buf[7], 2);

    /* Text content */
    TEST_ASSERT_EQUAL_MEMORY("Hello, World!", &buf[9], 13);

    /* Terminator */
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);
}

void test_text_french(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_text(buf, sizeof(buf), "fr", "Bonjour");
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[6]);  /* lang_len = 2 */
    TEST_ASSERT_EQUAL_MEMORY("fr", &buf[7], 2);
    TEST_ASSERT_EQUAL_MEMORY("Bonjour", &buf[9], 7);
}

void test_text_null_lang(void)
{
    uint8_t buf[128];
    TEST_ASSERT_EQUAL(0, ndef_encode_text(buf, sizeof(buf), NULL, "test"));
}

void test_text_null_text(void)
{
    uint8_t buf[128];
    TEST_ASSERT_EQUAL(0, ndef_encode_text(buf, sizeof(buf), "en", NULL));
}

void test_text_empty(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_text(buf, sizeof(buf), "en", "");
    TEST_ASSERT_GREATER_THAN(0, n);
    /* Payload = 1 (status) + 2 (lang) = 3 */
    TEST_ASSERT_EQUAL_HEX8(3, buf[4]);
}

/*═══════════════ WiFi Record ═══════════════*/

void test_wifi_wpa2(void)
{
    uint8_t buf[512];
    size_t n = ndef_encode_wifi(buf, sizeof(buf), "MyNetwork", "secret123", true);
    TEST_ASSERT_GREATER_THAN(0, n);

    /* TLV type */
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[0]);

    /* TNF should be 0x02 (Media type) */
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[2] & 0x07);

    /* Terminator */
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);
}

void test_wifi_open(void)
{
    uint8_t buf[512];
    size_t n = ndef_encode_wifi(buf, sizeof(buf), "OpenNet", NULL, false);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);
}

void test_wifi_null_ssid(void)
{
    uint8_t buf[512];
    TEST_ASSERT_EQUAL(0, ndef_encode_wifi(buf, sizeof(buf), NULL, "pass", true));
}

/*═══════════════ Phone Record ═══════════════*/

void test_phone_basic(void)
{
    uint8_t buf[128];
    size_t n = ndef_encode_phone(buf, sizeof(buf), "+1234567890");
    TEST_ASSERT_GREATER_THAN(0, n);

    /* URI code = tel: (0x05) */
    TEST_ASSERT_EQUAL_HEX8(0x05, buf[6]);
    TEST_ASSERT_EQUAL_MEMORY("+1234567890", &buf[7], 11);
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);
}

/*═══════════════ Pages Needed ═══════════════*/

void test_pages_exact(void)
{
    TEST_ASSERT_EQUAL(1, ndef_pages_needed(4));
    TEST_ASSERT_EQUAL(2, ndef_pages_needed(8));
}

void test_pages_rounded(void)
{
    TEST_ASSERT_EQUAL(1, ndef_pages_needed(1));
    TEST_ASSERT_EQUAL(1, ndef_pages_needed(3));
    TEST_ASSERT_EQUAL(2, ndef_pages_needed(5));
    TEST_ASSERT_EQUAL(3, ndef_pages_needed(9));
}

void test_pages_zero(void)
{
    TEST_ASSERT_EQUAL(0, ndef_pages_needed(0));
}

/*═══════════════ Cross-format consistency ═══════════════*/

void test_all_records_start_with_tlv(void)
{
    uint8_t buf[512];
    size_t n;

    n = ndef_encode_uri(buf, sizeof(buf), NDEF_URI_HTTPS, "test.com");
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);

    n = ndef_encode_text(buf, sizeof(buf), "en", "test");
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);

    n = ndef_encode_wifi(buf, sizeof(buf), "ssid", "pass", true);
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);

    n = ndef_encode_phone(buf, sizeof(buf), "123");
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFE, buf[n - 1]);
}

/*═══════════════ Main ═══════════════*/

int main(void)
{
    UNITY_BEGIN();

    /* URI */
    RUN_TEST(test_uri_https_www);
    RUN_TEST(test_uri_http);
    RUN_TEST(test_uri_tel);
    RUN_TEST(test_uri_none_full);
    RUN_TEST(test_uri_null_returns_zero);
    RUN_TEST(test_uri_null_buffer_returns_zero);
    RUN_TEST(test_uri_buffer_too_small);
    RUN_TEST(test_uri_empty_string);

    /* URI auto-detect */
    RUN_TEST(test_uri_auto_https_www);
    RUN_TEST(test_uri_auto_http_www);
    RUN_TEST(test_uri_auto_https);
    RUN_TEST(test_uri_auto_http);
    RUN_TEST(test_uri_auto_tel);
    RUN_TEST(test_uri_auto_mailto);
    RUN_TEST(test_uri_auto_unknown_protocol);
    RUN_TEST(test_uri_auto_null);

    /* Text */
    RUN_TEST(test_text_basic);
    RUN_TEST(test_text_french);
    RUN_TEST(test_text_null_lang);
    RUN_TEST(test_text_null_text);
    RUN_TEST(test_text_empty);

    /* WiFi */
    RUN_TEST(test_wifi_wpa2);
    RUN_TEST(test_wifi_open);
    RUN_TEST(test_wifi_null_ssid);

    /* Phone */
    RUN_TEST(test_phone_basic);

    /* Pages */
    RUN_TEST(test_pages_exact);
    RUN_TEST(test_pages_rounded);
    RUN_TEST(test_pages_zero);

    /* Cross-format */
    RUN_TEST(test_all_records_start_with_tlv);

    return UNITY_END();
}
