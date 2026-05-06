/* See COPYING.txt for license details. */

/*
 * test_esp32_caps.c
 *
 * Unit tests for the pure-logic helpers in m1_esp32_caps.h:
 *   m1_esp32_caps_parse_payload() — raw bytes → M1_ESP32_CAP_* bitmap + fw_name
 *
 * Both functions are static inlines in the header — zero hardware deps.
 *
 * Tests cover:
 *   - Valid payload with SiN360 cap profile → correct caps and fw_name
 *   - Valid payload with zero cap_bitmap    → zero caps
 *   - Payload too short → parse returns false, outputs unchanged
 *   - Wrong protocol version → parse returns false
 *   - fw_name is always null-terminated (even when payload fills all 32 bytes)
 *   - Payload struct is exactly 41 bytes
 *   - M1_ESP32_CAP_* bit constants do not overlap
 *   - Composite profile macros contain expected M1_ESP32_CAP_* bits
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "m1_esp32_caps.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * Helper: build a well-formed 41-byte status payload
 *
 * Layout (matches m1_esp32_status_payload_t):
 *   [0]      proto_ver
 *   [1-8]    cap_bitmap (uint64_t, LE)
 *   [9-40]   fw_name   (32 bytes, null-terminated)
 * =========================================================================*/

static void make_payload(uint8_t  buf[64],
                         uint8_t  proto_ver,
                         uint64_t cap,
                         const char *fw_name)
{
    memset(buf, 0, 64);
    buf[0] = proto_ver;
    /* cap_bitmap at bytes 1-8, little-endian */
    buf[1] = (uint8_t)(cap        & 0xFFu);
    buf[2] = (uint8_t)((cap >>  8) & 0xFFu);
    buf[3] = (uint8_t)((cap >> 16) & 0xFFu);
    buf[4] = (uint8_t)((cap >> 24) & 0xFFu);
    buf[5] = (uint8_t)((cap >> 32) & 0xFFu);
    buf[6] = (uint8_t)((cap >> 40) & 0xFFu);
    buf[7] = (uint8_t)((cap >> 48) & 0xFFu);
    buf[8] = (uint8_t)((cap >> 56) & 0xFFu);
    /* fw_name at bytes 9-40 */
    if (fw_name)
        strncpy((char *)&buf[9], fw_name, 31);
}

/* =========================================================================
 * Parse: valid payload
 * =========================================================================*/

void test_parse_valid_sin360(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_ESP32_CAP_PROFILE_SIN360,
                 "SiN360-0.9.6");

    uint64_t caps = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &caps, fw_name);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PROFILE_SIN360,
                              caps & M1_ESP32_CAP_PROFILE_SIN360);
    TEST_ASSERT_EQUAL_STRING("SiN360-0.9.6", fw_name);
}

void test_parse_empty_cap_gives_zero_caps(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, UINT64_C(0), "minimal-fw");

    uint64_t caps = UINT64_C(0xDEADBEEF);
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &caps, fw_name);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps);
}

/* =========================================================================
 * Parse: error paths
 * =========================================================================*/

void test_parse_too_short_returns_false(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_ESP32_CAP_WIFI_SCAN, "fw");

    uint64_t caps    = UINT64_C(0xDEADBEEFDEADBEEF);
    char     fw_name[32] = "unchanged";
    /* Pass length one byte short of the required struct */
    uint8_t short_len = (uint8_t)(sizeof(m1_esp32_status_payload_t) - 1u);
    bool ok = m1_esp32_caps_parse_payload(buf, short_len, &caps, fw_name);

    TEST_ASSERT_FALSE(ok);
    /* Outputs must not have been modified */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0xDEADBEEFDEADBEEF), caps);
    TEST_ASSERT_EQUAL_STRING("unchanged", fw_name);
}

void test_parse_zero_length_returns_false(void)
{
    uint8_t buf[64] = {0};
    uint64_t caps   = UINT64_C(0xDEADBEEFDEADBEEF);
    char     fw_name[32] = "unchanged";

    bool ok = m1_esp32_caps_parse_payload(buf, 0, &caps, fw_name);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0xDEADBEEFDEADBEEF), caps);
}

