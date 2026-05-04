/* See COPYING.txt for license details. */

/*
 * test_esp32_caps.c
 *
 * Unit tests for the pure-logic parse helper in m1_esp32_caps.h:
 *   m1_esp32_caps_parse_payload()
 *
 * This function is a static inline in the header, requiring no hardware
 * stubs — just the standard C library and the header itself.
 *
 * Tests cover:
 *   - Valid payload → correct bitmap and fw_name extraction
 *   - Payload too short → parse returns false
 *   - Wrong protocol version → parse returns false
 *   - Capability bit constants do not overlap
 *   - Composite profile macros contain expected bits
 *   - fw_name is always null-terminated (even when payload fills all 32 bytes)
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
 * Helper: build a well-formed status payload in a 64-byte buffer
 * =========================================================================*/

static void make_payload(uint8_t  buf[64],
                         uint8_t  proto_ver,
                         uint64_t bitmap,
                         const char *fw_name,
                         uint32_t bss_bytes,
                         uint32_t free_heap_bytes)
{
    memset(buf, 0, 64);
    buf[0] = proto_ver;
    /* little-endian 64-bit bitmap at bytes 1-8 */
    buf[1] = (uint8_t)(bitmap        & 0xFFu);
    buf[2] = (uint8_t)((bitmap >>  8) & 0xFFu);
    buf[3] = (uint8_t)((bitmap >> 16) & 0xFFu);
    buf[4] = (uint8_t)((bitmap >> 24) & 0xFFu);
    buf[5] = (uint8_t)((bitmap >> 32) & 0xFFu);
    buf[6] = (uint8_t)((bitmap >> 40) & 0xFFu);
    buf[7] = (uint8_t)((bitmap >> 48) & 0xFFu);
    buf[8] = (uint8_t)((bitmap >> 56) & 0xFFu);
    /* fw_name at bytes 9-40 */
    if (fw_name)
        strncpy((char *)&buf[9], fw_name, 31);
    /* bss_bytes at bytes 41-44 (little-endian) */
    buf[41] = (uint8_t)(bss_bytes        & 0xFFu);
    buf[42] = (uint8_t)((bss_bytes >>  8) & 0xFFu);
    buf[43] = (uint8_t)((bss_bytes >> 16) & 0xFFu);
    buf[44] = (uint8_t)((bss_bytes >> 24) & 0xFFu);
    /* free_heap_bytes at bytes 45-48 (little-endian) */
    buf[45] = (uint8_t)(free_heap_bytes        & 0xFFu);
    buf[46] = (uint8_t)((free_heap_bytes >>  8) & 0xFFu);
    buf[47] = (uint8_t)((free_heap_bytes >> 16) & 0xFFu);
    buf[48] = (uint8_t)((free_heap_bytes >> 24) & 0xFFu);
}

/* =========================================================================
 * Parse: valid payload
 * =========================================================================*/

void test_parse_valid_sin360(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_ESP32_CAP_PROFILE_SIN360, "SiN360-0.9.6",
                 0x00020000u, 0x0002C000u);

    uint64_t bitmap = 0;
    char     fw_name[32];
    uint32_t bss = 0, heap = 0;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PROFILE_SIN360, bitmap);
    TEST_ASSERT_EQUAL_STRING("SiN360-0.9.6", fw_name);
    TEST_ASSERT_EQUAL_UINT32(0x00020000u, bss);
    TEST_ASSERT_EQUAL_UINT32(0x0002C000u, heap);
}

void test_parse_valid_at_c3(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_ESP32_CAP_PROFILE_AT_C3, "AT-bedge117-2.0.2",
                 0x00018000u, 0x00030000u);

    uint64_t bitmap = 0;
    char     fw_name[32];
    uint32_t bss = 0, heap = 0;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT64(M1_ESP32_CAP_PROFILE_AT_C3, bitmap);
    TEST_ASSERT_EQUAL_STRING("AT-bedge117-2.0.2", fw_name);
    TEST_ASSERT_EQUAL_UINT32(0x00018000u, bss);
    TEST_ASSERT_EQUAL_UINT32(0x00030000u, heap);
}

