/* See COPYING.txt for license details. */

/*
 * test_esp32_caps.c
 *
 * Unit tests for the pure-logic helpers in m1_esp32_caps.h:
 *   m1_esp32_caps_parse_payload() — raw bytes → M1_ESP32_CAP_* bitmap + fw_name
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
 * M1_ESP32_FALLBACK_* constant sanity checks
 * =========================================================================*/

void test_fallback_constants_are_nonzero(void)
{
    TEST_ASSERT_GREATER_THAN(0u, M1_ESP32_FALLBACK_BSS_SIN360);
    TEST_ASSERT_GREATER_THAN(0u, M1_ESP32_FALLBACK_HEAP_SIN360);
    TEST_ASSERT_GREATER_THAN(0u, M1_ESP32_FALLBACK_BSS_AT);
    TEST_ASSERT_GREATER_THAN(0u, M1_ESP32_FALLBACK_HEAP_AT);
}

void test_fallback_at_bss_exceeds_sin360(void)
{
    /* AT firmware (bedge117) has larger BSS than SiN360:
     * full AT infrastructure + SPI ring buffers vs. NimBLE-only. */
    TEST_ASSERT_GREATER_THAN(M1_ESP32_FALLBACK_BSS_SIN360,
                             M1_ESP32_FALLBACK_BSS_AT);
}

void test_fallback_sin360_heap_exceeds_at(void)
{
    /* SiN360 has more free heap than AT firmware:
     * NimBLE uses less runtime RAM than the full AT stack + BLE HID. */
    TEST_ASSERT_GREATER_THAN(M1_ESP32_FALLBACK_HEAP_AT,
                             M1_ESP32_FALLBACK_HEAP_SIN360);
}


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
        M1_ESP32_CAP_STA_SCAN,
        M1_ESP32_CAP_BLE_SCAN,
        M1_ESP32_CAP_BLE_ADV,
        M1_ESP32_CAP_DEAUTH,
        M1_ESP32_CAP_BEACON,
        M1_ESP32_CAP_PROBE_FLOOD,
        M1_ESP32_CAP_KARMA,
        M1_ESP32_CAP_PKTMON,
        M1_ESP32_CAP_PORTAL,
        M1_ESP32_CAP_WIFI_JOIN,
        M1_ESP32_CAP_WIFI_SET_MAC,
        M1_ESP32_CAP_WIFI_SET_CHAN,
        M1_ESP32_CAP_NETSCAN,
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
        M1_ESP32_CAP_STA_SCAN,
        M1_ESP32_CAP_BLE_SCAN,
        M1_ESP32_CAP_BLE_ADV,
        M1_ESP32_CAP_DEAUTH,
        M1_ESP32_CAP_BEACON,
        M1_ESP32_CAP_PROBE_FLOOD,
        M1_ESP32_CAP_KARMA,
        M1_ESP32_CAP_PKTMON,
        M1_ESP32_CAP_PORTAL,
        M1_ESP32_CAP_WIFI_JOIN,
        M1_ESP32_CAP_WIFI_SET_MAC,
        M1_ESP32_CAP_WIFI_SET_CHAN,
        M1_ESP32_CAP_NETSCAN,
        M1_ESP32_CAP_BLE_HID,
        M1_ESP32_CAP_BT_MANAGE,
        M1_ESP32_CAP_802154,
    };
    const size_t ncaps = sizeof(caps) / sizeof(caps[0]);

    for (size_t i = 0; i < ncaps; i++)
    {
        uint64_t c = caps[i];
        TEST_ASSERT_NOT_EQUAL_UINT64_MESSAGE(UINT64_C(0), c, "Cmd bit is zero");
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(UINT64_C(0), c & (c - UINT64_C(1)),
            "M1_ESP32_CAP_* bit is not a power of two");
    }
}

/* =========================================================================
 * Composite profile macros: sanity checks
 * =========================================================================*/

void test_sin360_profile_has_expected_caps(void)
{
    const uint64_t p = M1_ESP32_CAP_PROFILE_SIN360;
    /* SiN360 profile includes all scan/attack/sniffer capabilities */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_STA_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_ADV);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_DEAUTH);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BEACON);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PROBE_FLOOD);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_KARMA);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PKTMON);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PORTAL);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_NETSCAN);
    /* SiN360 profile does NOT include BLE HID, BT manage, 802.15.4, or WiFi join */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_JOIN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BT_MANAGE);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_802154);
}

/* =========================================================================
 * AT+CMD? response parser: m1_esp32_caps_parse_at_cmd_list()
 *
 * The runtime AT secondary probe issues `AT+CMD?` to the connected ESP32 and
 * parses the response against a small (at_cmd_name → cap_bit) mapping table.
 * These tests use a representative mapping that mirrors the production
 * s_at_cmd_cap_map[] in m1_esp32_caps.c.
 * =========================================================================*/

