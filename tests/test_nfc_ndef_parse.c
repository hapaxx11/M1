/* See COPYING.txt for license details. */

/*
 * test_nfc_ndef_parse.c
 *
 * Host-side unit tests for nfc_ndef_parse.c — NDEF record parsing
 * for URI and Text records.
 */

#include "unity.h"
#include "nfc_ndef_parse.h"
#include <string.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

/*══════════════════ Helpers ══════════════════*/

/*
 * Build a single short-record NDEF message in-place.
 * Returns total NDEF message length (record headers + payload).
 *
 * Flags: MB=1, ME=1, SR=1, TNF=0x01 → header = 0xD1
 */
static uint16_t build_sr_record(uint8_t *buf, uint8_t type_byte,
                                const uint8_t *payload, uint8_t payload_len)
{
    uint16_t pos = 0;
    buf[pos++] = 0xD1;           /* MB|ME|SR|TNF=WKT */
    buf[pos++] = 0x01;           /* type length = 1 */
    buf[pos++] = payload_len;    /* payload length (SR) */
    buf[pos++] = type_byte;      /* type */
    memcpy(&buf[pos], payload, payload_len);
    pos += payload_len;
    return pos;
}

/*══════════════════ NULL / edge cases ══════════════════*/

void test_null_inputs(void)
{
    char out[64];
    TEST_ASSERT_EQUAL_UINT16(0, ndef_parse_records(NULL, 10, out, sizeof(out)));
    uint8_t dummy = 0;
    TEST_ASSERT_EQUAL_UINT16(0, ndef_parse_records(&dummy, 10, NULL, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT16(0, ndef_parse_records(&dummy, 10, out, 0));
}

void test_empty_message(void)
{
    char out[64] = "dirty";
    uint8_t msg[1] = {0};
    TEST_ASSERT_EQUAL_UINT16(0, ndef_parse_records(msg, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_truncated_header(void)
{
    char out[64];
    uint8_t msg[2] = {0xD1, 0x01}; /* Missing payload_len byte */
    TEST_ASSERT_EQUAL_UINT16(0, ndef_parse_records(msg, 2, out, sizeof(out)));
}

/*══════════════════ URI Records ══════════════════*/

void test_uri_no_prefix(void)
{
    /* prefix code 0x00 = no prefix, payload = "example.com" */
    uint8_t payload[12];
    payload[0] = 0x00; /* no prefix */
    memcpy(&payload[1], "example.com", 11);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 12);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("example.com", out);
    TEST_ASSERT_EQUAL_UINT16(11, n);
}

void test_uri_https_www(void)
{
    /* prefix code 0x02 = "https://www." */
    uint8_t payload[12];
    payload[0] = 0x02;
    memcpy(&payload[1], "example.com", 11);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 12);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("https://www.example.com", out);
    TEST_ASSERT_EQUAL_UINT16(23, n);
}

void test_uri_https(void)
{
    /* prefix code 0x04 = "https://" */
    uint8_t payload[12];
    payload[0] = 0x04;
    memcpy(&payload[1], "example.com", 11);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 12);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("https://example.com", out);
    TEST_ASSERT_EQUAL_UINT16(19, n);
}

void test_uri_tel(void)
{
    /* prefix code 0x05 = "tel:" */
    uint8_t payload[14];
    payload[0] = 0x05;
    memcpy(&payload[1], "+1234567890", 11);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 12);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("tel:+1234567890", out);
    TEST_ASSERT_EQUAL_UINT16(15, n);
}

void test_uri_mailto(void)
{
    /* prefix code 0x06 = "mailto:" */
    uint8_t payload[16];
    payload[0] = 0x06;
    memcpy(&payload[1], "user@test.com", 13);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 14);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("mailto:user@test.com", out);
    TEST_ASSERT_EQUAL_UINT16(20, n);
}

void test_uri_unknown_prefix(void)
{
    /* prefix code 0x30 = out of range, should use "" */
    uint8_t payload[12];
    payload[0] = 0x30;
    memcpy(&payload[1], "example.com", 11);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 12);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("example.com", out);
    TEST_ASSERT_EQUAL_UINT16(11, n);
}

void test_uri_http_www(void)
{
    /* prefix code 0x01 = "http://www." */
    uint8_t payload[10];
    payload[0] = 0x01;
    memcpy(&payload[1], "test.org", 8);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 9);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("http://www.test.org", out);
    TEST_ASSERT_EQUAL_UINT16(19, n);
}

/*══════════════════ Text Records ══════════════════*/

void test_text_en(void)
{
    /* Status=0x02 (UTF-8, lang_len=2), lang="en", text="Hello" */
    uint8_t payload[8];
    payload[0] = 0x02;           /* UTF-8, lang_len=2 */
    payload[1] = 'e';
    payload[2] = 'n';
    memcpy(&payload[3], "Hello", 5);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'T', payload, 8);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello", out);
    TEST_ASSERT_EQUAL_UINT16(5, n);
}

