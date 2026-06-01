/* See COPYING.txt for license details. */

/**
 * test_wifi_mac_utils.c
 *
 * Unit tests for the pure-logic helpers in wifi_mac_utils.h / wifi_mac_utils.c:
 *   wifi_mac_is_zero()  — all-zero MAC check
 *   wifi_mac_format()   — 6-byte MAC → "XX:XX:XX:XX:XX:XX"
 *   wifi_mac_match()    — byte-exact MAC comparison
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "wifi_mac_utils.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * wifi_mac_is_zero
 * =========================================================================*/

static void test_is_zero_all_zeros(void)
{
    uint8_t mac[6] = {0};
    TEST_ASSERT_TRUE(wifi_mac_is_zero(mac));
}

static void test_is_zero_first_byte_set(void)
{
    uint8_t mac[6] = {0x01, 0, 0, 0, 0, 0};
    TEST_ASSERT_FALSE(wifi_mac_is_zero(mac));
}

static void test_is_zero_last_byte_set(void)
{
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0x01};
    TEST_ASSERT_FALSE(wifi_mac_is_zero(mac));
}

static void test_is_zero_all_ff(void)
{
    uint8_t mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    TEST_ASSERT_FALSE(wifi_mac_is_zero(mac));
}

static void test_is_zero_middle_byte_set(void)
{
    uint8_t mac[6] = {0, 0, 0x80, 0, 0, 0};
    TEST_ASSERT_FALSE(wifi_mac_is_zero(mac));
}

/* =========================================================================
 * wifi_mac_format
 * =========================================================================*/

static void test_format_typical_mac(void)
{
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    char out[18];
    wifi_mac_format(mac, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", out);
}

static void test_format_all_zeros(void)
{
    uint8_t mac[6] = {0};
    char out[18];
    wifi_mac_format(mac, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("00:00:00:00:00:00", out);
}

static void test_format_leading_zeros(void)
{
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    char out[18];
    wifi_mac_format(mac, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("01:02:03:04:05:06", out);
}

static void test_format_small_buffer_truncates(void)
{
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    char out[10] = {0};
    wifi_mac_format(mac, out, sizeof(out));
    /* snprintf truncates; first 9 chars of "AA:BB:CC:DD:EE:FF" = "AA:BB:CC:" */
    TEST_ASSERT_EQUAL(9, strlen(out));
}

static void test_format_null_out_is_safe(void)
{
    uint8_t mac[6] = {0};
    wifi_mac_format(mac, NULL, 18);
    /* Should not crash */
}

static void test_format_zero_len_is_safe(void)
{
    uint8_t mac[6] = {0};
    char out[18] = "unchanged";
    wifi_mac_format(mac, out, 0);
    TEST_ASSERT_EQUAL_STRING("unchanged", out);
}

/* =========================================================================
 * wifi_mac_match
 * =========================================================================*/

static void test_match_identical(void)
{
    uint8_t a[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t b[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    TEST_ASSERT_TRUE(wifi_mac_match(a, b));
}

static void test_match_different_first_byte(void)
{
    uint8_t a[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t b[6] = {0x00, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    TEST_ASSERT_FALSE(wifi_mac_match(a, b));
}

static void test_match_different_last_byte(void)
{
    uint8_t a[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t b[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00};
    TEST_ASSERT_FALSE(wifi_mac_match(a, b));
}

static void test_match_both_zero(void)
{
    uint8_t a[6] = {0};
    uint8_t b[6] = {0};
    TEST_ASSERT_TRUE(wifi_mac_match(a, b));
}

static void test_match_same_pointer(void)
{
    uint8_t a[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    TEST_ASSERT_TRUE(wifi_mac_match(a, a));
}

/* =========================================================================
 * Runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* wifi_mac_is_zero */
    RUN_TEST(test_is_zero_all_zeros);
    RUN_TEST(test_is_zero_first_byte_set);
    RUN_TEST(test_is_zero_last_byte_set);
    RUN_TEST(test_is_zero_all_ff);
    RUN_TEST(test_is_zero_middle_byte_set);

    /* wifi_mac_format */
    RUN_TEST(test_format_typical_mac);
    RUN_TEST(test_format_all_zeros);
    RUN_TEST(test_format_leading_zeros);
    RUN_TEST(test_format_small_buffer_truncates);
    RUN_TEST(test_format_null_out_is_safe);
    RUN_TEST(test_format_zero_len_is_safe);

    /* wifi_mac_match */
    RUN_TEST(test_match_identical);
    RUN_TEST(test_match_different_first_byte);
    RUN_TEST(test_match_different_last_byte);
    RUN_TEST(test_match_both_zero);
    RUN_TEST(test_match_same_pointer);

    return UNITY_END();
}