void test_parse_valid_all_caps(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, UINT64_MAX, "future-fw-1.0",
                 0u, 0u);

    uint64_t bitmap = 0;
    char     fw_name[32];
    uint32_t bss = 1, heap = 1;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, bitmap);
    TEST_ASSERT_EQUAL_STRING("future-fw-1.0", fw_name);
    /* Zero values must round-trip correctly */
    TEST_ASSERT_EQUAL_UINT32(0u, bss);
    TEST_ASSERT_EQUAL_UINT32(0u, heap);
}

/* =========================================================================
 * Parse: error paths
 * =========================================================================*/

void test_parse_too_short_returns_false(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_WIFI_SCAN, "fw",
                 0u, 0u);

    uint64_t bitmap   = UINT64_C(0xDEADBEEFDEADBEEF);
    char     fw_name[32] = "unchanged";
    uint32_t bss = 0xABCDu, heap = 0x1234u;
    /* Pass length one byte short of the required struct */
    uint8_t short_len = (uint8_t)(sizeof(m1_esp32_status_payload_t) - 1u);
    bool ok = m1_esp32_caps_parse_payload(buf, short_len, &bitmap, fw_name,
                                          &bss, &heap);

    TEST_ASSERT_FALSE(ok);
    /* Outputs must not have been modified */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0xDEADBEEFDEADBEEF), bitmap);
    TEST_ASSERT_EQUAL_STRING("unchanged", fw_name);
    TEST_ASSERT_EQUAL_UINT32(0xABCDu, bss);
    TEST_ASSERT_EQUAL_UINT32(0x1234u, heap);
}

void test_parse_zero_length_returns_false(void)
{
    uint8_t buf[64] = {0};
    uint64_t bitmap = UINT64_C(0xDEADBEEFDEADBEEF);
    char     fw_name[32] = "unchanged";
    uint32_t bss = 1u, heap = 1u;

    bool ok = m1_esp32_caps_parse_payload(buf, 0, &bitmap, fw_name, &bss, &heap);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0xDEADBEEFDEADBEEF), bitmap);
}

void test_parse_wrong_proto_ver_returns_false(void)
{
    uint8_t buf[64];
    make_payload(buf, (uint8_t)(M1_ESP32_CAPS_PROTO_VER + 1u),
                 M1_ESP32_CAP_WIFI_SCAN, "fw", 0u, 0u);

    uint64_t bitmap   = UINT64_C(0xDEADBEEFDEADBEEF);
    char     fw_name[32] = "unchanged";
    uint32_t bss = 0u, heap = 0u;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0xDEADBEEFDEADBEEF), bitmap);
}

/* =========================================================================
 * fw_name null-termination guarantee
 * =========================================================================*/

void test_parse_fw_name_always_null_terminated(void)
{
    uint8_t buf[64];
    /* Fill fw_name field with 31 printable chars + no NUL */
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, 0u, NULL, 0u, 0u);
    /* bytes 9-39 = 31 'A' chars; byte 40 (last of the 32-byte fw_name field) = 'B'
     * This simulates a firmware that forgot the NUL terminator */
    memset(&buf[9], 'A', 31);
    buf[40] = 'B';

    uint64_t bitmap = 0;
    char     fw_name[32];
    uint32_t bss = 0, heap = 0;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);

    TEST_ASSERT_TRUE(ok);
    /* strncpy(dst, src, 31) + forced NUL at [31] must ensure termination */
    TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)fw_name[31]);
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
                "Two capability bits share a bit position");
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
        /* Power-of-two check: c & (c-1) == 0 */
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(UINT64_C(0), c & (c - UINT64_C(1)),
            "Cap bit is not a power of two");
    }
}

/* =========================================================================
 * Composite profile macros: sanity checks
 * =========================================================================*/

void test_sin360_profile_has_expected_caps(void)
{
    const uint64_t p = M1_ESP32_CAP_PROFILE_SIN360;
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_ATTACK);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_SPAM);
    /* SiN360 does NOT support WiFi connect or BLE HID */
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0), p & M1_ESP32_CAP_BLE_HID);
}

void test_at_c3_profile_has_expected_caps(void)
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

void test_neddy299_profile_extends_at_c3(void)
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
 * Parse bitmap field byte-order (little-endian)
 * =========================================================================*/

