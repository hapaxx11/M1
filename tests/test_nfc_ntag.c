/* See COPYING.txt for license details. */

/**
 * test_nfc_ntag.c
 *
 * Unit tests for the pure-logic NTAG helpers in nfc_ntag.h/.c:
 *   ntag_page_count_from_size()   — GET_VERSION size byte → total page count
 *   ntag_generate_amiibo_pwd()    — Amiibo XOR-derived PWD + PACK
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "nfc_ntag.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * ntag_page_count_from_size
 * =========================================================================*/

void test_page_count_ntag213(void)
{
    /* NTAG213: size byte 0x0F → 45 pages */
    TEST_ASSERT_EQUAL_UINT16(45, ntag_page_count_from_size(0x0F));
}

void test_page_count_ntag215(void)
{
    /* NTAG215: size byte 0x11 → 135 pages */
    TEST_ASSERT_EQUAL_UINT16(135, ntag_page_count_from_size(0x11));
}

void test_page_count_ntag216(void)
{
    /* NTAG216: size byte 0x13 → 231 pages */
    TEST_ASSERT_EQUAL_UINT16(231, ntag_page_count_from_size(0x13));
}

void test_page_count_unknown_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, ntag_page_count_from_size(0x00));
}

void test_page_count_adjacent_to_known_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, ntag_page_count_from_size(0x0E));
    TEST_ASSERT_EQUAL_UINT16(0, ntag_page_count_from_size(0x10));
    TEST_ASSERT_EQUAL_UINT16(0, ntag_page_count_from_size(0x12));
    TEST_ASSERT_EQUAL_UINT16(0, ntag_page_count_from_size(0x14));
}

void test_page_count_max_byte_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, ntag_page_count_from_size(0xFF));
}

/* =========================================================================
 * ntag_generate_amiibo_pwd — algorithm verification
 *
 * Algorithm:
 *   pwd[0] = 0xAA ^ uid[1] ^ uid[3]
 *   pwd[1] = 0x55 ^ uid[2] ^ uid[4]
 *   pwd[2] = 0xAA ^ uid[3] ^ uid[5]
 *   pwd[3] = 0x55 ^ uid[4] ^ uid[6]
 *   pack[0] = 0x80
 *   pack[1] = 0x80
 * =========================================================================*/