void test_text_fr(void)
{
    /* Status=0x02, lang="fr", text="Bonjour" */
    uint8_t payload[10];
    payload[0] = 0x02;
    payload[1] = 'f';
    payload[2] = 'r';
    memcpy(&payload[3], "Bonjour", 7);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'T', payload, 10);

    char out[128];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Bonjour", out);
    TEST_ASSERT_EQUAL_UINT16(7, n);
}

void test_text_empty_after_lang(void)
{
    /* Status=0x02 (lang_len=2), lang="en", text="" — payload exactly 3 bytes */
    uint8_t payload[3] = {0x02, 'e', 'n'};

    uint8_t msg[16];
    uint16_t mlen = build_sr_record(msg, 'T', payload, 3);

    char out[64];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    /* text_start(3) == payload_len(3) → no text output */
    TEST_ASSERT_EQUAL_UINT16(0, n);
}

/*══════════════════ Multi-record ══════════════════*/

void test_multi_uri_and_text(void)
{
    uint8_t msg[64];
    uint16_t pos = 0;

    /* Record 1: URI (MB=1, ME=0) — not last */
    uint8_t uri_payload[8];
    uri_payload[0] = 0x04; /* https:// */
    memcpy(&uri_payload[1], "m1.dev", 6);
    msg[pos++] = 0x91;         /* MB=1, ME=0, SR=1, TNF=WKT */
    msg[pos++] = 0x01;         /* type_len = 1 */
    msg[pos++] = 7;            /* payload_len */
    msg[pos++] = 'U';
    memcpy(&msg[pos], uri_payload, 7);
    pos += 7;

    /* Record 2: Text (MB=0, ME=1) — last */
    uint8_t text_payload[10];
    text_payload[0] = 0x02;    /* UTF-8, lang_len=2 */
    text_payload[1] = 'e';
    text_payload[2] = 'n';
    memcpy(&text_payload[3], "Welcome", 7);
    msg[pos++] = 0x51;         /* MB=0, ME=1, SR=1, TNF=WKT */
    msg[pos++] = 0x01;
    msg[pos++] = 10;
    msg[pos++] = 'T';
    memcpy(&msg[pos], text_payload, 10);
    pos += 10;

    char out[128];
    uint16_t n = ndef_parse_records(msg, pos, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("https://m1.dev\nWelcome", out);
    TEST_ASSERT_EQUAL_UINT16(22, n);
}

/*══════════════════ Output truncation ══════════════════*/

void test_output_truncation(void)
{
    uint8_t payload[12];
    payload[0] = 0x04; /* https:// */
    memcpy(&payload[1], "example.com", 11);

    uint8_t msg[32];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 12);

    /* "https://example.com" = 19 chars, buffer of 10 → truncated */
    char out[10];
    uint16_t n = ndef_parse_records(msg, mlen, out, sizeof(out));
    TEST_ASSERT_LESS_THAN(10, n);
    TEST_ASSERT_EQUAL_UINT8('\0', out[n]);
}

void test_output_buffer_size_1(void)
{
    uint8_t payload[4] = {0x04, 'a', 'b', 'c'};
    uint8_t msg[16];
    uint16_t mlen = build_sr_record(msg, 'U', payload, 4);

    char out[1] = {'x'};
    uint16_t n = ndef_parse_records(msg, mlen, out, 1);
    TEST_ASSERT_EQUAL_UINT16(0, n);
    TEST_ASSERT_EQUAL_UINT8('\0', out[0]);
}

/*══════════════════ Skipped record types ══════════════════*/