void test_parse_bitmap_little_endian(void)
{
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    buf[0] = M1_ESP32_CAPS_PROTO_VER;
    /* Manually write 0x0102030405060708 in LE at bytes 1-8:
     * byte 1 = LSB (0x08), byte 8 = MSB (0x01) */
    buf[1] = 0x08; buf[2] = 0x07; buf[3] = 0x06; buf[4] = 0x05;
    buf[5] = 0x04; buf[6] = 0x03; buf[7] = 0x02; buf[8] = 0x01;

    uint64_t bitmap = 0;
    char     fw_name[32];
    uint32_t bss = 0, heap = 0;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0x0102030405060708), bitmap);
}

/* =========================================================================
 * New fields: bss_bytes and free_heap_bytes
 * =========================================================================*/

void test_parse_bss_and_heap_round_trip(void)
{
    uint8_t buf[64];
    /* Use representative values: ~128 KB BSS, ~180 KB free heap */
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_WIFI_SCAN,
                 "SiN360-test", 0x00020000u, 0x0002D000u);

    uint64_t bitmap = 0;
    char     fw_name[32];
    uint32_t bss = 0, heap = 0;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(0x00020000u, bss);
    TEST_ASSERT_EQUAL_UINT32(0x0002D000u, heap);
}

void test_parse_bss_heap_little_endian(void)
{
    /* Manually build a payload and verify byte-order interpretation.
     * With 64-bit bitmap (bytes 1-8), fw_name (bytes 9-40),
     * bss_bytes at bytes 41-44, free_heap at bytes 45-48. */
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    buf[0] = M1_ESP32_CAPS_PROTO_VER;
    /* bss_bytes = 0x04030201 at bytes 41-44 LE */
    buf[41] = 0x01; buf[42] = 0x02; buf[43] = 0x03; buf[44] = 0x04;
    /* free_heap = 0x08070605 at bytes 45-48 LE */
    buf[45] = 0x05; buf[46] = 0x06; buf[47] = 0x07; buf[48] = 0x08;

    uint64_t bitmap = 0;
    char     fw_name[32];
    uint32_t bss = 0, heap = 0;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(0x04030201u, bss);
    TEST_ASSERT_EQUAL_UINT32(0x08070605u, heap);
}

void test_payload_struct_size(void)
{
    /* Confirm the packed struct is exactly 49 bytes:
     * 1 (proto_ver) + 8 (cap_bitmap) + 32 (fw_name) + 4 (bss) + 4 (heap) */
    TEST_ASSERT_EQUAL_UINT32(49u, (uint32_t)sizeof(m1_esp32_status_payload_t));
}

void test_parse_zero_bss_and_heap_are_valid(void)
{
    /* Report-0-if-unavailable is explicitly documented; zero must round-trip */
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_BLE_SCAN,
                 "fw-minimal", 0u, 0u);

    uint64_t bitmap = 0;
    char     fw_name[32];
    uint32_t bss = 0xFFFFu, heap = 0xFFFFu;
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name, &bss, &heap);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(0u, bss);
    TEST_ASSERT_EQUAL_UINT32(0u, heap);
}

/* =========================================================================
 * main
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_parse_valid_sin360);
    RUN_TEST(test_parse_valid_at_c3);
    RUN_TEST(test_parse_valid_all_caps);
    RUN_TEST(test_parse_too_short_returns_false);
    RUN_TEST(test_parse_zero_length_returns_false);
    RUN_TEST(test_parse_wrong_proto_ver_returns_false);
    RUN_TEST(test_parse_fw_name_always_null_terminated);
    RUN_TEST(test_cap_bits_are_unique);
    RUN_TEST(test_cap_bits_are_single_bit_powers_of_two);
    RUN_TEST(test_sin360_profile_has_expected_caps);
    RUN_TEST(test_at_c3_profile_has_expected_caps);
    RUN_TEST(test_neddy299_profile_extends_at_c3);
    RUN_TEST(test_parse_bitmap_little_endian);
    RUN_TEST(test_parse_bss_and_heap_round_trip);
    RUN_TEST(test_parse_bss_heap_little_endian);
    RUN_TEST(test_payload_struct_size);
    RUN_TEST(test_parse_zero_bss_and_heap_are_valid);

    return UNITY_END();
}