void test_amiibo_pwd_known_uid(void)
{
    /* Real Amiibo UID from published test vectors */
    const uint8_t uid[7]  = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
    uint8_t pwd[4] = {0};
    uint8_t pack[2] = {0};

    TEST_ASSERT_TRUE(ntag_generate_amiibo_pwd(uid, 7, pwd, pack));

    /* Manually derived expected values */
    TEST_ASSERT_EQUAL_HEX8(0xAA ^ uid[1] ^ uid[3], pwd[0]);  /* 0xAA ^ 0xA1 ^ 0xC3 = 0x68 */
    TEST_ASSERT_EQUAL_HEX8(0x55 ^ uid[2] ^ uid[4], pwd[1]);  /* 0x55 ^ 0xB2 ^ 0xD4 = 0x37 */
    TEST_ASSERT_EQUAL_HEX8(0xAA ^ uid[3] ^ uid[5], pwd[2]);  /* 0xAA ^ 0xC3 ^ 0xE5 = 0x28 */
    TEST_ASSERT_EQUAL_HEX8(0x55 ^ uid[4] ^ uid[6], pwd[3]);  /* 0x55 ^ 0xD4 ^ 0xF6 = 0x7F */
    TEST_ASSERT_EQUAL_HEX8(0x80, pack[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, pack[1]);
}

void test_amiibo_pwd_all_zeros_uid(void)
{
    const uint8_t uid[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t pwd[4] = {0};
    uint8_t pack[2] = {0};

    TEST_ASSERT_TRUE(ntag_generate_amiibo_pwd(uid, 7, pwd, pack));

    /* XOR with all-zero UID: just the constants */
    TEST_ASSERT_EQUAL_HEX8(0xAA, pwd[0]);
    TEST_ASSERT_EQUAL_HEX8(0x55, pwd[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, pwd[2]);
    TEST_ASSERT_EQUAL_HEX8(0x55, pwd[3]);
    TEST_ASSERT_EQUAL_HEX8(0x80, pack[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, pack[1]);
}

void test_amiibo_pwd_all_ff_uid(void)
{
    const uint8_t uid[7] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t pwd[4] = {0};
    uint8_t pack[2] = {0};

    TEST_ASSERT_TRUE(ntag_generate_amiibo_pwd(uid, 7, pwd, pack));

    /* 0xAA ^ 0xFF ^ 0xFF = 0xAA; 0x55 ^ 0xFF ^ 0xFF = 0x55 */
    TEST_ASSERT_EQUAL_HEX8(0xAA, pwd[0]);
    TEST_ASSERT_EQUAL_HEX8(0x55, pwd[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, pwd[2]);
    TEST_ASSERT_EQUAL_HEX8(0x55, pwd[3]);
    TEST_ASSERT_EQUAL_HEX8(0x80, pack[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, pack[1]);
}

void test_amiibo_pwd_null_uid_rejected(void)
{
    uint8_t pwd[4]  = {0};
    uint8_t pack[2] = {0};
    TEST_ASSERT_FALSE(ntag_generate_amiibo_pwd(NULL, 7, pwd, pack));
}

void test_amiibo_pwd_null_pwd_rejected(void)
{
    const uint8_t uid[7] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t pack[2] = {0};
    TEST_ASSERT_FALSE(ntag_generate_amiibo_pwd(uid, 7, NULL, pack));
}

void test_amiibo_pwd_null_pack_rejected(void)
{
    const uint8_t uid[7] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t pwd[4] = {0};
    TEST_ASSERT_FALSE(ntag_generate_amiibo_pwd(uid, 7, pwd, NULL));
}

void test_amiibo_pwd_wrong_uid_len_rejected(void)
{
    const uint8_t uid[7] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t pwd[4]  = {0};
    uint8_t pack[2] = {0};
    /* uid_len != 7 must return false */
    TEST_ASSERT_FALSE(ntag_generate_amiibo_pwd(uid, 4, pwd, pack));
    TEST_ASSERT_FALSE(ntag_generate_amiibo_pwd(uid, 0, pwd, pack));
    TEST_ASSERT_FALSE(ntag_generate_amiibo_pwd(uid, 10, pwd, pack));
}

void test_amiibo_pwd_outputs_unchanged_on_failure(void)
{
    const uint8_t uid[7] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t pwd[4]  = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t pack[2] = {0x11, 0x22};

    /* Passing wrong uid_len; outputs must stay as initialized */
    TEST_ASSERT_FALSE(ntag_generate_amiibo_pwd(uid, 6, pwd, pack));
    TEST_ASSERT_EQUAL_HEX8(0xAA, pwd[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11, pack[0]);
}

/* =========================================================================
 * main
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* ntag_page_count_from_size */
    RUN_TEST(test_page_count_ntag213);
    RUN_TEST(test_page_count_ntag215);
    RUN_TEST(test_page_count_ntag216);
    RUN_TEST(test_page_count_unknown_returns_zero);
    RUN_TEST(test_page_count_adjacent_to_known_is_zero);
    RUN_TEST(test_page_count_max_byte_is_zero);

    /* ntag_generate_amiibo_pwd */
    RUN_TEST(test_amiibo_pwd_known_uid);
    RUN_TEST(test_amiibo_pwd_all_zeros_uid);
    RUN_TEST(test_amiibo_pwd_all_ff_uid);
    RUN_TEST(test_amiibo_pwd_null_uid_rejected);
    RUN_TEST(test_amiibo_pwd_null_pwd_rejected);
    RUN_TEST(test_amiibo_pwd_null_pack_rejected);
    RUN_TEST(test_amiibo_pwd_wrong_uid_len_rejected);
    RUN_TEST(test_amiibo_pwd_outputs_unchanged_on_failure);

    return UNITY_END();
}
