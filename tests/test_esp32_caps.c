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
 *   - Valid payload with AT/C3 cap profile  → correct caps and fw_name
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

void test_parse_valid_at_c3(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_ESP32_CAP_PROFILE_AT_C3,
                 "AT-bedge117-2.0.2");

    uint64_t caps = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &caps, fw_name);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BT_MANAGE);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_802154);
    TEST_ASSERT_EQUAL_STRING("AT-bedge117-2.0.2", fw_name);
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

void test_at_c3_cap_profile_has_expected_caps(void)
{
    const uint64_t p = M1_ESP32_CAP_PROFILE_AT_C3;
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_802154);
    /* AT/C3 does NOT support SiN360 sniffers or attacks */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_SNIFF);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_ATTACK);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SPAM);
}

void test_neddy299_cap_profile_extends_at_c3(void)
{
    const uint64_t c3     = M1_ESP32_CAP_PROFILE_AT_C3;
    const uint64_t neddy  = M1_ESP32_CAP_PROFILE_AT_NEDDY299;
    /* neddy299 is a strict superset of AT_C3 */
    TEST_ASSERT_EQUAL_UINT64(c3, neddy & c3);
    /* Adds station scan and attack */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), neddy & M1_ESP32_CAP_WIFI_STA_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), neddy & M1_ESP32_CAP_WIFI_ATTACK);
}

/* =========================================================================
 * main
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Parse: valid payload */
    RUN_TEST(test_parse_valid_sin360);
    RUN_TEST(test_parse_valid_at_c3);
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
    RUN_TEST(test_at_c3_cap_profile_has_expected_caps);
    RUN_TEST(test_neddy299_cap_profile_extends_at_c3);

    return UNITY_END();
}
