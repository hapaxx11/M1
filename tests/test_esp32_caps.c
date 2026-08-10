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
    TEST_ASSERT_GREATER_THAN(0u, M1_ESP32_FALLBACK_BSS_T800);
    TEST_ASSERT_GREATER_THAN(0u, M1_ESP32_FALLBACK_HEAP_T800);
    TEST_ASSERT_GREATER_THAN(0u, M1_ESP32_FALLBACK_BSS_CD3);
    TEST_ASSERT_GREATER_THAN(0u, M1_ESP32_FALLBACK_HEAP_CD3);
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
        M1_ESP32_CAP_PMKID,
        M1_ESP32_CAP_HANDSHAKE,
        M1_ESP32_CAP_OTA,
        M1_ESP32_CAP_BLE_SPAM,
        M1_ESP32_CAP_802154_TX,
        M1_ESP32_CAP_SOFTAP,
        M1_ESP32_CAP_ESPNOW,
        M1_ESP32_CAP_WIFI_HOTSPOT,
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
        M1_ESP32_CAP_PMKID,
        M1_ESP32_CAP_HANDSHAKE,
        M1_ESP32_CAP_OTA,
        M1_ESP32_CAP_BLE_SPAM,
        M1_ESP32_CAP_802154_TX,
        M1_ESP32_CAP_SOFTAP,
        M1_ESP32_CAP_ESPNOW,
        M1_ESP32_CAP_WIFI_HOTSPOT,
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
 * Canonical CD3 wire-protocol bit alignment (regression)
 *
 * Bits 0-23 of M1_ESP32_CAP_* MUST match the canonical CD3 firmware header
 * (bedge117/m1-esp32-brain, components/m1_rpc/include/m1_rpc.h — the M1_CAP_*
 * definitions).  The CD3 firmware serialises its self-reported capability
 * bitmap (M1_FW_CAPS) using those exact positions in the M1_RPC_SYS_GET_STATUS
 * response, so any divergence silently mis-maps every CD3 capability from the
 * first mismatched bit upward.  These literal values are copied from m1_rpc.h
 * and must not be changed to "follow" the header — if the header ever moves a
 * bit, the wire protocol has broken and BOTH must be corrected together.
 * =========================================================================*/

void test_caps_match_canonical_cd3_wire_bits(void)
{
    /* System / WiFi / BLE / 802.15.4 bits 0-20 (M1_CAP_* in m1_rpc.h). */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  0, M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  1, M1_ESP32_CAP_STA_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  2, M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  3, M1_ESP32_CAP_BLE_ADV);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  4, M1_ESP32_CAP_DEAUTH);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  5, M1_ESP32_CAP_BEACON);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  6, M1_ESP32_CAP_PROBE_FLOOD);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  7, M1_ESP32_CAP_KARMA);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  8, M1_ESP32_CAP_PKTMON);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) <<  9, M1_ESP32_CAP_PORTAL);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 10, M1_ESP32_CAP_WIFI_JOIN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 11, M1_ESP32_CAP_WIFI_SET_MAC);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 12, M1_ESP32_CAP_WIFI_SET_CHAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 13, M1_ESP32_CAP_NETSCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 14, M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 15, M1_ESP32_CAP_BT_MANAGE);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 16, M1_ESP32_CAP_802154);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 17, M1_ESP32_CAP_BLE_GATT);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 18, M1_ESP32_CAP_PMKID);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 19, M1_ESP32_CAP_HANDSHAKE);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 20, M1_ESP32_CAP_OTA);

    /* Bits 21-23 — the ones that were previously mis-assigned on the M1. */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 21, M1_ESP32_CAP_BLE_SPAM);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 22, M1_ESP32_CAP_802154_TX);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1) << 23, M1_ESP32_CAP_SOFTAP);

    /* Host-only capabilities must NOT collide with any canonical CD3 bit
     * (0-23); they live at bit 24+. */
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(UINT64_C(1) << 24, M1_ESP32_CAP_ESPNOW);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(UINT64_C(1) << 24, M1_ESP32_CAP_WIFI_HOTSPOT);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0),
        (M1_ESP32_CAP_ESPNOW | M1_ESP32_CAP_WIFI_HOTSPOT) &
        ((UINT64_C(1) << 24) - 1));
}

/* The exact capability bitmap the shipped CD3 firmware self-reports in its
 * GET_STATUS response (M1_FW_CAPS in bedge117/m1-esp32-brain main/main.c). */
#define CD3_M1_FW_CAPS_WIRE                                                    \
    ((UINT64_C(1) <<  0) | (UINT64_C(1) << 10) | (UINT64_C(1) <<  4) |         \
     (UINT64_C(1) <<  8) | (UINT64_C(1) <<  1) | (UINT64_C(1) <<  5) |         \
     (UINT64_C(1) <<  9) | (UINT64_C(1) << 19) | (UINT64_C(1) << 16) |         \
     (UINT64_C(1) <<  2) | (UINT64_C(1) <<  3) | (UINT64_C(1) << 14) |         \
     (UINT64_C(1) << 21) | (UINT64_C(1) << 22) | (UINT64_C(1) <<  6) |         \
     (UINT64_C(1) <<  7) | (UINT64_C(1) << 23))