void test_parse_wrong_proto_ver_returns_false(void)
{
    uint8_t buf[64];
    make_payload(buf, (uint8_t)(M1_ESP32_CAPS_PROTO_VER + 1u),
                 M1_ESP32_CAP_WIFI_SCAN, "fw");

    uint64_t caps    = UINT64_C(0xDEADBEEFDEADBEEF);
    char     fw_name[32] = "unchanged";
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &caps, fw_name);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0xDEADBEEFDEADBEEF), caps);
}

/* =========================================================================
 * fw_name null-termination guarantee
 * =========================================================================*/

void test_parse_fw_name_always_null_terminated(void)
{
    uint8_t buf[64];
    /* Fill fw_name field with 32 printable chars (no NUL) */
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, UINT64_C(0), NULL);
    /* bytes 9-39 = 31 'A' chars; byte 40 (last of the 32-byte fw_name) = 'B'
     * This simulates a firmware that forgot the NUL terminator */
    memset(&buf[9], 'A', 31);
    buf[40] = 'B';

    uint64_t caps = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &caps, fw_name);

    TEST_ASSERT_TRUE(ok);
    /* strncpy(dst, src, 31) + forced NUL at [31] must ensure termination */
    TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)fw_name[31]);
}

/* =========================================================================
 * Payload struct size
 * =========================================================================*/

void test_payload_struct_size(void)
{
    /* 1 (proto_ver) + 8 (cap_bitmap) + 32 (fw_name) = 41 */
    TEST_ASSERT_EQUAL_UINT32(41u, (uint32_t)sizeof(m1_esp32_status_payload_t));
}

/* =========================================================================
 * Capability bit constants: no two bits overlap
 * =========================================================================*/

void test_cap_bits_are_unique(void)
{
    const uint64_t caps[] = {
        M1_ESP32_CAP_WIFI_SCAN,
        M1_ESP32_CAP_WIFI_STA_SCAN,
        M1_ESP32_CAP_WIFI_SNIFF,
        M1_ESP32_CAP_WIFI_ATTACK,
        M1_ESP32_CAP_WIFI_NETSCAN,
        M1_ESP32_CAP_WIFI_EVIL_PORTAL,
        M1_ESP32_CAP_WIFI_CONNECT,
        M1_ESP32_CAP_BLE_SCAN,
        M1_ESP32_CAP_BLE_ADV,
        M1_ESP32_CAP_BLE_SPAM,
        M1_ESP32_CAP_BLE_SNIFF,
        M1_ESP32_CAP_BLE_HID,
        M1_ESP32_CAP_BT_MANAGE,
        M1_ESP32_CAP_802154,
    };
    const size_t ncaps = sizeof(caps) / sizeof(caps[0]);

    for (size_t i = 0; i < ncaps; i++)
    {
        for (size_t j = i + 1; j < ncaps; j++)
        {
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(UINT64_C(0), caps[i] & caps[j],
                "Two M1_ESP32_CAP_* bits share a bit position");
        }
    }
}

void test_cap_bits_are_single_bit_powers_of_two(void)
{
    const uint64_t caps[] = {
        M1_ESP32_CAP_WIFI_SCAN,
        M1_ESP32_CAP_WIFI_STA_SCAN,
        M1_ESP32_CAP_WIFI_SNIFF,
        M1_ESP32_CAP_WIFI_ATTACK,
        M1_ESP32_CAP_WIFI_NETSCAN,
        M1_ESP32_CAP_WIFI_EVIL_PORTAL,
        M1_ESP32_CAP_WIFI_CONNECT,
        M1_ESP32_CAP_BLE_SCAN,
        M1_ESP32_CAP_BLE_ADV,
        M1_ESP32_CAP_BLE_SPAM,
        M1_ESP32_CAP_BLE_SNIFF,
        M1_ESP32_CAP_BLE_HID,
        M1_ESP32_CAP_BT_MANAGE,
        M1_ESP32_CAP_802154,
    };
    const size_t ncaps = sizeof(caps) / sizeof(caps[0]);

    for (size_t i = 0; i < ncaps; i++)
    {
        uint64_t c = caps[i];
        TEST_ASSERT_NOT_EQUAL_UINT64_MESSAGE(UINT64_C(0), c, "Cap bit is zero");
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(UINT64_C(0), c & (c - UINT64_C(1)),
            "M1_ESP32_CAP_* bit is not a power of two");
    }
}

