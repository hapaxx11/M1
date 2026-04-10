/* See COPYING.txt for license details. */

/*
 * test_bit_util.c
 *
 * Unit tests for bit_util.c using the Unity test framework.
 * Tests CRC, parity, byte manipulation, and whitening functions.
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "bit_util.h"
#include <string.h>

/* Unity setup / teardown (required stubs) */
void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * reverse8 / reverse32
 * =================================================================== */

void test_reverse8_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0x00, reverse8(0x00));
}

void test_reverse8_0x01(void)
{
    TEST_ASSERT_EQUAL_UINT8(0x80, reverse8(0x01));
}

void test_reverse8_0xA5(void)
{
    /* 0xA5 = 1010 0101 → 1010 0101 = 0xA5 (palindrome) */
    TEST_ASSERT_EQUAL_UINT8(0xA5, reverse8(0xA5));
}

void test_reverse8_0x0F(void)
{
    /* 0x0F = 0000 1111 → 1111 0000 = 0xF0 */
    TEST_ASSERT_EQUAL_UINT8(0xF0, reverse8(0x0F));
}

void test_reverse8_roundtrip(void)
{
    for (unsigned i = 0; i < 256; i++) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, reverse8(reverse8((uint8_t)i)));
    }
}

void test_reverse32_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0x00000000, reverse32(0x00000000));
}

void test_reverse32_one(void)
{
    TEST_ASSERT_EQUAL_UINT32(0x80000000, reverse32(0x00000001));
}

void test_reverse32_roundtrip(void)
{
    uint32_t val = 0xDEADBEEF;
    TEST_ASSERT_EQUAL_UINT32(val, reverse32(reverse32(val)));
}

/* ===================================================================
 * reflect4 / reflect_bytes / reflect_nibbles
 * =================================================================== */

void test_reflect4_0x12(void)
{
    /* Nibbles: high=1, low=2 → reflect each: 8, 4 → 0x84 */
    TEST_ASSERT_EQUAL_UINT8(0x84, reflect4(0x12));
}

void test_reflect_bytes_reverses_bits(void)
{
    uint8_t msg[] = {0xFF, 0x00, 0xAA};
    uint8_t exp[] = {0xFF, 0x00, 0x55};
    reflect_bytes(msg, 3);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp, msg, 3);
}

/* ===================================================================
 * CRC functions
 * =================================================================== */

void test_crc8_poly31(void)
{
    /* CRC-8 with poly=0x31 (MSB-first), init=0x00 over "123456789" */
    uint8_t data[] = "123456789";
    uint8_t crc = crc8(data, 9, 0x31, 0x00);
    TEST_ASSERT_EQUAL_UINT8(0xA2, crc);
}

void test_crc8_empty(void)
{
    uint8_t crc = crc8(NULL, 0, 0x31, 0x00);
    TEST_ASSERT_EQUAL_UINT8(0x00, crc);
}

void test_crc16_ccitt(void)
{
    /* CRC-16/CCITT-FALSE: poly=0x1021, init=0xFFFF, check for "123456789" = 0x29B1 */
    uint8_t data[] = "123456789";
    uint16_t crc = crc16(data, 9, 0x1021, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT16(0x29B1, crc);
}

void test_crc4_basic(void)
{
    /* CRC-4 with poly=0x03 (MSB-first), init=0x00 over {0x12, 0x34} = 0x0C */
    uint8_t data[] = {0x12, 0x34};
    uint8_t crc = crc4(data, 2, 0x03, 0x00);
    TEST_ASSERT_EQUAL_UINT8(0x0C, crc);
}

void test_crc7_basic(void)
{
    /* CRC-7/MMC: poly=0x09, init=0x00, check for "123456789" = 0x75 */
    uint8_t data[] = "123456789";
    uint8_t crc = crc7(data, 9, 0x09, 0x00);
    TEST_ASSERT_EQUAL_UINT8(0x75, crc);
}

/* ===================================================================
 * Parity functions
 * =================================================================== */

void test_parity8_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, parity8(0x00));
}

void test_parity8_one(void)
{
    TEST_ASSERT_EQUAL_INT(1, parity8(0x01));
}

void test_parity8_0xFF(void)
{
    /* 8 bits set → even parity = 0 */
    TEST_ASSERT_EQUAL_INT(0, parity8(0xFF));
}

void test_parity8_0x03(void)
{
    /* 2 bits set → even parity = 0 */
    TEST_ASSERT_EQUAL_INT(0, parity8(0x03));
}

void test_parity_bytes_single(void)
{
    uint8_t msg[] = {0x01};
    TEST_ASSERT_EQUAL_INT(1, parity_bytes(msg, 1));
}

void test_parity_bytes_multiple(void)
{
    uint8_t msg[] = {0x01, 0x01};
    /* Two bytes, each with parity 1 → XOR = 0 */
    TEST_ASSERT_EQUAL_INT(0, parity_bytes(msg, 2));
}

/* ===================================================================
 * xor_bytes / add_bytes / add_nibbles
 * =================================================================== */

void test_xor_bytes_basic(void)
{
    uint8_t msg[] = {0xAA, 0x55};
    TEST_ASSERT_EQUAL_UINT8(0xFF, xor_bytes(msg, 2));
}

void test_xor_bytes_same(void)
{
    uint8_t msg[] = {0x42, 0x42};
    TEST_ASSERT_EQUAL_UINT8(0x00, xor_bytes(msg, 2));
}