void test_unknown_tnf_skipped(void)
{
    /* TNF=0x04 (external), should be skipped */
    uint8_t msg[16];
    uint16_t pos = 0;
    msg[pos++] = 0xD4;         /* MB=1, ME=1, SR=1, TNF=0x04 */
    msg[pos++] = 0x03;         /* type_len = 3 */
    msg[pos++] = 0x02;         /* payload_len = 2 */
    msg[pos++] = 'a';
    msg[pos++] = 'b';
    msg[pos++] = 'c';
    msg[pos++] = 0xDE;
    msg[pos++] = 0xAD;

    char out[64];
    uint16_t n = ndef_parse_records(msg, pos, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT16(0, n);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_uri_payload_len_zero(void)
{
    /* URI record with payload_len=0 — should be skipped */
    uint8_t msg[8];
    uint16_t pos = 0;
    msg[pos++] = 0xD1;
    msg[pos++] = 0x01;
    msg[pos++] = 0x00; /* payload_len = 0 */
    msg[pos++] = 'U';

    char out[64];
    uint16_t n = ndef_parse_records(msg, pos, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT16(0, n);
}

/*══════════════════ Long record (non-SR) ══════════════════*/

void test_long_record_format(void)
{
    /* Non-SR URI record: 4-byte payload length */
    uint8_t msg[64];
    uint16_t pos = 0;
    msg[pos++] = 0xC1;         /* MB=1, ME=1, SR=0, TNF=WKT */
    msg[pos++] = 0x01;         /* type_len = 1 */
    msg[pos++] = 0x00;         /* payload_len MSB */
    msg[pos++] = 0x00;
    msg[pos++] = 0x00;
    msg[pos++] = 0x08;         /* payload_len = 8 */
    msg[pos++] = 'U';          /* type */
    msg[pos]   = 0x04;         /* https:// */
    memcpy(&msg[pos + 1], "abc.com", 7);
    pos += 8;

    char out[64];
    uint16_t n = ndef_parse_records(msg, pos, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("https://abc.com", out);
    TEST_ASSERT_EQUAL_UINT16(15, n);
}

/*══════════════════ ID Length flag ══════════════════*/

void test_il_flag_skips_id(void)
{
    /* SR record with IL=1, id_len=2 */
    uint8_t msg[32];
    uint16_t pos = 0;
    msg[pos++] = 0xD9;         /* MB=1, ME=1, SR=1, IL=1, TNF=WKT */
    msg[pos++] = 0x01;         /* type_len = 1 */
    msg[pos++] = 0x07;         /* payload_len = 7 */
    msg[pos++] = 0x02;         /* id_len = 2 */
    msg[pos++] = 'U';          /* type */
    msg[pos++] = 'I';          /* id byte 1 */
    msg[pos++] = 'D';          /* id byte 2 */
    msg[pos]   = 0x04;         /* prefix: https:// */
    memcpy(&msg[pos + 1], "x.com", 5);
    msg[pos + 6] = 'z';       /* extra byte in payload */
    pos += 7;

    char out[64];
    uint16_t n = ndef_parse_records(msg, pos, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("https://x.comz", out);
    TEST_ASSERT_EQUAL_UINT16(14, n);
}

/*══════════════════ Roundtrip with nfc_ndef_encode ══════════════════*/

/* If nfc_ndef_encode.h is available, test encode→parse roundtrip */
#include "nfc_ndef_encode.h"

void test_roundtrip_uri(void)
{
    uint8_t ndef_buf[128];
    size_t ndef_len = ndef_encode_uri(ndef_buf, sizeof(ndef_buf),
                                      NDEF_URI_HTTPS, "github.com/hapaxx11/M1");
    TEST_ASSERT_GREATER_THAN(0, ndef_len);

    /* ndef_encode_uri produces: [0x03][TLV_len][record...][0xFE]
     * ndef_parse_records expects raw record bytes (after TLV header).
     * Skip TLV type (0x03) + TLV length (1 byte for short),
     * and exclude terminator (0xFE). */
    uint8_t tlv_hdr_len = 2; /* 0x03 + 1-byte length */
    const uint8_t *record = &ndef_buf[tlv_hdr_len];
    uint16_t record_len = (uint16_t)(ndef_len - tlv_hdr_len - 1); /* -1 for 0xFE */

    char out[128];
    uint16_t n = ndef_parse_records(record, record_len, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("https://github.com/hapaxx11/M1", out);
    TEST_ASSERT_EQUAL_UINT16(30, n);
}

void test_roundtrip_text(void)
{
    uint8_t ndef_buf[128];
    size_t ndef_len = ndef_encode_text(ndef_buf, sizeof(ndef_buf),
                                       "en", "Hello World");
    TEST_ASSERT_GREATER_THAN(0, ndef_len);

    uint8_t tlv_hdr_len = 2;
    const uint8_t *record = &ndef_buf[tlv_hdr_len];
    uint16_t record_len = (uint16_t)(ndef_len - tlv_hdr_len - 1);

    char out[128];
    uint16_t n = ndef_parse_records(record, record_len, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello World", out);
    TEST_ASSERT_EQUAL_UINT16(11, n);
}

/*══════════════════ Runner ══════════════════*/

int main(void)
{
    UNITY_BEGIN();

    /* Null / edge cases */
    RUN_TEST(test_null_inputs);
    RUN_TEST(test_empty_message);
    RUN_TEST(test_truncated_header);

    /* URI records */
    RUN_TEST(test_uri_no_prefix);
    RUN_TEST(test_uri_https_www);
    RUN_TEST(test_uri_https);
    RUN_TEST(test_uri_tel);
    RUN_TEST(test_uri_mailto);
    RUN_TEST(test_uri_unknown_prefix);
    RUN_TEST(test_uri_http_www);

    /* Text records */
    RUN_TEST(test_text_en);
    RUN_TEST(test_text_fr);
    RUN_TEST(test_text_empty_after_lang);

    /* Multi-record */
    RUN_TEST(test_multi_uri_and_text);

    /* Truncation */
    RUN_TEST(test_output_truncation);
    RUN_TEST(test_output_buffer_size_1);

    /* Skipped types */
    RUN_TEST(test_unknown_tnf_skipped);
    RUN_TEST(test_uri_payload_len_zero);

    /* Long record / IL flag */
    RUN_TEST(test_long_record_format);
    RUN_TEST(test_il_flag_skips_id);

    /* Roundtrip */
    RUN_TEST(test_roundtrip_uri);
    RUN_TEST(test_roundtrip_text);

    return UNITY_END();
}