/* =========================================================================
 * Composite profile macros: sanity checks
 * =========================================================================*/

void test_sin360_cap_profile_has_expected_caps(void)
{
    const uint64_t p = M1_ESP32_CAP_PROFILE_SIN360;
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_ATTACK);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SPAM);
    /* SiN360 does NOT support WiFi connect or BLE HID or BT manage */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BT_MANAGE);
}

void test_at_bedge117_cap_profile_has_expected_caps(void)
{
    const uint64_t p = M1_ESP32_CAP_PROFILE_AT_BEDGE117;
    /* AT firmware: Bad-BT (BLE HID) and 802.15.4 work via AT commands */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_802154);
    /* bedge117 AT does NOT expose binary-SPI-only features */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SPAM);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BT_MANAGE);
}

void test_at_neddy299_cap_profile_has_expected_caps(void)
{
    const uint64_t p = M1_ESP32_CAP_PROFILE_AT_NEDDY299;
    /* neddy299 is a superset of bedge117 */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_802154);
    /* neddy299 adds station scan and WiFi attacks (AT+STASCAN / AT+DEAUTH) */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_STA_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_ATTACK);
    /* Still no binary-SPI-only features */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BT_MANAGE);
}

void test_at_neddy299_is_superset_of_bedge117(void)
{
    const uint64_t base  = M1_ESP32_CAP_PROFILE_AT_BEDGE117;
    const uint64_t neddy = M1_ESP32_CAP_PROFILE_AT_NEDDY299;
    TEST_ASSERT_EQUAL_UINT64(base, neddy & base);
}

void test_at_and_sin360_profiles_are_disjoint(void)
{
    /* AT bedge117 and SiN360 fallback profiles share no bits — they represent
     * completely different firmware capabilities */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0),
        M1_ESP32_CAP_PROFILE_AT_BEDGE117 & M1_ESP32_CAP_PROFILE_SIN360);
}

void test_at_neddy299_shares_wifi_bits_with_sin360(void)
{
    /* neddy299 adds WIFI_STA_SCAN and WIFI_ATTACK (via AT+STASCAN / AT+DEAUTH).
     * SiN360 also supports these features via binary SPI.  The two profiles
     * intentionally share those bits — this is expected, not a collision. */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0),
        M1_ESP32_CAP_PROFILE_AT_NEDDY299 & M1_ESP32_CAP_WIFI_STA_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0),
        M1_ESP32_CAP_PROFILE_AT_NEDDY299 & M1_ESP32_CAP_WIFI_ATTACK);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0),
        M1_ESP32_CAP_PROFILE_SIN360 & M1_ESP32_CAP_WIFI_STA_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0),
        M1_ESP32_CAP_PROFILE_SIN360 & M1_ESP32_CAP_WIFI_ATTACK);
}

/* =========================================================================
 * AT hex response parser: m1_esp32_caps_parse_at_hex()
 * =========================================================================*/

/* Helper: build a "+GETSTATUSHEX:<hex>\r\nOK\r\n" response string from
 * the same payload produced by make_payload().  hex_out must be at least
 * 82+1 bytes.  resp_out must be at least strlen("+GETSTATUSHEX:") + 82 + 8. */
static void make_at_hex_resp(char *resp_out, size_t resp_sz,
                              uint8_t proto_ver, uint64_t cap, const char *fw_name)
{
    uint8_t buf[64];
    char    hex[83] = {0};
    make_payload(buf, proto_ver, cap, fw_name);
    for (int i = 0; i < (int)sizeof(m1_esp32_status_payload_t); i++)
        snprintf(&hex[i * 2], 3, "%02X", buf[i]);
    snprintf(resp_out, resp_sz, "+GETSTATUSHEX:%s\r\n\r\nOK\r\n", hex);
}

