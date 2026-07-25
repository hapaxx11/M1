/* See COPYING.txt for license details. */

/*
 * test_esp32_spi_bin.c
 *
 * Host-side regression tests for esp32_spi_bin_copy() — the binary-safe copy
 * that replaced the NUL-truncating strcpy() on the ESP32 SPI-HD receive path.
 *
 * Regression target (PR #669 deferred fix): CD3 native M1_RPC frames (magic
 * 0x4D31) contain embedded 0x00 bytes in their header/CRC.  The old strcpy()
 * path truncated such frames at the first NUL, so CD3 was misdetected as
 * "Unknown (fallback)".  These tests prove the replacement preserves every
 * byte — including embedded NULs — exactly as reported by the slave.
 *
 * M1 Project
 */

#include <string.h>
#include "unity.h"
#include "esp32_spi_bin.h"

void setUp(void) {}
void tearDown(void) {}

/* A representative M1_RPC PING response frame: magic 0x4D31, ver 1, RESP 0x02,
 * msg_id 0x0001, payload_len 0x0004, 4-byte cookie, CRC16.  Note the multiple
 * embedded 0x00 bytes (indices 5, 7, and potentially the CRC). */
static const uint8_t k_rpc_frame[] = {
    0x31, 0x4D, 0x01, 0x02, 0x01, 0x00, 0x04, 0x00,
    0x4D, 0x31, 0x50, 0x49, 0x12, 0x34
};

void test_copy_preserves_embedded_nul_bytes(void)
{
    uint8_t dst[64];
    memset(dst, 0xAA, sizeof(dst));

    size_t n = esp32_spi_bin_copy(dst, sizeof(dst),
                                  k_rpc_frame, sizeof(k_rpc_frame));

    TEST_ASSERT_EQUAL_size_t(sizeof(k_rpc_frame), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_rpc_frame, dst, sizeof(k_rpc_frame));
}

/* Demonstrates the exact defect: strlen()/strcpy() would stop at index 5
 * (the first 0x00), losing the msg_id high byte, payload, and CRC. */
void test_copy_beats_strlen_truncation(void)
{
    size_t strlen_would_copy = strlen((const char *)k_rpc_frame);
    uint8_t dst[64];

    size_t n = esp32_spi_bin_copy(dst, sizeof(dst),
                                  k_rpc_frame, sizeof(k_rpc_frame));

    TEST_ASSERT_TRUE(strlen_would_copy < sizeof(k_rpc_frame));
    TEST_ASSERT_EQUAL_size_t(sizeof(k_rpc_frame), n);
    TEST_ASSERT_TRUE(n > strlen_would_copy);
}

void test_copy_caps_at_dst_capacity(void)
{
    uint8_t dst[6];
    memset(dst, 0x00, sizeof(dst));

    size_t n = esp32_spi_bin_copy(dst, sizeof(dst),
                                  k_rpc_frame, sizeof(k_rpc_frame));

    TEST_ASSERT_EQUAL_size_t(sizeof(dst), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_rpc_frame, dst, sizeof(dst));
}

void test_copy_zero_src_len_copies_nothing(void)
{
    uint8_t dst[8];
    memset(dst, 0x5A, sizeof(dst));

    size_t n = esp32_spi_bin_copy(dst, sizeof(dst), k_rpc_frame, 0u);

    TEST_ASSERT_EQUAL_size_t(0u, n);
    TEST_ASSERT_EQUAL_UINT8(0x5A, dst[0]);  /* untouched */
}

void test_copy_null_args_are_safe(void)
{
    uint8_t dst[8];

    TEST_ASSERT_EQUAL_size_t(0u, esp32_spi_bin_copy(NULL, 8u,
                                                    k_rpc_frame, 4u));
    TEST_ASSERT_EQUAL_size_t(0u, esp32_spi_bin_copy(dst, 8u, NULL, 4u));
    TEST_ASSERT_EQUAL_size_t(0u, esp32_spi_bin_copy(dst, 0u,
                                                    k_rpc_frame, 4u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_copy_preserves_embedded_nul_bytes);
    RUN_TEST(test_copy_beats_strlen_truncation);
    RUN_TEST(test_copy_caps_at_dst_capacity);
    RUN_TEST(test_copy_zero_src_len_copies_nothing);
    RUN_TEST(test_copy_null_args_are_safe);
    return UNITY_END();
}
