/* See COPYING.txt for license details. */

/*
 * test_esp32_caps.c
 *
 * Unit tests for the pure-logic helpers in m1_esp32_caps.h:
 *   m1_esp32_caps_derive()        — wire bitmap → internal M1_ESP32_CAP_* bits
 *   m1_esp32_caps_parse_payload() — raw bytes → derived caps + fw_name
 *
 * Both functions are static inlines in the header — zero hardware deps.
 *
 * Tests cover:
 *   - Wire-format derive: each M1_AT_CMD_* / M1_EXT_CMD_* bit maps to the
 *     correct M1_ESP32_CAP_* bit
 *   - Wire-format derive: no-bits-set returns zero caps
 *   - Valid payload → correct derived caps and fw_name extraction
 *   - Payload too short → parse returns false, outputs unchanged
 *   - Wrong protocol version → parse returns false
 *   - fw_name is always null-terminated (even when payload fills all 32 bytes)
 *   - Payload struct is exactly 45 bytes
 *   - M1_ESP32_CAP_* bit constants do not overlap
 *   - M1_AT_CMD_* and M1_EXT_CMD_* bit constants do not overlap within
 *     their respective namespaces
 *   - Composite profile macros contain expected M1_ESP32_CAP_* bits
 *   - SiN360 profile derives correctly from wire-format profile macros
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
 * Helper: build a well-formed 45-byte status payload
 *
 * Layout (matches m1_esp32_status_payload_t):
 *   [0]      proto_ver
 *   [1-8]    at_cmd_bitmap (uint64_t, LE)
 *   [9-12]   ext_bitmap    (uint32_t, LE)
 *   [13-44]  fw_name       (32 bytes, null-terminated)
 * =========================================================================*/

static void make_payload(uint8_t  buf[64],
                         uint8_t  proto_ver,
                         uint64_t at_cmd,
                         uint32_t ext,
                         const char *fw_name)
{
    memset(buf, 0, 64);
    buf[0] = proto_ver;
    /* at_cmd_bitmap at bytes 1-8, little-endian */
    buf[1] = (uint8_t)(at_cmd        & 0xFFu);
    buf[2] = (uint8_t)((at_cmd >>  8) & 0xFFu);
    buf[3] = (uint8_t)((at_cmd >> 16) & 0xFFu);
    buf[4] = (uint8_t)((at_cmd >> 24) & 0xFFu);
    buf[5] = (uint8_t)((at_cmd >> 32) & 0xFFu);
    buf[6] = (uint8_t)((at_cmd >> 40) & 0xFFu);
    buf[7] = (uint8_t)((at_cmd >> 48) & 0xFFu);
    buf[8] = (uint8_t)((at_cmd >> 56) & 0xFFu);
    /* ext_bitmap at bytes 9-12, little-endian */
    buf[9]  = (uint8_t)(ext        & 0xFFu);
    buf[10] = (uint8_t)((ext >>  8) & 0xFFu);
    buf[11] = (uint8_t)((ext >> 16) & 0xFFu);
    buf[12] = (uint8_t)((ext >> 24) & 0xFFu);
    /* fw_name at bytes 13-44 */
    if (fw_name)
        strncpy((char *)&buf[13], fw_name, 31);
}

/* =========================================================================
 * Wire-format derive: individual bit mapping
 * =========================================================================*/

void test_derive_zero_bitmaps_gives_zero_caps(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, 0u);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps);
}

void test_derive_cwlap_gives_wifi_scan(void)
{
    uint64_t caps = m1_esp32_caps_derive(M1_AT_CMD_CWLAP, 0u);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_WIFI_SCAN);
}

void test_derive_cwjap_gives_wifi_connect(void)
{
    uint64_t caps = m1_esp32_caps_derive(M1_AT_CMD_CWJAP, 0u);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_WIFI_CONNECT);
}

void test_derive_blescan_gives_ble_scan(void)
{
    uint64_t caps = m1_esp32_caps_derive(M1_AT_CMD_BLESCAN, 0u);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_BLE_SCAN);
}

