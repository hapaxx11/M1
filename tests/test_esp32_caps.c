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
 *   - Fallback estimate constants satisfy expected ordering invariants
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
                         uint32_t bitmap,
                         const char *fw_name)
{
    memset(buf, 0, 64);
    buf[0] = proto_ver;
    /* little-endian bitmap at bytes 1-4 */
    buf[1] = (uint8_t)(bitmap        & 0xFFu);
    buf[2] = (uint8_t)((bitmap >>  8) & 0xFFu);
    buf[3] = (uint8_t)((bitmap >> 16) & 0xFFu);
    buf[4] = (uint8_t)((bitmap >> 24) & 0xFFu);
    /* fw_name at bytes 5-36 */
    if (fw_name)
        strncpy((char *)&buf[5], fw_name, 31);
}

/* =========================================================================
 * Parse: valid payload
 * =========================================================================*/

void test_parse_valid_sin360(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_ESP32_CAP_PROFILE_SIN360, "SiN360-0.9.6");

    uint32_t bitmap = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(M1_ESP32_CAP_PROFILE_SIN360, bitmap);
    TEST_ASSERT_EQUAL_STRING("SiN360-0.9.6", fw_name);
}

void test_parse_valid_at_c3(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER,
                 M1_ESP32_CAP_PROFILE_AT_C3, "AT-bedge117-2.0.2");

    uint32_t bitmap = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(M1_ESP32_CAP_PROFILE_AT_C3, bitmap);
    TEST_ASSERT_EQUAL_STRING("AT-bedge117-2.0.2", fw_name);
}

void test_parse_valid_all_caps(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, 0xFFFFFFFFu, "future-fw-1.0");

    uint32_t bitmap = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, bitmap);
    TEST_ASSERT_EQUAL_STRING("future-fw-1.0", fw_name);
}

/* =========================================================================
 * Parse: error paths
 * =========================================================================*/

void test_parse_too_short_returns_false(void)
{
    uint8_t buf[64];
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, M1_ESP32_CAP_WIFI_SCAN, "fw");

    uint32_t bitmap   = 0xDEADBEEFu;
    char     fw_name[32] = "unchanged";
    /* Pass length one byte short of the required struct */
    uint8_t short_len = (uint8_t)(sizeof(m1_esp32_status_payload_t) - 1u);
    bool ok = m1_esp32_caps_parse_payload(buf, short_len, &bitmap, fw_name);

    TEST_ASSERT_FALSE(ok);
    /* Outputs must not have been modified */
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, bitmap);
    TEST_ASSERT_EQUAL_STRING("unchanged", fw_name);
}

void test_parse_zero_length_returns_false(void)
{
    uint8_t buf[64] = {0};
    uint32_t bitmap = 0xDEADBEEFu;
    char     fw_name[32] = "unchanged";

    bool ok = m1_esp32_caps_parse_payload(buf, 0, &bitmap, fw_name);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, bitmap);
}

void test_parse_wrong_proto_ver_returns_false(void)
{
    uint8_t buf[64];
    make_payload(buf, (uint8_t)(M1_ESP32_CAPS_PROTO_VER + 1u),
                 M1_ESP32_CAP_WIFI_SCAN, "fw");

    uint32_t bitmap   = 0xDEADBEEFu;
    char     fw_name[32] = "unchanged";
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, bitmap);
}

/* =========================================================================
 * fw_name null-termination guarantee
 * =========================================================================*/

void test_parse_fw_name_always_null_terminated(void)
{
    uint8_t buf[64];
    /* Fill fw_name field with 31 printable chars + no NUL */
    make_payload(buf, M1_ESP32_CAPS_PROTO_VER, 0u, NULL);
    /* bytes 5-35 = 31 'A' chars; byte 36 (last of the 32-byte field) = 'B'
     * This simulates a firmware that forgot the NUL terminator */
    memset(&buf[5], 'A', 31);
    buf[36] = 'B';

    uint32_t bitmap = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name);

    TEST_ASSERT_TRUE(ok);
    /* strncpy(dst, src, 31) + forced NUL at [31] must ensure termination */
    TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)fw_name[31]);
}

/* =========================================================================
 * Capability bit constants: no two bits overlap
 * =========================================================================*/

void test_cap_bits_are_unique(void)
{
    const uint32_t caps[] = {
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
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, caps[i] & caps[j],
                "Two capability bits share a bit position");
        }
    }
}

void test_cap_bits_are_single_bit_powers_of_two(void)
{
    const uint32_t caps[] = {
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
        uint32_t c = caps[i];
        TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(0u, c, "Cap bit is zero");
        /* Power-of-two check: c & (c-1) == 0 */
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, c & (c - 1u),
            "Cap bit is not a power of two");
    }
}