void test_cd3_reported_bitmap_maps_to_correct_caps(void)
{
    /* Serialise the CD3 bitmap the way the firmware does (LE uint8[8]) and
     * unpack it via the shared helper — exactly what m1_esp32_caps_init()
     * does with the GET_STATUS payload. */
    uint8_t bitmap[8];
    for (unsigned i = 0; i < 8u; i++)
        bitmap[i] = (uint8_t)((CD3_M1_FW_CAPS_WIRE >> (i * 8u)) & 0xFFu);

    const uint64_t caps = m1_esp32_rpc_caps_get(bitmap);

    /* Bits 21-23 the firmware advertises resolve to the right capabilities. */
    TEST_ASSERT_TRUE(caps & M1_ESP32_CAP_BLE_SPAM);   /* bit 21 */
    TEST_ASSERT_TRUE(caps & M1_ESP32_CAP_802154_TX);  /* bit 22 */
    TEST_ASSERT_TRUE(caps & M1_ESP32_CAP_SOFTAP);     /* bit 23 */
    TEST_ASSERT_TRUE(caps & M1_ESP32_CAP_HANDSHAKE);  /* bit 19 */

    /* Host-only bits are NOT advertised by CD3, so they must read as false —
     * the pre-fix bug set ESPNOW (old bit 21) and WIFI_HOTSPOT (old bit 22)
     * from the firmware's BLE_SPAM / 802154_TX advertisement. */
    TEST_ASSERT_FALSE(caps & M1_ESP32_CAP_ESPNOW);
    TEST_ASSERT_FALSE(caps & M1_ESP32_CAP_WIFI_HOTSPOT);
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
    /* SiN360 v0.9.1.0+ supports BLE HID via CMD_BLE_HID_START/STOP/STATUS/REPORT */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
    /* SiN360 exposes a NimBLE GATT client via CMD_BLE_GATT_START/NEXT/STOP/WRITE/SUB/NOTIF */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_GATT);
    /* SiN360 profile does NOT include BT manage, 802.15.4, or WiFi join */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_JOIN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BT_MANAGE);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_802154);
}

void test_dag_t800_profile_has_expected_caps(void)
{
    const uint64_t p = M1_ESP32_CAP_PROFILE_DAG_T800;
    /* T-800 includes WiFi join (stock AT), all dag attack caps, and BLE HID */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_JOIN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_DEAUTH);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BEACON);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_KARMA);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PORTAL);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_ADV);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PKTMON);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PROBE_FLOOD);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
    /* T-800 does NOT include BLE scan, STA scan, NETSCAN, or BLE GATT
     * (those are SiN360-specific binary SPI features) */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_NETSCAN);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_GATT);
}

void test_fallback_t800_bss_exceeds_sin360(void)
{
    /* T-800 AT firmware has larger BSS than SiN360 due to AT infrastructure.
     * Unity: TEST_ASSERT_GREATER_THAN(threshold, actual) passes when actual > threshold. */
    TEST_ASSERT_GREATER_THAN(M1_ESP32_FALLBACK_BSS_SIN360,
                             M1_ESP32_FALLBACK_BSS_T800);
}

void test_fallback_t800_heap_less_than_sin360(void)
{
    /* T-800 has less free heap than SiN360 due to AT + custom modules.
     * Use LESS_THAN so the assertion reads naturally: T800_HEAP < SIN360_HEAP. */
    TEST_ASSERT_LESS_THAN(M1_ESP32_FALLBACK_HEAP_SIN360,
                          M1_ESP32_FALLBACK_HEAP_T800);
}

/* =========================================================================
 * CD3 profile macro sanity checks
 * =========================================================================*/

void test_cd3_profile_has_expected_caps(void)
{
    const uint64_t p = M1_ESP32_CAP_PROFILE_CD3;
    /* CD3 includes WiFi scan/join, attack features, BLE, 802.15.4 */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_JOIN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_STA_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_DEAUTH);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BEACON);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PROBE_FLOOD);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_KARMA);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PKTMON);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PORTAL);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_ADV);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_GATT);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_802154);
    /* CD3-specific caps */
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_PMKID);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_HANDSHAKE);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_OTA);
    /* CD3 does NOT include NETSCAN (no ping/ARP scanner in v1) */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_NETSCAN);
}

void test_cd3_fallback_bss_less_than_at(void)
{
    /* CD3 native WiFi has less BSS than AT firmware (no AT ring buffers). */
    TEST_ASSERT_LESS_THAN(M1_ESP32_FALLBACK_BSS_AT,
                          M1_ESP32_FALLBACK_BSS_CD3);
}