/* AT command name → capability bit mapping used by the parser tests.
 *
 * NOTE: This is intentionally a separate copy of the production table in
 * `m1_csrc/m1_esp32_caps.c` (`s_at_cmd_cap_map[]`) — the test deliberately
 * does not link in the production .c file (it would pull in FreeRTOS / HAL
 * headers and the SPI transport).  When the production table changes, this
 * test table must be updated in lockstep to keep the AT+CMD? parser
 * coverage in sync with the firmware behaviour. */
static const m1_esp32_at_cmd_cap_entry_t k_test_at_cmd_map[] = {
    { "AT+CWJAP",      M1_ESP32_CAP_WIFI_JOIN },
    { "AT+BLEHIDINIT", M1_ESP32_CAP_BLE_HID   },
    { "AT+ZIGSNIFF",   M1_ESP32_CAP_802154    },
    { "AT+DEAUTH",     M1_ESP32_CAP_DEAUTH    },
    { "AT+STASCAN",    M1_ESP32_CAP_STA_SCAN  },
};
static const size_t k_test_at_cmd_map_n =
    sizeof(k_test_at_cmd_map) / sizeof(k_test_at_cmd_map[0]);

/* Stock ESP-AT AT+CMD? response excerpt (alphabetical, abbreviated). */
static const char *k_resp_stock_at =
    "+CMD:0,\"AT\",0,0,0,1\r\n"
    "+CMD:1,\"AT+CWJAP\",1,1,1,1\r\n"
    "+CMD:2,\"AT+CWMODE\",1,1,1,0\r\n"
    "+CMD:3,\"AT+GMR\",0,0,0,1\r\n"
    "+CMD:4,\"AT+RST\",0,0,0,1\r\n"
    "\r\nOK\r\n";

/* bedge117/dag custom AT firmware: stock + BLE HID + 802.15.4 */
static const char *k_resp_bedge_dag =
    "+CMD:0,\"AT\",0,0,0,1\r\n"
    "+CMD:1,\"AT+CWJAP\",1,1,1,1\r\n"
    "+CMD:2,\"AT+BLEHIDINIT\",1,1,1,1\r\n"
    "+CMD:3,\"AT+ZIGSNIFF\",1,1,1,0\r\n"
    "\r\nOK\r\n";

/* neddy299 / dag-deauth: stock + BLE HID + 802.15.4 + deauth + stascan */
static const char *k_resp_neddy299 =
    "+CMD:0,\"AT\",0,0,0,1\r\n"
    "+CMD:1,\"AT+CWJAP\",1,1,1,1\r\n"
    "+CMD:2,\"AT+BLEHIDINIT\",1,1,1,1\r\n"
    "+CMD:3,\"AT+ZIGSNIFF\",1,1,1,0\r\n"
    "+CMD:4,\"AT+DEAUTH\",1,1,1,0\r\n"
    "+CMD:5,\"AT+STASCAN\",1,1,1,0\r\n"
    "\r\nOK\r\n";

void test_at_cmd_parse_stock_at_only_wifi_join(void)
{
    uint64_t caps = m1_esp32_caps_parse_at_cmd_list(
        k_resp_stock_at, k_test_at_cmd_map, k_test_at_cmd_map_n);

    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_WIFI_JOIN, caps);
}

void test_at_cmd_parse_bedge_dag_caps(void)
{
    const uint64_t expected = M1_ESP32_CAP_WIFI_JOIN |
                              M1_ESP32_CAP_BLE_HID  |
                              M1_ESP32_CAP_802154;
    uint64_t caps = m1_esp32_caps_parse_at_cmd_list(
        k_resp_bedge_dag, k_test_at_cmd_map, k_test_at_cmd_map_n);

    TEST_ASSERT_EQUAL_UINT64(expected, caps);
}

void test_at_cmd_parse_neddy299_caps(void)
{
    const uint64_t expected = M1_ESP32_CAP_WIFI_JOIN |
                              M1_ESP32_CAP_BLE_HID  |
                              M1_ESP32_CAP_802154   |
                              M1_ESP32_CAP_DEAUTH   |
                              M1_ESP32_CAP_STA_SCAN;
    uint64_t caps = m1_esp32_caps_parse_at_cmd_list(
        k_resp_neddy299, k_test_at_cmd_map, k_test_at_cmd_map_n);

    TEST_ASSERT_EQUAL_UINT64(expected, caps);
}