void test_at_hex_parse_valid_sin360(void)
{
    char resp[200];
    make_at_hex_resp(resp, sizeof(resp),
                     M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_PROFILE_SIN360,
                     "SiN360-0.9.6");

    uint8_t decoded[sizeof(m1_esp32_status_payload_t)];
    bool ok = m1_esp32_caps_parse_at_hex(resp, decoded,
                                         (uint8_t)sizeof(decoded));
    TEST_ASSERT_TRUE(ok);

    /* Now parse the decoded bytes */
    uint64_t caps = 0;
    char     fw_name[32];
    bool ok2 = m1_esp32_caps_parse_payload(decoded,
                                           (uint8_t)sizeof(m1_esp32_status_payload_t),
                                           &caps, fw_name);
    TEST_ASSERT_TRUE(ok2);
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PROFILE_SIN360,
                              caps & M1_ESP32_CAP_PROFILE_SIN360);
    TEST_ASSERT_EQUAL_STRING("SiN360-0.9.6", fw_name);
}

void test_at_hex_parse_valid_at_bedge117(void)
{
    char resp[200];
    make_at_hex_resp(resp, sizeof(resp),
                     M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_PROFILE_AT_BEDGE117,
                     "AT-bedge117-2.0.2");

    uint8_t decoded[sizeof(m1_esp32_status_payload_t)];
    bool ok = m1_esp32_caps_parse_at_hex(resp, decoded,
                                         (uint8_t)sizeof(decoded));
    TEST_ASSERT_TRUE(ok);

    uint64_t caps = 0;
    char     fw_name[32];
    bool ok2 = m1_esp32_caps_parse_payload(decoded,
                                           (uint8_t)sizeof(m1_esp32_status_payload_t),
                                           &caps, fw_name);
    TEST_ASSERT_TRUE(ok2);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_802154);
    TEST_ASSERT_EQUAL_STRING("AT-bedge117-2.0.2", fw_name);
}

void test_at_hex_parse_no_prefix_returns_false(void)
{
    /* Response without the "+GETSTATUSHEX:" prefix */
    const char *bad = "\r\nOK\r\n";
    uint8_t decoded[sizeof(m1_esp32_status_payload_t)] = {0};
    bool ok = m1_esp32_caps_parse_at_hex(bad, decoded, (uint8_t)sizeof(decoded));
    TEST_ASSERT_FALSE(ok);
}

void test_at_hex_parse_truncated_hex_returns_false(void)
{
    /* Prefix present but hex string is too short (only 10 chars, not 82) */
    const char *bad = "+GETSTATUSHEX:0102030405\r\nOK\r\n";
    uint8_t decoded[sizeof(m1_esp32_status_payload_t)] = {0};
    bool ok = m1_esp32_caps_parse_at_hex(bad, decoded, (uint8_t)sizeof(decoded));
    TEST_ASSERT_FALSE(ok);
}

void test_at_hex_parse_invalid_hex_char_returns_false(void)
{
    /* Build a valid response then corrupt one hex character */
    char resp[200];
    make_at_hex_resp(resp, sizeof(resp),
                     M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_WIFI_SCAN,
                     "test-fw");
    /* Corrupt the 5th hex char (position 14 + 4 = 18) to a non-hex char */
    resp[18] = 'G';

    uint8_t decoded[sizeof(m1_esp32_status_payload_t)] = {0};
    bool ok = m1_esp32_caps_parse_at_hex(resp, decoded, (uint8_t)sizeof(decoded));
    TEST_ASSERT_FALSE(ok);
}

