/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_alutech_at_4n.c
 * @brief  Host tests for the Alutech AT-4N TEA-variant cipher (P4).
 *
 * Since the production rainbow table is proprietary, these tests use a
 * synthetic 32-byte table to validate algorithm correctness
 * (encrypt/decrypt roundtrip, CRC functions, determinism).
 */

#include "unity.h"
#include "subghz_alutech_at_4n.h"

#include <string.h>

/*============================================================================*/
/* Synthetic test table                                                        */
/*============================================================================*/

/**
 * A deterministic synthetic 32-byte table for testing.  This is NOT a
 * production rainbow table — it contains 8 arbitrary uint32_t values
 * stored big-endian so the TEA rounds produce meaningful output.
 *
 * Layout (8 × uint32_t, big-endian):
 *   [0] = 0xC6EF3720  (initial sum for decrypt — standard TEA delta*32)
 *   [1] = 0x01234567  (K0)
 *   [2] = 0x89ABCDEF  (K1)
 *   [3] = 0x61C88647  (negative delta = -0x9E3779B9 as unsigned)
 *   [4] = 0xFEDCBA98  (K2)
 *   [5] = 0x76543210  (K3)
 *   [6] = 0x9E3779B9  (encrypt step delta = standard TEA delta)
 *   [7] = 0xC6EF3720  (encrypt terminal sum = same as [0])
 */
static const uint8_t TEST_TABLE[ALUTECH_AT_4N_TABLE_SIZE] = {
    /* [0] 0xC6EF3720 */ 0xC6, 0xEF, 0x37, 0x20,
    /* [1] 0x01234567 */ 0x01, 0x23, 0x45, 0x67,
    /* [2] 0x89ABCDEF */ 0x89, 0xAB, 0xCD, 0xEF,
    /* [3] 0x61C88647 */ 0x61, 0xC8, 0x86, 0x47,
    /* [4] 0xFEDCBA98 */ 0xFE, 0xDC, 0xBA, 0x98,
    /* [5] 0x76543210 */ 0x76, 0x54, 0x32, 0x10,
    /* [6] 0x9E3779B9 */ 0x9E, 0x37, 0x79, 0xB9,
    /* [7] 0xC6EF3720 */ 0xC6, 0xEF, 0x37, 0x20,
};

/*============================================================================*/
/* Test setup / teardown                                                       */
/*============================================================================*/

void setUp(void)  {}
void tearDown(void) {}

/*============================================================================*/
/* Encrypt / decrypt roundtrip                                                 */
/*============================================================================*/

/** Encrypt → decrypt roundtrip must recover the original plaintext. */
static void test_encrypt_decrypt_roundtrip(void)
{
    const uint64_t plaintext = 0x0123456789ABCDEFULL;
    const uint64_t enc = alutech_at_4n_encrypt(plaintext, TEST_TABLE);
    const uint64_t dec = alutech_at_4n_decrypt(enc, TEST_TABLE);

    TEST_ASSERT_EQUAL_HEX64(plaintext, dec);
}

/** Roundtrip with zero plaintext. */
static void test_roundtrip_zero(void)
{
    const uint64_t plaintext = 0x0ULL;
    const uint64_t enc = alutech_at_4n_encrypt(plaintext, TEST_TABLE);
    const uint64_t dec = alutech_at_4n_decrypt(enc, TEST_TABLE);

    TEST_ASSERT_EQUAL_HEX64(plaintext, dec);
}

/** Roundtrip with all-ones plaintext. */
static void test_roundtrip_all_ones(void)
{
    const uint64_t plaintext = 0xFFFFFFFFFFFFFFFFULL;
    const uint64_t enc = alutech_at_4n_encrypt(plaintext, TEST_TABLE);
    const uint64_t dec = alutech_at_4n_decrypt(enc, TEST_TABLE);

    TEST_ASSERT_EQUAL_HEX64(plaintext, dec);
}

