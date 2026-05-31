/* See COPYING.txt for license details. */

/**
 * test_mfc_layout.c
 *
 * Unit tests for the pure-logic MFC layout helpers in mfc_layout.h/.c:
 *   mfc_layout_from_sak()     — SAK → sectors + blocks
 *   mfc_sector_first_block()  — sector → first block number
 *   mfc_sector_block_count()  — sector → blocks-in-sector
 *   mfc_uid4_from_nfcid()     — NFCID → 4-byte Crypto-1 UID
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "mfc_layout.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * mfc_layout_from_sak
 * =========================================================================*/

void test_sak_mini(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x09);
    TEST_ASSERT_EQUAL_UINT16(5, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(20, l.total_blocks);
}

void test_sak_1k(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x08);
    TEST_ASSERT_EQUAL_UINT16(16, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(64, l.total_blocks);
}

void test_sak_1k_tnp3xxx(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x01);
    TEST_ASSERT_EQUAL_UINT16(16, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(64, l.total_blocks);
}

void test_sak_1k_ev1(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x28);
    TEST_ASSERT_EQUAL_UINT16(16, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(64, l.total_blocks);
}

void test_sak_2k(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x19);
    TEST_ASSERT_EQUAL_UINT16(32, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(128, l.total_blocks);
}

void test_sak_2k_plus(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x10);
    TEST_ASSERT_EQUAL_UINT16(32, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(128, l.total_blocks);
}

void test_sak_4k(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x18);
    TEST_ASSERT_EQUAL_UINT16(40, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(256, l.total_blocks);
}

void test_sak_4k_ev1(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x38);
    TEST_ASSERT_EQUAL_UINT16(40, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(256, l.total_blocks);
}

void test_sak_4k_plus(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0x11);
    TEST_ASSERT_EQUAL_UINT16(40, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(256, l.total_blocks);
}

void test_sak_unknown_fallback(void)
{
    mfc_layout_t l = mfc_layout_from_sak(0xFF);
    TEST_ASSERT_EQUAL_UINT16(16, l.total_sectors);
    TEST_ASSERT_EQUAL_UINT16(64, l.total_blocks);
}

/* =========================================================================
 * mfc_sector_first_block
 * =========================================================================*/

void test_first_block_sector0(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, mfc_sector_first_block(0));
}

void test_first_block_sector1(void)
{
    TEST_ASSERT_EQUAL_UINT16(4, mfc_sector_first_block(1));
}

void test_first_block_sector15(void)
{
    TEST_ASSERT_EQUAL_UINT16(60, mfc_sector_first_block(15));
}

void test_first_block_sector31(void)
{
    TEST_ASSERT_EQUAL_UINT16(124, mfc_sector_first_block(31));
}

void test_first_block_sector32(void)
{
    TEST_ASSERT_EQUAL_UINT16(128, mfc_sector_first_block(32));
}

void test_first_block_sector33(void)
{
    TEST_ASSERT_EQUAL_UINT16(144, mfc_sector_first_block(33));
}

void test_first_block_sector39(void)
{
    TEST_ASSERT_EQUAL_UINT16(240, mfc_sector_first_block(39));
}

/* =========================================================================
 * mfc_sector_block_count
 * =========================================================================*/

void test_block_count_small_sector(void)
{
    TEST_ASSERT_EQUAL_UINT16(4, mfc_sector_block_count(0, 16));
}

void test_block_count_sector31_on_4k(void)
{
    TEST_ASSERT_EQUAL_UINT16(4, mfc_sector_block_count(31, 40));
}

void test_block_count_sector32_on_4k(void)
{
    TEST_ASSERT_EQUAL_UINT16(16, mfc_sector_block_count(32, 40));
}

void test_block_count_sector39_on_4k(void)
{
    TEST_ASSERT_EQUAL_UINT16(16, mfc_sector_block_count(39, 40));
}

void test_block_count_sector15_on_1k(void)
{
    TEST_ASSERT_EQUAL_UINT16(4, mfc_sector_block_count(15, 16));
}

/* =========================================================================
 * mfc_uid4_from_nfcid
 * =========================================================================*/

void test_uid4_from_4byte(void)
{
    uint8_t nfcid[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t uid4[4] = {0};
    mfc_uid4_from_nfcid(nfcid, 4, uid4);
    TEST_ASSERT_EQUAL_HEX8(0xAA, uid4[0]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, uid4[3]);
}

void test_uid4_from_7byte(void)
{
    uint8_t nfcid[] = {0x01, 0x02, 0x03, 0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t uid4[4] = {0};
    mfc_uid4_from_nfcid(nfcid, 7, uid4);
    /* Should use bytes 3..6 */
    TEST_ASSERT_EQUAL_HEX8(0xAA, uid4[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, uid4[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, uid4[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, uid4[3]);
}

void test_uid4_from_10byte(void)
{
    uint8_t nfcid[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    uint8_t uid4[4] = {0};
    mfc_uid4_from_nfcid(nfcid, 10, uid4);
    /* >= 7 bytes → use bytes 3..6 */
    TEST_ASSERT_EQUAL_HEX8(0x04, uid4[0]);
}

void test_uid4_short_nfcid(void)
{
    uint8_t nfcid[] = {0xAA, 0xBB};
    uint8_t uid4[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    mfc_uid4_from_nfcid(nfcid, 2, uid4);
    TEST_ASSERT_EQUAL_HEX8(0xAA, uid4[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, uid4[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, uid4[2]);
}

void test_uid4_null_input(void)
{
    uint8_t uid4[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    mfc_uid4_from_nfcid(NULL, 4, uid4);
    TEST_ASSERT_EQUAL_HEX8(0x00, uid4[0]);
}

/* =========================================================================
 * main
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* layout from SAK */
    RUN_TEST(test_sak_mini);
    RUN_TEST(test_sak_1k);
    RUN_TEST(test_sak_1k_tnp3xxx);
    RUN_TEST(test_sak_1k_ev1);
    RUN_TEST(test_sak_2k);
    RUN_TEST(test_sak_2k_plus);
    RUN_TEST(test_sak_4k);
    RUN_TEST(test_sak_4k_ev1);
    RUN_TEST(test_sak_4k_plus);
    RUN_TEST(test_sak_unknown_fallback);

    /* sector first block */
    RUN_TEST(test_first_block_sector0);
    RUN_TEST(test_first_block_sector1);
    RUN_TEST(test_first_block_sector15);
    RUN_TEST(test_first_block_sector31);
    RUN_TEST(test_first_block_sector32);
    RUN_TEST(test_first_block_sector33);
    RUN_TEST(test_first_block_sector39);

    /* sector block count */
    RUN_TEST(test_block_count_small_sector);
    RUN_TEST(test_block_count_sector31_on_4k);
    RUN_TEST(test_block_count_sector32_on_4k);
    RUN_TEST(test_block_count_sector39_on_4k);
    RUN_TEST(test_block_count_sector15_on_1k);

    /* uid4 from nfcid */
    RUN_TEST(test_uid4_from_4byte);
    RUN_TEST(test_uid4_from_7byte);
    RUN_TEST(test_uid4_from_10byte);
    RUN_TEST(test_uid4_short_nfcid);
    RUN_TEST(test_uid4_null_input);

    return UNITY_END();
}