void test_derive_blegapadv_gives_ble_adv(void)
{
    uint64_t caps = m1_esp32_caps_derive(M1_AT_CMD_BLEGAPADV, 0u);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_ADV);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_BLE_ADV);
}

void test_derive_ext_sta_scan(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_STA_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_STA_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_WIFI_STA_SCAN);
}

void test_derive_ext_wifi_sniff(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_WIFI_SNIFF);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_SNIFF);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_WIFI_SNIFF);
}

void test_derive_ext_wifi_attack(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_WIFI_ATTACK);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_ATTACK);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_WIFI_ATTACK);
}

void test_derive_ext_wifi_netscan(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_WIFI_NETSCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_NETSCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_WIFI_NETSCAN);
}

void test_derive_ext_wifi_portal(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_WIFI_PORTAL);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_EVIL_PORTAL);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_WIFI_EVIL_PORTAL);
}

void test_derive_ext_ble_spam(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_BLE_SPAM);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_SPAM);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_BLE_SPAM);
}

void test_derive_ext_ble_sniff(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_BLE_SNIFF);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_SNIFF);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_BLE_SNIFF);
}

void test_derive_ext_ble_hid(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_BLE_HID);
}

void test_derive_ext_bt_manage(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_BT_MANAGE);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BT_MANAGE);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_BT_MANAGE);
}

void test_derive_ext_802154(void)
{
    uint64_t caps = m1_esp32_caps_derive(0u, M1_EXT_CMD_802154);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_802154);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), caps & ~M1_ESP32_CAP_802154);
}

/* =========================================================================
 * Wire-format derive: SiN360 profile round-trip
 * =========================================================================*/

void test_derive_sin360_profile_matches_cap_profile(void)
{
    uint64_t derived = m1_esp32_caps_derive(M1_AT_CMD_PROFILE_SIN360,
                                             M1_EXT_CMD_PROFILE_SIN360);
    /* Every bit in M1_ESP32_CAP_PROFILE_SIN360 must be present */
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PROFILE_SIN360,
                              derived & M1_ESP32_CAP_PROFILE_SIN360);
    /* SiN360 does NOT support WiFi connect or BLE HID */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), derived & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), derived & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), derived & M1_ESP32_CAP_BT_MANAGE);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), derived & M1_ESP32_CAP_802154);
}

/* =========================================================================
 * Parse: valid payload
 * =========================================================================*/

void test_parse_valid_sin360(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_AT_CMD_PROFILE_SIN360, M1_EXT_CMD_PROFILE_SIN360,
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

void test_parse_valid_all_at_cmds(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_AT_CMD_CWLAP | M1_AT_CMD_CWJAP | M1_AT_CMD_BLESCAN |
                 M1_AT_CMD_BLEGAPADV,
                 M1_EXT_CMD_BLE_HID | M1_EXT_CMD_BT_MANAGE | M1_EXT_CMD_802154,
                 "AT-bedge117-2.0.2");

    uint64_t caps = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &caps, fw_name);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_ADV);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_BT_MANAGE);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), caps & M1_ESP32_CAP_802154);
    TEST_ASSERT_EQUAL_STRING("AT-bedge117-2.0.2", fw_name);
}

void test_parse_empty_bitmaps_gives_zero_caps(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, 0u, 0u, "minimal-fw");

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
                 M1_AT_CMD_CWLAP, 0u, "fw");

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
                 M1_AT_CMD_CWLAP, 0u, "fw");

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
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, 0u, 0u, NULL);
    /* bytes 13-43 = 31 'A' chars; byte 44 (last of the 32-byte fw_name) = 'B'
     * This simulates a firmware that forgot the NUL terminator */
    memset(&buf[13], 'A', 31);
    buf[44] = 'B';

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
    /* 1 (proto_ver) + 8 (at_cmd_bitmap) + 4 (ext_bitmap) + 32 (fw_name) = 45 */
    TEST_ASSERT_EQUAL_UINT32(45u, (uint32_t)sizeof(m1_esp32_status_payload_t));
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
 * AT command bits: no two bits overlap within the namespace
 * =========================================================================*/