/** Roundtrip over a range of counter values with fixed serial. */
static void test_roundtrip_counter_sweep(void)
{
    /* Construct a plaintext with serial=0xDEADBEEF, button=0xFF,
     * and sweep the counter. */
    for (uint16_t cnt = 0; cnt < 256; ++cnt)
    {
        /* Build plaintext: crc | serial[3:0] | cnt_hi | cnt_lo | btn */
        uint8_t cnt_lo = (uint8_t)(cnt & 0xFF);
        uint8_t crc_check = alutech_at_4n_decrypt_data_crc(cnt_lo);
        uint64_t plain = 0;
        uint8_t *p = (uint8_t *)&plain;
        p[0] = crc_check;
        p[1] = 0xDE; p[2] = 0xAD; p[3] = 0xBE; p[4] = 0xEF;
        p[5] = (uint8_t)(cnt >> 8);
        p[6] = cnt_lo;
        p[7] = 0xFF;

        const uint64_t enc = alutech_at_4n_encrypt(plain, TEST_TABLE);
        const uint64_t dec = alutech_at_4n_decrypt(enc, TEST_TABLE);

        TEST_ASSERT_EQUAL_HEX64(plain, dec);
    }
}

/*============================================================================*/
/* Encryption is non-trivial                                                   */
/*============================================================================*/

/** Encrypted output must differ from plaintext. */
static void test_encrypt_changes_data(void)
{
    const uint64_t plaintext = 0x0123456789ABCDEFULL;
    const uint64_t enc = alutech_at_4n_encrypt(plaintext, TEST_TABLE);

    TEST_ASSERT_NOT_EQUAL(plaintext, enc);
}

/*============================================================================*/
/* Determinism                                                                 */
/*============================================================================*/

/** Same input + same table → same output. */
static void test_encrypt_deterministic(void)
{
    const uint64_t plaintext = 0xDEADBEEF12345678ULL;
    const uint64_t enc1 = alutech_at_4n_encrypt(plaintext, TEST_TABLE);
    const uint64_t enc2 = alutech_at_4n_encrypt(plaintext, TEST_TABLE);

    TEST_ASSERT_EQUAL_HEX64(enc1, enc2);
}

/*============================================================================*/
/* Different plaintexts produce different ciphertexts                           */
/*============================================================================*/

static void test_different_plaintexts_different_ciphertexts(void)
{
    const uint64_t enc1 = alutech_at_4n_encrypt(0x0123456789ABCDEFULL, TEST_TABLE);
    const uint64_t enc2 = alutech_at_4n_encrypt(0x0123456789ABCDF0ULL, TEST_TABLE);

    TEST_ASSERT_NOT_EQUAL(enc1, enc2);
}

/*============================================================================*/
/* CRC tests                                                                   */
/*============================================================================*/

/** Frame CRC should be deterministic and non-trivial. */
static void test_crc_deterministic(void)
{
    const uint64_t data = 0x0123456789ABCDEFULL;
    const uint8_t crc1 = alutech_at_4n_crc(data);
    const uint8_t crc2 = alutech_at_4n_crc(data);
    TEST_ASSERT_EQUAL_HEX8(crc1, crc2);
}

/** Different data should produce different CRCs (for these test values). */
static void test_crc_different_data(void)
{
    const uint8_t crc1 = alutech_at_4n_crc(0x0123456789ABCDEFULL);
    const uint8_t crc2 = alutech_at_4n_crc(0xFEDCBA9876543210ULL);
    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

/** Decrypt data CRC should be consistent and invertible.
 *  crc(x) should differ from crc(y) when x != y. */
static void test_decrypt_data_crc_varies(void)
{
    const uint8_t c1 = alutech_at_4n_decrypt_data_crc(0x00);
    const uint8_t c2 = alutech_at_4n_decrypt_data_crc(0x01);
    TEST_ASSERT_NOT_EQUAL(c1, c2);
}

/*============================================================================*/
/* Main runner                                                                 */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_encrypt_decrypt_roundtrip);
    RUN_TEST(test_roundtrip_zero);
    RUN_TEST(test_roundtrip_all_ones);
    RUN_TEST(test_roundtrip_counter_sweep);
    RUN_TEST(test_encrypt_changes_data);
    RUN_TEST(test_encrypt_deterministic);
    RUN_TEST(test_different_plaintexts_different_ciphertexts);
    RUN_TEST(test_crc_deterministic);
    RUN_TEST(test_crc_different_data);
    RUN_TEST(test_decrypt_data_crc_varies);

    return UNITY_END();
}