void test_cd3_fallback_heap_exceeds_at(void)
{
    /* CD3 has more free heap than AT firmware (no AT infrastructure overhead). */
    TEST_ASSERT_GREATER_THAN(M1_ESP32_FALLBACK_HEAP_AT,
                             M1_ESP32_FALLBACK_HEAP_CD3);
}

/* =========================================================================
 * M1_RPC protocol helper tests
 * =========================================================================*/

/* ---- CRC16 -------------------------------------------------------------- */

void test_rpc_crc16_empty_returns_0xffff(void)
{
    /* CRC of zero bytes with init=0xFFFF is always 0xFFFF */
    TEST_ASSERT_EQUAL_UINT16(0xFFFFu, m1_esp32_rpc_crc16(NULL, 0));
}

void test_rpc_crc16_single_zero_byte(void)
{
    /* CRC-16/CCITT of 0x00: init=0xFFFF XOR (0x00 << 8) = 0xFFFF,
     * 8 shift iterations with bit 15 always set → result is well-known.
     * Pre-computed: 0xE1F0 */
    const uint8_t data[] = {0x00};
    TEST_ASSERT_EQUAL_UINT16(0xE1F0u,
                             m1_esp32_rpc_crc16(data, sizeof(data)));
}

void test_rpc_crc16_known_vector(void)
{
    /* CRC-16/CCITT of "123456789" (ASCII) = 0x29B1 — standard test vector. */
    const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    TEST_ASSERT_EQUAL_UINT16(0x29B1u,
                             m1_esp32_rpc_crc16(data, sizeof(data)));
}

/* ---- m1_esp32_rpc_build_req --------------------------------------------- */

void test_rpc_build_req_ping_frame_layout(void)
{
    uint8_t buf[64];
    memset(buf, 0xCC, sizeof(buf));

    const uint8_t cookie[4] = {0x4D, 0x31, 0x50, 0x49}; /* "M1PI" */
    uint16_t frame_len = m1_esp32_rpc_build_req(buf, sizeof(buf),
                                                 M1_ESP32_RPC_SYS_PING,
                                                 cookie, 4u);

    /* Frame size: 8 (header) + 4 (payload) + 2 (CRC) = 14 */
    TEST_ASSERT_EQUAL_UINT16(14u, frame_len);

    /* Magic bytes (LE 0x4D31 → [0x31, 0x4D]) */
    TEST_ASSERT_EQUAL_UINT8(0x31u, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x4Du, buf[1]);
    /* Version */
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_VERSION, buf[2]);
    /* msg_type = REQ */
    TEST_ASSERT_EQUAL_UINT8(M1_ESP32_RPC_REQ, buf[3]);
    /* msg_id = PING (LE 0x0001 → [0x01, 0x00]) */
    TEST_ASSERT_EQUAL_UINT8(0x01u, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, buf[5]);
    /* payload_len = 4 */
    TEST_ASSERT_EQUAL_UINT8(0x04u, buf[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, buf[7]);
    /* payload = cookie */
    TEST_ASSERT_EQUAL_UINT8(0x4Du, buf[8]);
    TEST_ASSERT_EQUAL_UINT8(0x31u, buf[9]);
    TEST_ASSERT_EQUAL_UINT8(0x50u, buf[10]);
    TEST_ASSERT_EQUAL_UINT8(0x49u, buf[11]);
    /* CRC16 over bytes 0-11 should match inline computation */
    uint16_t expected_crc = m1_esp32_rpc_crc16(buf, 12);
    uint16_t wire_crc = (uint16_t)buf[12] | ((uint16_t)buf[13] << 8u);
    TEST_ASSERT_EQUAL_UINT16(expected_crc, wire_crc);
}