void test_add_bytes_basic(void)
{
    uint8_t msg[] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT(10, add_bytes(msg, 4));
}

void test_add_nibbles_basic(void)
{
    /* 0x12 → nibbles 1 + 2 = 3, 0x34 → 3 + 4 = 7, total = 10 */
    uint8_t msg[] = {0x12, 0x34};
    TEST_ASSERT_EQUAL_INT(10, add_nibbles(msg, 2));
}

/* ===================================================================
 * Whitening functions
 * =================================================================== */

void test_ccitt_whitening_zeros(void)
{
    uint8_t buf[16] = {0};
    uint8_t expected[16] = {
        0xff, 0x87, 0xb8, 0x59, 0xb7, 0xa1, 0xcc, 0x24,
        0x57, 0x5e, 0x4b, 0x9c, 0x0e, 0xe9, 0xea, 0x50
    };
    ccitt_whitening(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, 16);
}

void test_ibm_whitening_zeros(void)
{
    uint8_t buf[16] = {0};
    uint8_t expected[16] = {
        0xff, 0xe1, 0x1d, 0x9a, 0xed, 0x85, 0x33, 0x24,
        0xea, 0x7a, 0xd2, 0x39, 0x70, 0x97, 0x57, 0x0a
    };
    ibm_whitening(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, 16);
}

void test_ccitt_whitening_involution(void)
{
    /* Whitening applied twice should return original data */
    uint8_t original[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    uint8_t buf[8];
    memcpy(buf, original, 8);
    ccitt_whitening(buf, 8);
    ccitt_whitening(buf, 8);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original, buf, 8);
}

void test_ibm_whitening_involution(void)
{
    uint8_t original[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    uint8_t buf[8];
    memcpy(buf, original, 8);
    ibm_whitening(buf, 8);
    ibm_whitening(buf, 8);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original, buf, 8);
}

/* ===================================================================
 * LFSR digest functions
 * =================================================================== */

void test_lfsr_digest8_deterministic(void)
{
    uint8_t msg[] = {0x01, 0x02, 0x03};
    uint8_t d1 = lfsr_digest8(msg, 3, 0x93, 0x00);
    uint8_t d2 = lfsr_digest8(msg, 3, 0x93, 0x00);
    TEST_ASSERT_EQUAL_UINT8(d1, d2);
}

void test_lfsr_digest16_deterministic(void)
{
    uint8_t msg[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint16_t d1 = lfsr_digest16(msg, 4, 0x2D, 0x00);
    uint16_t d2 = lfsr_digest16(msg, 4, 0x2D, 0x00);
    TEST_ASSERT_EQUAL_UINT16(d1, d2);
}

/* ===================================================================
 * extract_bytes_uart
 * =================================================================== */

void test_extract_bytes_uart_basic(void)
{
    /* Use the same test vector as bit_util.c's built-in self-test:
     * uart[] = {0x7F, 0xD9, 0x90} → extracts 2 bytes: 0xFF, 0x33
     * The implementation uses reverse8() on extracted data bits. */
    uint8_t uart[] = {0x7F, 0xD9, 0x90};
    uint8_t bytes[2] = {0};
    unsigned n = extract_bytes_uart(uart, 0, 24, bytes);

    TEST_ASSERT_EQUAL_UINT(2, n);
    TEST_ASSERT_EQUAL_UINT8(0xFF, bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(0x33, bytes[1]);
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* reverse / reflect */
    RUN_TEST(test_reverse8_zero);
    RUN_TEST(test_reverse8_0x01);
    RUN_TEST(test_reverse8_0xA5);
    RUN_TEST(test_reverse8_0x0F);
    RUN_TEST(test_reverse8_roundtrip);
    RUN_TEST(test_reverse32_zero);
    RUN_TEST(test_reverse32_one);
    RUN_TEST(test_reverse32_roundtrip);
    RUN_TEST(test_reflect4_0x12);
    RUN_TEST(test_reflect_bytes_reverses_bits);

    /* CRC */
    RUN_TEST(test_crc8_poly31);
    RUN_TEST(test_crc8_empty);
    RUN_TEST(test_crc16_ccitt);
    RUN_TEST(test_crc4_basic);
    RUN_TEST(test_crc7_basic);

    /* parity */
    RUN_TEST(test_parity8_zero);
    RUN_TEST(test_parity8_one);
    RUN_TEST(test_parity8_0xFF);
    RUN_TEST(test_parity8_0x03);
    RUN_TEST(test_parity_bytes_single);
    RUN_TEST(test_parity_bytes_multiple);

    /* xor / add */
    RUN_TEST(test_xor_bytes_basic);
    RUN_TEST(test_xor_bytes_same);
    RUN_TEST(test_add_bytes_basic);
    RUN_TEST(test_add_nibbles_basic);

    /* whitening */
    RUN_TEST(test_ccitt_whitening_zeros);
    RUN_TEST(test_ibm_whitening_zeros);
    RUN_TEST(test_ccitt_whitening_involution);
    RUN_TEST(test_ibm_whitening_involution);

    /* LFSR */
    RUN_TEST(test_lfsr_digest8_deterministic);
    RUN_TEST(test_lfsr_digest16_deterministic);

    /* UART extract */
    RUN_TEST(test_extract_bytes_uart_basic);

    return UNITY_END();
}