void test_at_cmd_bits_are_unique(void)
{
    const uint64_t bits[] = {
        M1_AT_CMD_AT,
        M1_AT_CMD_GMR,
        M1_AT_CMD_CWMODE,
        M1_AT_CMD_CWLAP,
        M1_AT_CMD_CWJAP,
        M1_AT_CMD_CWQAP,
        M1_AT_CMD_CIPSTAMAC,
        M1_AT_CMD_CWSTARTSMART,
        M1_AT_CMD_CWSTOPSMART,
        M1_AT_CMD_BLEINIT,
        M1_AT_CMD_BLESCANPARAM,
        M1_AT_CMD_BLESCAN,
        M1_AT_CMD_BLEGAPADV,
        M1_AT_CMD_BLEADVSTART,
        M1_AT_CMD_BLEADVSTOP,
        M1_AT_CMD_BLEGATTCPRIMSRV,
        M1_AT_CMD_BLEGATTCCHAR,
        M1_AT_CMD_BLEGATTCWR,
        M1_AT_CMD_BLEGATTCNTFY,
    };
    const size_t n = sizeof(bits) / sizeof(bits[0]);

    for (size_t i = 0; i < n; i++)
    {
        for (size_t j = i + 1; j < n; j++)
        {
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(UINT64_C(0), bits[i] & bits[j],
                "Two M1_AT_CMD_* bits share a bit position");
        }
    }
}

/* =========================================================================
 * EXT command bits: no two bits overlap within the namespace
 * =========================================================================*/

void test_ext_cmd_bits_are_unique(void)
{
    const uint32_t bits[] = {
        M1_EXT_CMD_STA_SCAN,
        M1_EXT_CMD_WIFI_SNIFF,
        M1_EXT_CMD_WIFI_ATTACK,
        M1_EXT_CMD_WIFI_NETSCAN,
        M1_EXT_CMD_WIFI_PORTAL,
        M1_EXT_CMD_BLE_SPAM,
        M1_EXT_CMD_BLE_SNIFF,
        M1_EXT_CMD_BLE_HID,
        M1_EXT_CMD_BT_MANAGE,
        M1_EXT_CMD_802154,
    };
    const size_t n = sizeof(bits) / sizeof(bits[0]);

    for (size_t i = 0; i < n; i++)
    {
        for (size_t j = i + 1; j < n; j++)
        {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, bits[i] & bits[j],
                "Two M1_EXT_CMD_* bits share a bit position");
        }
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

    /* Wire-format derive: individual bit mapping */
    RUN_TEST(test_derive_zero_bitmaps_gives_zero_caps);
    RUN_TEST(test_derive_cwlap_gives_wifi_scan);
    RUN_TEST(test_derive_cwjap_gives_wifi_connect);
    RUN_TEST(test_derive_blescan_gives_ble_scan);
    RUN_TEST(test_derive_blegapadv_gives_ble_adv);
    RUN_TEST(test_derive_ext_sta_scan);
    RUN_TEST(test_derive_ext_wifi_sniff);
    RUN_TEST(test_derive_ext_wifi_attack);
    RUN_TEST(test_derive_ext_wifi_netscan);
    RUN_TEST(test_derive_ext_wifi_portal);
    RUN_TEST(test_derive_ext_ble_spam);
    RUN_TEST(test_derive_ext_ble_sniff);
    RUN_TEST(test_derive_ext_ble_hid);
    RUN_TEST(test_derive_ext_bt_manage);
    RUN_TEST(test_derive_ext_802154);
    RUN_TEST(test_derive_sin360_profile_matches_cap_profile);

    /* Parse: valid payload */
    RUN_TEST(test_parse_valid_sin360);
    RUN_TEST(test_parse_valid_all_at_cmds);
    RUN_TEST(test_parse_empty_bitmaps_gives_zero_caps);

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
    RUN_TEST(test_at_cmd_bits_are_unique);
    RUN_TEST(test_ext_cmd_bits_are_unique);

    /* Profile macros */
    RUN_TEST(test_sin360_cap_profile_has_expected_caps);
    RUN_TEST(test_at_c3_cap_profile_has_expected_caps);
    RUN_TEST(test_neddy299_cap_profile_extends_at_c3);

    return UNITY_END();
}