void test_rpc_build_req_get_status_no_payload(void)
{
    uint8_t buf[64];
    memset(buf, 0xCC, sizeof(buf));

    uint16_t frame_len = m1_esp32_rpc_build_req(buf, sizeof(buf),
                                                 M1_ESP32_RPC_SYS_GET_STATUS,
                                                 NULL, 0u);

    /* Frame size: 8 + 0 + 2 = 10 */
    TEST_ASSERT_EQUAL_UINT16(10u, frame_len);
    /* msg_id = GET_STATUS (LE 0x0002 → [0x02, 0x00]) */
    TEST_ASSERT_EQUAL_UINT8(0x02u, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, buf[5]);
    /* payload_len = 0 */
    TEST_ASSERT_EQUAL_UINT8(0x00u, buf[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, buf[7]);
}

void test_rpc_build_req_buf_too_small_returns_zero(void)
{
    uint8_t buf[9]; /* Enough for header but not header+CRC */
    uint16_t frame_len = m1_esp32_rpc_build_req(buf, sizeof(buf),
                                                 M1_ESP32_RPC_SYS_GET_STATUS,
                                                 NULL, 0u);
    TEST_ASSERT_EQUAL_UINT16(0u, frame_len);
}

void test_rpc_build_req_null_buf_returns_zero(void)
{
    uint16_t frame_len = m1_esp32_rpc_build_req(NULL, 64u,
                                                 M1_ESP32_RPC_SYS_PING,
                                                 NULL, 0u);
    TEST_ASSERT_EQUAL_UINT16(0u, frame_len);
}

/* ---- m1_esp32_rpc_parse_resp -------------------------------------------- */

/** Helper: build a synthetic M1_RPC response frame. */
static void make_rpc_resp(uint8_t buf[64], uint16_t msg_id,
                           const uint8_t *payload, uint16_t plen)
{
    memset(buf, 0, 64);
    buf[0] = (uint8_t)(M1_ESP32_RPC_MAGIC        & 0xFFu);
    buf[1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
    buf[2] = M1_ESP32_RPC_VERSION;
    buf[3] = M1_ESP32_RPC_RESP;
    buf[4] = (uint8_t)(msg_id        & 0xFFu);
    buf[5] = (uint8_t)((msg_id >> 8u) & 0xFFu);
    buf[6] = (uint8_t)(plen        & 0xFFu);
    buf[7] = (uint8_t)((plen >> 8u) & 0xFFu);
    for (uint16_t i = 0; i < plen; i++)
        buf[8u + i] = payload ? payload[i] : 0u;
    uint16_t crc = m1_esp32_rpc_crc16(buf, 8u + plen);
    buf[8u + plen]      = (uint8_t)(crc        & 0xFFu);
    buf[8u + plen + 1u] = (uint8_t)((crc >> 8u) & 0xFFu);
}

void test_rpc_parse_resp_valid_ping(void)
{
    uint8_t buf[64];
    const uint8_t echo[4] = {0x4D, 0x31, 0x50, 0x49};
    make_rpc_resp(buf, M1_ESP32_RPC_SYS_PING, echo, 4u);

    const uint8_t *pl  = NULL;
    uint16_t       plen = 0u;
    bool ok = m1_esp32_rpc_parse_resp(buf, 64u,
                                       M1_ESP32_RPC_SYS_PING,
                                       &pl, &plen);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT16(4u, plen);
    TEST_ASSERT_EQUAL_UINT8(0x4Du, pl[0]);
}

void test_rpc_parse_resp_wrong_magic_returns_false(void)
{
    uint8_t buf[64];
    make_rpc_resp(buf, M1_ESP32_RPC_SYS_PING, NULL, 0u);
    buf[0] = 0xAB;  /* Corrupt magic */

    const uint8_t *pl  = NULL;
    uint16_t       plen = 0u;
    TEST_ASSERT_FALSE(m1_esp32_rpc_parse_resp(buf, 64u,
                                               M1_ESP32_RPC_SYS_PING,
                                               &pl, &plen));
}

void test_rpc_parse_resp_wrong_msg_id_returns_false(void)
{
    uint8_t buf[64];
    make_rpc_resp(buf, M1_ESP32_RPC_SYS_GET_STATUS, NULL, 0u);

    const uint8_t *pl  = NULL;
    uint16_t       plen = 0u;
    /* Parse as PING but frame is a GET_STATUS response */
    TEST_ASSERT_FALSE(m1_esp32_rpc_parse_resp(buf, 64u,
                                               M1_ESP32_RPC_SYS_PING,
                                               &pl, &plen));
}

void test_rpc_parse_resp_bad_crc_returns_false(void)
{
    uint8_t buf[64];
    make_rpc_resp(buf, M1_ESP32_RPC_SYS_PING, NULL, 0u);
    buf[8] ^= 0xFF;  /* Corrupt CRC low byte */

    const uint8_t *pl  = NULL;
    uint16_t       plen = 0u;
    TEST_ASSERT_FALSE(m1_esp32_rpc_parse_resp(buf, 64u,
                                               M1_ESP32_RPC_SYS_PING,
                                               &pl, &plen));
}

void test_rpc_parse_resp_nak_returns_false(void)
{
    uint8_t buf[64];
    make_rpc_resp(buf, M1_ESP32_RPC_SYS_PING, NULL, 0u);
    buf[3] = M1_ESP32_RPC_NAK;  /* Overwrite msg_type → recompute CRC */
    uint16_t crc = m1_esp32_rpc_crc16(buf, 8u);
    buf[8] = (uint8_t)(crc & 0xFFu);
    buf[9] = (uint8_t)((crc >> 8u) & 0xFFu);

    const uint8_t *pl  = NULL;
    uint16_t       plen = 0u;
    TEST_ASSERT_FALSE(m1_esp32_rpc_parse_resp(buf, 64u,
                                               M1_ESP32_RPC_SYS_PING,
                                               &pl, &plen));
}

void test_rpc_parse_resp_too_short_returns_false(void)
{
    uint8_t buf[64];
    make_rpc_resp(buf, M1_ESP32_RPC_SYS_PING, NULL, 0u);

    const uint8_t *pl  = NULL;
    uint16_t       plen = 0u;
    /* Pass only 9 bytes: header(8) + CRC_low(1), missing CRC_high */
    TEST_ASSERT_FALSE(m1_esp32_rpc_parse_resp(buf, 9u,
                                               M1_ESP32_RPC_SYS_PING,
                                               &pl, &plen));
}

/* ---- m1_esp32_rpc_devstatus_t ------------------------------------------- */

void test_rpc_devstatus_struct_size(void)
{
    /* 1 (proto_ver) + 8 (cap_bitmap[8]) + 32 (fw_name) = 41 */
    TEST_ASSERT_EQUAL_UINT32(41u, (uint32_t)sizeof(m1_esp32_rpc_devstatus_t));
}

void test_rpc_caps_get_round_trip(void)
{
    /* Pack a known bitmap into a uint8_t[8] LE array, then unpack. */
    const uint64_t expected = M1_ESP32_CAP_PROFILE_CD3;
    uint8_t bitmap[8];
    for (unsigned i = 0; i < 8u; i++)
        bitmap[i] = (uint8_t)((expected >> (i * 8u)) & 0xFFu);
    TEST_ASSERT_EQUAL_UINT64(expected, m1_esp32_rpc_caps_get(bitmap));
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
    /* Stock ESP-AT */
    { "AT+CWJAP",        M1_ESP32_CAP_WIFI_JOIN  },
    { "AT+BLEHIDINIT",   M1_ESP32_CAP_BLE_HID    },
    /* bedge117 / neddy299 custom commands */
    { "AT+ZIGSNIFF",     M1_ESP32_CAP_802154     },
    { "AT+DEAUTH",       M1_ESP32_CAP_DEAUTH     },
    { "AT+STASCAN",      M1_ESP32_CAP_STA_SCAN   },
    /* dag T-800 custom WiFi commands (at_custom_wifi_cmd.c) */
    { "AT+M1DEAUTH",     M1_ESP32_CAP_DEAUTH     },
    { "AT+M1DEAUTHALL",  M1_ESP32_CAP_DEAUTH     },
    { "AT+M1DEAUTHSTOP", M1_ESP32_CAP_DEAUTH     },
    { "AT+M1BEACON",     M1_ESP32_CAP_BEACON     },
    { "AT+M1KARMA",      M1_ESP32_CAP_KARMA      },
    { "AT+M1EVILTWIN",   M1_ESP32_CAP_PORTAL     },
    { "AT+M1BLESPAM",    M1_ESP32_CAP_BLE_ADV    },
    { "AT+M1MONITOR",    M1_ESP32_CAP_PKTMON     },
    { "AT+M1PROBE",      M1_ESP32_CAP_PROBE_FLOOD},
    { "AT+M1PMKID",      M1_ESP32_CAP_PKTMON     },
    { "AT+M1HSCAP",      M1_ESP32_CAP_PKTMON     },
    { "AT+M1WIFISTATS",  M1_ESP32_CAP_WIFI_SCAN  },
    /* dag T-800 custom HID commands (at_custom_hid_cmd.c) */
    { "AT+HIDKBINIT",    M1_ESP32_CAP_BLE_HID    },
    { "AT+HIDKBSEND",    M1_ESP32_CAP_BLE_HID    },
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

/* dag T-800 (dagnazty/M1-T-800): all 14 custom AT commands.
 * Note: AT+CWJAP is stock ESP-AT and IS present; AT+ZIGSNIFF is also
 * present in the dag T-800 firmware. */
static const char *k_resp_dag_t800 =
    "+CMD:0,\"AT\",0,0,0,1\r\n"
    "+CMD:1,\"AT+CWJAP\",1,1,1,1\r\n"
    "+CMD:2,\"AT+ZIGSNIFF\",1,1,1,0\r\n"
    "+CMD:3,\"AT+M1DEAUTH\",1,1,1,0\r\n"
    "+CMD:4,\"AT+M1DEAUTHALL\",1,1,1,0\r\n"
    "+CMD:5,\"AT+M1DEAUTHSTOP\",1,1,1,0\r\n"
    "+CMD:6,\"AT+M1BEACON\",1,1,1,0\r\n"
    "+CMD:7,\"AT+M1KARMA\",1,1,1,0\r\n"
    "+CMD:8,\"AT+M1EVILTWIN\",1,1,1,0\r\n"
    "+CMD:9,\"AT+M1BLESPAM\",1,1,1,0\r\n"
    "+CMD:10,\"AT+M1MONITOR\",1,1,1,0\r\n"
    "+CMD:11,\"AT+M1PROBE\",1,1,1,0\r\n"
    "+CMD:12,\"AT+M1PMKID\",1,1,1,0\r\n"
    "+CMD:13,\"AT+M1HSCAP\",1,1,1,0\r\n"
    "+CMD:14,\"AT+M1WIFISTATS\",1,1,1,0\r\n"
    "+CMD:15,\"AT+HIDKBINIT\",1,1,1,0\r\n"
    "+CMD:16,\"AT+HIDKBSEND\",1,1,1,0\r\n"
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

void test_at_cmd_parse_dag_t800_caps(void)
{
    const uint64_t expected = M1_ESP32_CAP_WIFI_JOIN   |
                              M1_ESP32_CAP_802154      |
                              M1_ESP32_CAP_DEAUTH      |
                              M1_ESP32_CAP_BEACON      |
                              M1_ESP32_CAP_KARMA       |
                              M1_ESP32_CAP_PORTAL      |
                              M1_ESP32_CAP_BLE_ADV     |
                              M1_ESP32_CAP_PKTMON      |
                              M1_ESP32_CAP_PROBE_FLOOD |
                              M1_ESP32_CAP_WIFI_SCAN   |
                              M1_ESP32_CAP_BLE_HID;
    uint64_t caps = m1_esp32_caps_parse_at_cmd_list(
        k_resp_dag_t800, k_test_at_cmd_map, k_test_at_cmd_map_n);

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

/* =========================================================================
 * m1_esp32_caps_should_run_at_probe() — AT-task-not-yet-running regression
 *
 * Bug: navigating directly to a capability-gated scene (e.g. WiFi ->
 * 802.15.4 -> Zigbee Scan, or Attacks -> PMKID Grab) as the very first
 * ESP32 interaction left the AT task not yet started.  m1_esp32_caps_init()
 * used to bail out of Probe 3 unconditionally whenever the AT task was not
 * already running, permanently failing capability detection for AT-only
 * firmware (dag T-800) — firmware name stuck at "Unknown" and every
 * capability bit (including M1_ESP32_CAP_BEACON, gating PMKID Grab) unset.
 * =========================================================================*/

void test_should_run_at_probe_already_running(void)
{
    /* AT task already running before any start attempt — always proceed. */
    TEST_ASSERT_TRUE(m1_esp32_caps_should_run_at_probe(true, true));
}

void test_should_run_at_probe_started_successfully(void)
{
    /* AT task was not running, but the start attempt brought it up —
     * this is the fix: proceed with the probe instead of bailing out. */
    TEST_ASSERT_TRUE(m1_esp32_caps_should_run_at_probe(false, true));
}

void test_should_run_at_probe_still_not_running(void)
{
    /* AT task was not running and the start attempt did not bring it up
     * (e.g. heap exhaustion) — retry on a later call instead of probing. */
    TEST_ASSERT_FALSE(m1_esp32_caps_should_run_at_probe(false, false));
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
 * "Feature not supported" screen wording (m1_esp32_require_cap body lines)
 *
 * Regression coverage for issue #668 defect 2 — the screen previously read
 * "Flash compatible / ESP32 firmware" (broken English).  The corrected
 * wording reads "Flash a compatible / ESP32 firmware".
 * =========================================================================*/

void test_unsupported_screen_lines_are_grammatical(void)
{
    /* First line unchanged. */
    TEST_ASSERT_EQUAL_STRING("Not supported by", M1_ESP32_UNSUPPORTED_LINE_1);

    /* The instruction must include the article "a" so the two lines read
     * "Flash a compatible ESP32 firmware" rather than the broken
     * "Flash compatible ESP32 firmware". */
    TEST_ASSERT_EQUAL_STRING("Flash a compatible", M1_ESP32_UNSUPPORTED_LINE_2);
    TEST_ASSERT_EQUAL_STRING("ESP32 firmware", M1_ESP32_UNSUPPORTED_LINE_3);
    TEST_ASSERT_NOT_NULL(strstr(M1_ESP32_UNSUPPORTED_LINE_2, " a "));
}

void test_unsupported_screen_lines_fit_display(void)
{
    /* Main-menu font renders ~21 chars on the 128px display. */
    TEST_ASSERT_LESS_OR_EQUAL_UINT(21u, (unsigned)strlen(M1_ESP32_UNSUPPORTED_LINE_1));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(21u, (unsigned)strlen(M1_ESP32_UNSUPPORTED_LINE_2));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(21u, (unsigned)strlen(M1_ESP32_UNSUPPORTED_LINE_3));
}

/* =========================================================================
 * Brain-CD3 GET_FW_VERSION formatting (qMonstatek compatibility regression)
 *
 * Regression coverage for the "ESP32 detected — incompatible firmware" issue:
 * qMonstatek only treats the ESP as compatible when esp32_version contains a
 * parseable dotted version (parseVerNums()).  The brain's GET_STATUS fw_name
 * ("m1-native") carries no version, so the device-info string must fold in the
 * GET_FW_VERSION semver.  m1_esp32_rpc_format_fw_version() builds that string.
 * =========================================================================*/

void test_rpc_fw_version_struct_size(void)
{
    /* 3 semver bytes + 16-byte git hash = 19 bytes on the wire. */
    TEST_ASSERT_EQUAL_UINT(19u, (unsigned)sizeof(m1_esp32_rpc_fw_version_t));
}

void test_format_fw_version_basic_semver(void)
{
    m1_esp32_rpc_fw_version_t v;
    memset(&v, 0, sizeof(v));
    v.major = 1u; v.minor = 5u; v.patch = 0u;

    char out[32];
    m1_esp32_rpc_format_fw_version(out, sizeof(out), "m1-native", &v);

    /* Must be exactly "<name> X.Y.Z" with no trailing hash when git_hash
     * is empty. */
    TEST_ASSERT_EQUAL_STRING("m1-native 1.5.0", out);
}

void test_format_fw_version_contains_parseable_dotted_version(void)
{
    /* The whole point: a client scanning for /(\d+)\.(\d+)\.(\d+)/ must find
     * a match in the produced string.  Verify the "X.Y.Z" substring exists. */
    m1_esp32_rpc_fw_version_t v;
    memset(&v, 0, sizeof(v));
    v.major = 1u; v.minor = 2u; v.patch = 16u;

    char out[32];
    m1_esp32_rpc_format_fw_version(out, sizeof(out), "m1-native", &v);

    TEST_ASSERT_NOT_NULL(strstr(out, "1.2.16"));
    /* Sanity: the bare fw_name alone would NOT have matched. */
    TEST_ASSERT_NULL(strstr("m1-native", "1.2.16"));
}

void test_format_fw_version_appends_distinct_hash(void)
{
    /* The firmware leaves git_hash blank when it equals the semver; when it is
     * a distinct build tag it is appended after the version. */
    m1_esp32_rpc_fw_version_t v;
    memset(&v, 0, sizeof(v));
    v.major = 1u; v.minor = 5u; v.patch = 0u;
    strncpy(v.git_hash, "g-e515182", sizeof(v.git_hash) - 1);

    char out[32];
    m1_esp32_rpc_format_fw_version(out, sizeof(out), "m1-native", &v);
    TEST_ASSERT_EQUAL_STRING("m1-native 1.5.0 g-e515182", out);
}

void test_format_fw_version_omits_redundant_hash(void)
{
    /* When git_hash equals the semver string it must not be duplicated
     * ("1.5.0 1.5.0"). */
    m1_esp32_rpc_fw_version_t v;
    memset(&v, 0, sizeof(v));
    v.major = 1u; v.minor = 5u; v.patch = 0u;
    strncpy(v.git_hash, "1.5.0", sizeof(v.git_hash) - 1);

    char out[32];
    m1_esp32_rpc_format_fw_version(out, sizeof(out), "m1-native", &v);
    TEST_ASSERT_EQUAL_STRING("m1-native 1.5.0", out);
}

void test_format_fw_version_null_name_uses_default(void)
{
    m1_esp32_rpc_fw_version_t v;
    memset(&v, 0, sizeof(v));
    v.major = 1u; v.minor = 0u; v.patch = 3u;

    char out[32];
    m1_esp32_rpc_format_fw_version(out, sizeof(out), NULL, &v);
    TEST_ASSERT_EQUAL_STRING("m1-native 1.0.3", out);
}

void test_format_fw_version_unterminated_hash_is_bounded(void)
{
    /* A malformed payload with no NUL in git_hash must not overrun the
     * destination or read past the 16-byte field. */
    m1_esp32_rpc_fw_version_t v;
    memset(&v, 0, sizeof(v));
    v.major = 9u; v.minor = 9u; v.patch = 9u;
    memset(v.git_hash, 'A', sizeof(v.git_hash)); /* no NUL terminator */

    char out[32];
    m1_esp32_rpc_format_fw_version(out, sizeof(out), "m1-native", &v);

    /* Output must be NUL-terminated within the buffer and contain the
     * version. */
    TEST_ASSERT_EQUAL_CHAR('\0', out[sizeof(out) - 1]);
    TEST_ASSERT_NOT_NULL(strstr(out, "9.9.9"));
    /* Bounded: total length must fit the 32-byte device-info field. */
    TEST_ASSERT_LESS_THAN_UINT(32u, (unsigned)strlen(out));
}

void test_format_fw_version_null_ver_clears_output(void)
{
    char out[32];
    memset(out, 'X', sizeof(out));
    m1_esp32_rpc_format_fw_version(out, sizeof(out), "m1-native", NULL);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_format_fw_version_null_out_is_safe(void)
{
    m1_esp32_rpc_fw_version_t v;
    memset(&v, 0, sizeof(v));
    /* Must not crash on NULL output or zero size. */
    m1_esp32_rpc_format_fw_version(NULL, 32u, "m1-native", &v);
    char out[4];
    memset(out, 'X', sizeof(out));
    m1_esp32_rpc_format_fw_version(out, 0u, "m1-native", &v);
    TEST_ASSERT_EQUAL_CHAR('X', out[0]); /* untouched when size==0 */
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
    RUN_TEST(test_caps_match_canonical_cd3_wire_bits);
    RUN_TEST(test_cd3_reported_bitmap_maps_to_correct_caps);

    /* Profile macros */
    RUN_TEST(test_sin360_profile_has_expected_caps);
    RUN_TEST(test_dag_t800_profile_has_expected_caps);

    /* AT+CMD? response parser */
    RUN_TEST(test_at_cmd_parse_stock_at_only_wifi_join);
    RUN_TEST(test_at_cmd_parse_bedge_dag_caps);
    RUN_TEST(test_at_cmd_parse_neddy299_caps);
    RUN_TEST(test_at_cmd_parse_dag_t800_caps);
    RUN_TEST(test_at_cmd_parse_empty_response_returns_zero);
    RUN_TEST(test_at_cmd_parse_null_inputs_return_zero);
    RUN_TEST(test_at_cmd_parse_requires_quoted_match);
    RUN_TEST(test_at_cmd_parse_substring_in_text_does_not_match);
    RUN_TEST(test_at_cmd_response_valid_detector);
    RUN_TEST(test_at_cmd_parse_arbitrary_line_order);

    /* AT-task startup regression (Probe 3 skip bug) */
    RUN_TEST(test_should_run_at_probe_already_running);
    RUN_TEST(test_should_run_at_probe_started_successfully);
    RUN_TEST(test_should_run_at_probe_still_not_running);

    /* M1_ESP32_FALLBACK_* constant invariants */
    RUN_TEST(test_fallback_constants_are_nonzero);
    RUN_TEST(test_fallback_at_bss_exceeds_sin360);
    RUN_TEST(test_fallback_sin360_heap_exceeds_at);
    RUN_TEST(test_fallback_t800_bss_exceeds_sin360);
    RUN_TEST(test_fallback_t800_heap_less_than_sin360);

    /* CD3 profile + fallback invariants */
    RUN_TEST(test_cd3_profile_has_expected_caps);
    RUN_TEST(test_cd3_fallback_bss_less_than_at);
    RUN_TEST(test_cd3_fallback_heap_exceeds_at);

    /* M1_RPC helpers: CRC16 */
    RUN_TEST(test_rpc_crc16_empty_returns_0xffff);
    RUN_TEST(test_rpc_crc16_single_zero_byte);
    RUN_TEST(test_rpc_crc16_known_vector);

    /* M1_RPC helpers: build_req */
    RUN_TEST(test_rpc_build_req_ping_frame_layout);
    RUN_TEST(test_rpc_build_req_get_status_no_payload);
    RUN_TEST(test_rpc_build_req_buf_too_small_returns_zero);
    RUN_TEST(test_rpc_build_req_null_buf_returns_zero);

    /* M1_RPC helpers: parse_resp */
    RUN_TEST(test_rpc_parse_resp_valid_ping);
    RUN_TEST(test_rpc_parse_resp_wrong_magic_returns_false);
    RUN_TEST(test_rpc_parse_resp_wrong_msg_id_returns_false);
    RUN_TEST(test_rpc_parse_resp_bad_crc_returns_false);
    RUN_TEST(test_rpc_parse_resp_nak_returns_false);
    RUN_TEST(test_rpc_parse_resp_too_short_returns_false);

    /* M1_RPC devstatus */
    RUN_TEST(test_rpc_devstatus_struct_size);
    RUN_TEST(test_rpc_caps_get_round_trip);

    /* Brain-CD3 GET_FW_VERSION formatting (qMonstatek compat regression) */
    RUN_TEST(test_rpc_fw_version_struct_size);
    RUN_TEST(test_format_fw_version_basic_semver);
    RUN_TEST(test_format_fw_version_contains_parseable_dotted_version);
    RUN_TEST(test_format_fw_version_appends_distinct_hash);
    RUN_TEST(test_format_fw_version_omits_redundant_hash);
    RUN_TEST(test_format_fw_version_null_name_uses_default);
    RUN_TEST(test_format_fw_version_unterminated_hash_is_bounded);
    RUN_TEST(test_format_fw_version_null_ver_clears_output);
    RUN_TEST(test_format_fw_version_null_out_is_safe);

    /* "Feature not supported" screen wording (issue #668 defect 2) */
    RUN_TEST(test_unsupported_screen_lines_are_grammatical);
    RUN_TEST(test_unsupported_screen_lines_fit_display);

    return UNITY_END();
}
