/* See COPYING.txt for license details. */

/**
 * test_nfc_classify.c
 *
 * Unit tests for the pure-logic NFC-A classifier in nfc_classify.h/.c:
 *   nfc_classify_family()  — SAK + ATQA → family code
 *   nfc_classify_nfca()    — full NFC-A classification from primitives
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "nfc_classify.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * nfc_classify_family
 * =========================================================================*/

void test_family_classic_1k(void)
{
    uint8_t atqa[] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_CLASSIC, nfc_classify_family(0x08, atqa));
}

void test_family_classic_4k(void)
{
    uint8_t atqa[] = {0x02, 0x00};
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_CLASSIC, nfc_classify_family(0x18, atqa));
}

void test_family_classic_mini(void)
{
    uint8_t atqa[] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_CLASSIC, nfc_classify_family(0x09, atqa));
}

void test_family_ultralight(void)
{
    uint8_t atqa[] = {0x44, 0x00};
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_ULTRALIGHT, nfc_classify_family(0x00, atqa));
}

void test_family_desfire(void)
{
    uint8_t atqa[] = {0x44, 0x03};
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_DESFIRE, nfc_classify_family(0x20, atqa));
}

void test_family_unknown_defaults_classic(void)
{
    uint8_t atqa[] = {0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_CLASSIC, nfc_classify_family(0x42, atqa));
}

void test_family_sak00_non_ntag(void)
{
    /* SAK=0x00 but ATQA[0] != 0x44 → falls through to default (classic) */
    uint8_t atqa[] = {0x04, 0x00};
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_CLASSIC, nfc_classify_family(0x00, atqa));
}

void test_family_null_atqa(void)
{
    /* SAK=0x00 with NULL atqa → can't match ultralight, falls to default */
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_CLASSIC, nfc_classify_family(0x00, NULL));
}

/* =========================================================================
 * nfc_classify_nfca
 * =========================================================================*/

void test_classify_nfca_4byte_uid(void)
{
    uint8_t nfcid[] = {0x04, 0xA2, 0xBC, 0xDE};
    nfc_classify_result_t r;
    nfc_classify_nfca(nfcid, 4, 0x44, 0x00, 0x00, &r);

    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_TECH_A, r.tech);
    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_ULTRALIGHT, r.family);
    TEST_ASSERT_EQUAL_UINT8(4, r.uid_len);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.uid[0]);
    TEST_ASSERT_EQUAL_HEX8(0xDE, r.uid[3]);
    TEST_ASSERT_TRUE(r.has_atqa);
    TEST_ASSERT_TRUE(r.has_sak);
    TEST_ASSERT_EQUAL_HEX8(0x44, r.atqa[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.atqa[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.sak);
}

void test_classify_nfca_7byte_uid(void)
{
    uint8_t nfcid[] = {0x04, 0xA2, 0xBC, 0xDE, 0x12, 0x34, 0x56};
    nfc_classify_result_t r;
    nfc_classify_nfca(nfcid, 7, 0x44, 0x00, 0x00, &r);

    TEST_ASSERT_EQUAL_UINT8(7, r.uid_len);
    TEST_ASSERT_EQUAL_HEX8(0x56, r.uid[6]);
}

void test_classify_nfca_classic_1k(void)
{
    uint8_t nfcid[] = {0xAA, 0xBB, 0xCC, 0xDD};
    nfc_classify_result_t r;
    nfc_classify_nfca(nfcid, 4, 0x04, 0x00, 0x08, &r);

    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_CLASSIC, r.family);
    TEST_ASSERT_EQUAL_HEX8(0x08, r.sak);
}

void test_classify_nfca_desfire(void)
{
    uint8_t nfcid[] = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    nfc_classify_result_t r;
    nfc_classify_nfca(nfcid, 7, 0x44, 0x03, 0x20, &r);

    TEST_ASSERT_EQUAL_UINT8(NFC_CLASSIFY_FAM_DESFIRE, r.family);
}

void test_classify_nfca_null_nfcid(void)
{
    nfc_classify_result_t r;
    nfc_classify_nfca(NULL, 4, 0x44, 0x00, 0x00, &r);

    TEST_ASSERT_EQUAL_UINT8(0, r.uid_len);  /* clamped to 0 because nfcid is NULL */
    TEST_ASSERT_TRUE(r.has_atqa);
}

void test_classify_nfca_null_result(void)
{
    uint8_t nfcid[] = {0x04};
    /* Should not crash */
    nfc_classify_nfca(nfcid, 1, 0x44, 0x00, 0x00, NULL);
}

void test_classify_nfca_uid_clamp(void)
{
    /* UID longer than 10 → clamped */
    uint8_t nfcid[12] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C};
    nfc_classify_result_t r;
    nfc_classify_nfca(nfcid, 12, 0x44, 0x00, 0x00, &r);

    TEST_ASSERT_EQUAL_UINT8(10, r.uid_len);
    TEST_ASSERT_EQUAL_HEX8(0x0A, r.uid[9]);
}

/* =========================================================================
 * main
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* family classification */
    RUN_TEST(test_family_classic_1k);
    RUN_TEST(test_family_classic_4k);
    RUN_TEST(test_family_classic_mini);
    RUN_TEST(test_family_ultralight);
    RUN_TEST(test_family_desfire);
    RUN_TEST(test_family_unknown_defaults_classic);
    RUN_TEST(test_family_sak00_non_ntag);
    RUN_TEST(test_family_null_atqa);

    /* full nfca classification */
    RUN_TEST(test_classify_nfca_4byte_uid);
    RUN_TEST(test_classify_nfca_7byte_uid);
    RUN_TEST(test_classify_nfca_classic_1k);
    RUN_TEST(test_classify_nfca_desfire);
    RUN_TEST(test_classify_nfca_null_nfcid);
    RUN_TEST(test_classify_nfca_null_result);
    RUN_TEST(test_classify_nfca_uid_clamp);

    return UNITY_END();
}