void test_at_hex_parse_lowercase_hex_accepted(void)
{
    /* Build response, convert hex to lowercase, verify still parsed */
    char resp[200];
    make_at_hex_resp(resp, sizeof(resp),
                     M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_BLE_HID,
                     "dag-fw");
    /* Convert hex portion to lowercase */
    const size_t pfx_len = 14u;  /* strlen("+GETSTATUSHEX:") */
    for (size_t i = pfx_len; i < pfx_len + 82u && resp[i]; i++)
        if (resp[i] >= 'A' && resp[i] <= 'F')
            resp[i] = (char)(resp[i] - 'A' + 'a');

    uint8_t decoded[sizeof(m1_esp32_status_payload_t)];
    bool ok = m1_esp32_caps_parse_at_hex(resp, decoded, (uint8_t)sizeof(decoded));
    TEST_ASSERT_TRUE(ok);

    uint64_t caps = 0;
    char     fw_name[32];
    bool ok2 = m1_esp32_caps_parse_payload(decoded,
                                           (uint8_t)sizeof(m1_esp32_status_payload_t),
                                           &caps, fw_name);
    TEST_ASSERT_TRUE(ok2);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_HID);
}

void test_at_hex_parse_prefix_with_leading_content(void)
{
    /* Prefix may appear after some leading content in the response */
    char resp[200];
    snprintf(resp, sizeof(resp), "\r\nsome preamble\r\n");
    const size_t pfx_offset = strlen(resp);

    char hex_resp[200];
    make_at_hex_resp(hex_resp, sizeof(hex_resp),
                     M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_WIFI_SCAN, "fw");
    /* Append to the preamble */
    strncat(resp, hex_resp, sizeof(resp) - pfx_offset - 1u);

    uint8_t decoded[sizeof(m1_esp32_status_payload_t)];
    bool ok = m1_esp32_caps_parse_at_hex(resp, decoded, (uint8_t)sizeof(decoded));
    TEST_ASSERT_TRUE(ok);
}

void test_at_hex_parse_small_output_buffer_returns_false(void)
{
    char resp[200];
    make_at_hex_resp(resp, sizeof(resp),
                     M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_WIFI_SCAN, "fw");

    /* Output buffer one byte too small */
    uint8_t decoded[40];  /* sizeof(m1_esp32_status_payload_t) - 1 == 40 */
    bool ok = m1_esp32_caps_parse_at_hex(resp, decoded, (uint8_t)sizeof(decoded));
    TEST_ASSERT_FALSE(ok);
}

/* =========================================================================
 * main
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Parse: valid payload */
    RUN_TEST(test_parse_valid_sin360);
    RUN_TEST(test_parse_empty_cap_gives_zero_caps);

    /* Parse: error paths */
    RUN_TEST(test_parse_too_short_returns_false);
    RUN_TEST(test_parse_zero_length_returns_false);
    RUN_TEST(test_parse_wrong_proto_ver_returns_false);

    /* fw_name */
    RUN_TEST(test_parse_fw_name_always_null_terminated);

    /* Struct size */
    RUN_TEST(test_payload_struct_size);

    /* Bit uniqueness */
    RUN_TEST(test_cap_bits_are_unique);
    RUN_TEST(test_cap_bits_are_single_bit_powers_of_two);

    /* Profile macros */
    RUN_TEST(test_sin360_cap_profile_has_expected_caps);
    RUN_TEST(test_at_bedge117_cap_profile_has_expected_caps);
    RUN_TEST(test_at_neddy299_cap_profile_has_expected_caps);
    RUN_TEST(test_at_neddy299_is_superset_of_bedge117);
    RUN_TEST(test_at_and_sin360_profiles_are_disjoint);
    RUN_TEST(test_at_neddy299_shares_wifi_bits_with_sin360);

    /* AT hex response parser */
    RUN_TEST(test_at_hex_parse_valid_sin360);
    RUN_TEST(test_at_hex_parse_valid_at_bedge117);
    RUN_TEST(test_at_hex_parse_no_prefix_returns_false);
    RUN_TEST(test_at_hex_parse_truncated_hex_returns_false);
    RUN_TEST(test_at_hex_parse_invalid_hex_char_returns_false);
    RUN_TEST(test_at_hex_parse_lowercase_hex_accepted);
    RUN_TEST(test_at_hex_parse_prefix_with_leading_content);
    RUN_TEST(test_at_hex_parse_small_output_buffer_returns_false);

    return UNITY_END();
}