/* =========================================================================
 * Composite profile macros: sanity checks
 * =========================================================================*/

void test_sin360_profile_has_expected_caps(void)
{
    const uint32_t p = M1_ESP32_CAP_PROFILE_SIN360;
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_WIFI_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_WIFI_ATTACK);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_BLE_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_BLE_SPAM);
    /* SiN360 does NOT support WiFi connect or BLE HID */
    TEST_ASSERT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_BLE_HID);
}

void test_at_c3_profile_has_expected_caps(void)
{
    const uint32_t p = M1_ESP32_CAP_PROFILE_AT_C3;
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_WIFI_CONNECT);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_BLE_HID);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_802154);
    /* AT/C3 does NOT support SiN360 sniffers or attacks */
    TEST_ASSERT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_WIFI_SNIFF);
    TEST_ASSERT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_WIFI_ATTACK);
    TEST_ASSERT_EQUAL_UINT32(0u, p & M1_ESP32_CAP_BLE_SPAM);
}

void test_neddy299_profile_extends_at_c3(void)
{
    const uint32_t c3     = M1_ESP32_CAP_PROFILE_AT_C3;
    const uint32_t neddy  = M1_ESP32_CAP_PROFILE_AT_NEDDY299;
    /* neddy299 is a strict superset of AT_C3 */
    TEST_ASSERT_EQUAL_UINT32(c3, neddy & c3);
    /* Adds station scan and attack */
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, neddy & M1_ESP32_CAP_WIFI_STA_SCAN);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, neddy & M1_ESP32_CAP_WIFI_ATTACK);
}

/* =========================================================================
 * Parse bitmap field byte-order (little-endian)
 * =========================================================================*/

void test_parse_bitmap_little_endian(void)
{
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    buf[0] = M1_ESP32_CAPS_PROTO_VER;
    /* Manually write 0x01020304 in LE: byte1=0x04, byte2=0x03, ... */
    buf[1] = 0x04;
    buf[2] = 0x03;
    buf[3] = 0x02;
    buf[4] = 0x01;

    uint32_t bitmap = 0;
    char     fw_name[32];
    bool ok = m1_esp32_caps_parse_payload(buf, sizeof(m1_esp32_status_payload_t),
                                          &bitmap, fw_name);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(0x01020304u, bitmap);
}

/* =========================================================================
 * Payload struct size
 * =========================================================================*/

void test_payload_struct_size(void)
{
    /* Confirm the packed struct is exactly 37 bytes:
     * 1 (proto_ver) + 4 (cap_bitmap) + 32 (fw_name) */
    TEST_ASSERT_EQUAL_UINT32(37u, (uint32_t)sizeof(m1_esp32_status_payload_t));
}

/* =========================================================================
 * Fallback estimate constants: sanity checks
 *
 * These tests are compile-time assertions expressed as runtime Unity tests
 * so they appear in the CI report and any regression is immediately obvious.
 * The actual numeric values are derived from source-code analysis of
 * sincere360/M1_SiN360_ESP32 v0.9.0.8 (ESP-IDF 5.5.4) and
 * bedge117/esp32-at-monstatek-m1 v2.0.2 (ESP-AT v4.0.0.0).
 * =========================================================================*/

void test_fallback_constants_are_nonzero(void)
{
    TEST_ASSERT_GREATER_THAN_UINT32(0u, M1_ESP32_FALLBACK_BSS_SIN360);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, M1_ESP32_FALLBACK_HEAP_SIN360);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, M1_ESP32_FALLBACK_BSS_AT);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, M1_ESP32_FALLBACK_HEAP_AT);
}

void test_fallback_at_bss_exceeds_sin360(void)
{
    /* AT firmware carries full AT command infrastructure + SPI ring buffers
     * + BLE HID + 802.15.4 state → larger static footprint than SiN360. */
    TEST_ASSERT_GREATER_THAN_UINT32(M1_ESP32_FALLBACK_BSS_SIN360,
                                    M1_ESP32_FALLBACK_BSS_AT);
}

void test_fallback_sin360_heap_exceeds_at(void)
{
    /* SiN360's smaller static footprint leaves more runtime heap free. */
    TEST_ASSERT_GREATER_THAN_UINT32(M1_ESP32_FALLBACK_HEAP_AT,
                                    M1_ESP32_FALLBACK_HEAP_SIN360);
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
    RUN_TEST(test_payload_struct_size);
    RUN_TEST(test_fallback_constants_are_nonzero);
    RUN_TEST(test_fallback_at_bss_exceeds_sin360);
    RUN_TEST(test_fallback_sin360_heap_exceeds_at);

    return UNITY_END();
}