void test_at_cmd_parse_empty_response_returns_zero(void)
{
    uint64_t caps = m1_esp32_caps_parse_at_cmd_list(
        "", k_test_at_cmd_map, k_test_at_cmd_map_n);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps);
}

void test_at_cmd_parse_null_inputs_return_zero(void)
{
    TEST_ASSERT_EQUAL_UINT64(
        UINT64_C(0),
        m1_esp32_caps_parse_at_cmd_list(NULL, k_test_at_cmd_map, k_test_at_cmd_map_n));

    TEST_ASSERT_EQUAL_UINT64(
        UINT64_C(0),
        m1_esp32_caps_parse_at_cmd_list(k_resp_stock_at, NULL, 0));
}

void test_at_cmd_parse_requires_quoted_match(void)
{
    /* A name appearing as a prefix of another command must not match.
     * "AT+CWJAP" is a substring of "AT+CWJAPCFG" / "AT+CWJAPSOMETHING"
     * but only the exact quoted form should count. */
    const char *resp =
        "+CMD:0,\"AT+CWJAPCFG\",1,1,1,0\r\n"
        "\r\nOK\r\n";

    uint64_t caps = m1_esp32_caps_parse_at_cmd_list(
        resp, k_test_at_cmd_map, k_test_at_cmd_map_n);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps);
}

void test_at_cmd_parse_substring_in_text_does_not_match(void)
{
    /* If the command appears unquoted (e.g. in a comment-like line), it
     * must not match because the parser anchors on the surrounding quotes. */
    const char *resp =
        "+CMD:0,\"AT\",0,0,0,1\r\n"
        "Some leading line mentioning AT+DEAUTH without quotes\r\n"
        "\r\nOK\r\n";

    uint64_t caps = m1_esp32_caps_parse_at_cmd_list(
        resp, k_test_at_cmd_map, k_test_at_cmd_map_n);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps);
}

void test_at_cmd_response_valid_detector(void)
{
    /* A real AT+CMD? response always contains at least one "+CMD:" line. */
    TEST_ASSERT_TRUE(m1_esp32_caps_at_cmd_response_valid(k_resp_stock_at));
    TEST_ASSERT_TRUE(m1_esp32_caps_at_cmd_response_valid(k_resp_neddy299));

    /* Plain ERROR / OK responses or NULL should be rejected. */
    TEST_ASSERT_FALSE(m1_esp32_caps_at_cmd_response_valid("\r\nERROR\r\n"));
    TEST_ASSERT_FALSE(m1_esp32_caps_at_cmd_response_valid("\r\nOK\r\n"));
    TEST_ASSERT_FALSE(m1_esp32_caps_at_cmd_response_valid(""));
    TEST_ASSERT_FALSE(m1_esp32_caps_at_cmd_response_valid(NULL));
}

void test_at_cmd_parse_arbitrary_line_order(void)
{
    /* The probe must work regardless of ordering in the response. */
    const char *resp =
        "+CMD:42,\"AT+STASCAN\",1,1,1,0\r\n"
        "+CMD:11,\"AT+ZIGSNIFF\",1,1,1,0\r\n"
        "+CMD:7,\"AT+CWJAP\",1,1,1,1\r\n"
        "\r\nOK\r\n";

    const uint64_t expected = M1_ESP32_CAP_STA_SCAN |
                              M1_ESP32_CAP_802154   |
                              M1_ESP32_CAP_WIFI_JOIN;
    uint64_t caps = m1_esp32_caps_parse_at_cmd_list(
        resp, k_test_at_cmd_map, k_test_at_cmd_map_n);
    TEST_ASSERT_EQUAL_UINT64(expected, caps);
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
    RUN_TEST(test_sin360_profile_has_expected_caps);

    /* AT+CMD? response parser */
    RUN_TEST(test_at_cmd_parse_stock_at_only_wifi_join);
    RUN_TEST(test_at_cmd_parse_bedge_dag_caps);
    RUN_TEST(test_at_cmd_parse_neddy299_caps);
    RUN_TEST(test_at_cmd_parse_empty_response_returns_zero);
    RUN_TEST(test_at_cmd_parse_null_inputs_return_zero);
    RUN_TEST(test_at_cmd_parse_requires_quoted_match);
    RUN_TEST(test_at_cmd_parse_substring_in_text_does_not_match);
    RUN_TEST(test_at_cmd_response_valid_detector);
    RUN_TEST(test_at_cmd_parse_arbitrary_line_order);

    /* M1_ESP32_FALLBACK_* constant invariants */
    RUN_TEST(test_fallback_constants_are_nonzero);
    RUN_TEST(test_fallback_at_bss_exceeds_sin360);
    RUN_TEST(test_fallback_sin360_heap_exceeds_at);

    return UNITY_END();
}
