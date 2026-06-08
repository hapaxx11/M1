/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_nice_flor_s.c
 * @brief  Host tests for the Nice FloR-S cipher (P3).
 *
 * The Nice FloR-S cipher uses a 32-byte rainbow/permutation table.
 * Since the production table is proprietary, these tests use a
 * synthetic table to validate algorithm correctness (encrypt/decrypt
 * roundtrip, determinism, zero-input behaviour).
 */

#include "unity.h"
#include "subghz_nice_flor_s.h"

#include <string.h>

/*============================================================================*/
/* Synthetic test table                                                        */
/*============================================================================*/

/**
 * A deterministic synthetic 32-byte table for testing.  Not the
 * production rainbow table — used solely to verify algorithm correctness.
 */
static const uint8_t TEST_TABLE[NICE_FLOR_S_TABLE_SIZE] = {
    0xE2, 0x4B, 0x75, 0x3A, 0x96, 0xD1, 0x0F, 0x58,
    0xC7, 0x6D, 0xA3, 0x1E, 0x84, 0xF0, 0x2C, 0x69,
    0xB5, 0x47, 0xDE, 0x03, 0x91, 0x5C, 0xFA, 0x28,
    0x7B, 0x34, 0xE6, 0x0A, 0xCD, 0x62, 0x8F, 0x11,
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
    /* serial=0x0436C682, counter=0x0444 → data = 0x0436C6820444 */
    const uint64_t plaintext = 0x0436C6820444ULL;
    const uint64_t enc = nice_flor_s_encrypt(plaintext, TEST_TABLE);
    const uint64_t dec = nice_flor_s_decrypt(enc, TEST_TABLE);

    TEST_ASSERT_EQUAL_HEX64(plaintext, dec);
}

/** Roundtrip with zero plaintext. */
static void test_roundtrip_zero(void)
{
    const uint64_t plaintext = 0x0ULL;
    const uint64_t enc = nice_flor_s_encrypt(plaintext, TEST_TABLE);
    const uint64_t dec = nice_flor_s_decrypt(enc, TEST_TABLE);

    TEST_ASSERT_EQUAL_HEX64(plaintext, dec);
}

/** Roundtrip with maximum 44-bit payload. */
static void test_roundtrip_max_44bit(void)
{
    const uint64_t plaintext = 0x0FFFFFFFFFFFULL;
    const uint64_t enc = nice_flor_s_encrypt(plaintext, TEST_TABLE);
    const uint64_t dec = nice_flor_s_decrypt(enc, TEST_TABLE);

    /* Only lower 44 bits matter — p[5] is masked to 4 bits. */
    TEST_ASSERT_EQUAL_HEX64(plaintext & 0x0FFFFFFFFFFFULL,
                            dec       & 0x0FFFFFFFFFFFULL);
}

/** Roundtrip over a range of counter values with a fixed serial. */
static void test_roundtrip_counter_sweep(void)
{
    const uint32_t serial = 0x01234567U & 0x0FFFFFFFU;
    for (uint16_t cnt = 0; cnt < 256; ++cnt)
    {
        const uint64_t plain = ((uint64_t)serial << 16) | cnt;
        const uint64_t enc = nice_flor_s_encrypt(plain, TEST_TABLE);
        const uint64_t dec = nice_flor_s_decrypt(enc, TEST_TABLE);

        TEST_ASSERT_EQUAL_HEX64(plain, dec);
    }
}

/*============================================================================*/
/* Encryption is non-trivial                                                   */
/*============================================================================*/

/** Encrypted output must differ from plaintext (for non-trivial inputs). */
static void test_encrypt_changes_data(void)
{
    const uint64_t plaintext = 0x0436C6820444ULL;
    const uint64_t enc = nice_flor_s_encrypt(plaintext, TEST_TABLE);

    TEST_ASSERT_NOT_EQUAL(plaintext, enc);
}

/*============================================================================*/
/* Determinism                                                                 */
/*============================================================================*/

/** Same input + same table → same output. */
static void test_encrypt_deterministic(void)
{
    const uint64_t plaintext = 0x0ABCDEF01234ULL;
    const uint64_t enc1 = nice_flor_s_encrypt(plaintext, TEST_TABLE);
    const uint64_t enc2 = nice_flor_s_encrypt(plaintext, TEST_TABLE);

    TEST_ASSERT_EQUAL_HEX64(enc1, enc2);
}

/*============================================================================*/
/* Different tables produce different output                                   */
/*============================================================================*/

/** Changing the table key must change the ciphertext. */
static void test_different_table_different_output(void)
{
    uint8_t alt_table[NICE_FLOR_S_TABLE_SIZE];
    /* Use a substantially different table to avoid coincidental match. */
    for (int i = 0; i < NICE_FLOR_S_TABLE_SIZE; ++i)
        alt_table[i] = TEST_TABLE[i] ^ 0xFFU;

    const uint64_t plaintext = 0x0436C6820444ULL;
    const uint64_t enc1 = nice_flor_s_encrypt(plaintext, TEST_TABLE);
    const uint64_t enc2 = nice_flor_s_encrypt(plaintext, alt_table);

    TEST_ASSERT_NOT_EQUAL(enc1, enc2);
}

/*============================================================================*/
/* Upper bits are zeroed                                                       */
/*============================================================================*/

/** Bits above 47 in the output must be zero (p[5] high nibble masked). */
static void test_encrypt_output_upper_bits_zero(void)
{
    const uint64_t plaintext = 0x0436C6820444ULL;
    const uint64_t enc = nice_flor_s_encrypt(plaintext, TEST_TABLE);

    /* Bytes 6 and 7 (bits [63:48]) must be zero.
     * Byte 5 upper nibble (bits [47:44]) must be zero. */
    TEST_ASSERT_EQUAL_HEX64(0ULL, enc & 0xFFF0000000000000ULL);
}

/*============================================================================*/
/* Decrypt of wrong table doesn't roundtrip                                    */
/*============================================================================*/

/** Encrypting with one table and decrypting with another must not recover
 *  the original plaintext (except by coincidence — extremely unlikely). */
static void test_decrypt_wrong_table(void)
{
    uint8_t alt_table[NICE_FLOR_S_TABLE_SIZE];
    /* Use a substantially different table. */
    for (int i = 0; i < NICE_FLOR_S_TABLE_SIZE; ++i)
        alt_table[i] = TEST_TABLE[i] ^ 0xA5U;

    const uint64_t plaintext = 0x0436C6820444ULL;
    const uint64_t enc = nice_flor_s_encrypt(plaintext, TEST_TABLE);
    const uint64_t dec = nice_flor_s_decrypt(enc, alt_table);

    TEST_ASSERT_NOT_EQUAL(plaintext, dec);
}

/*============================================================================*/
/* Different plaintexts produce different ciphertexts                           */
/*============================================================================*/

static void test_different_plaintexts_different_ciphertexts(void)
{
    const uint64_t enc1 = nice_flor_s_encrypt(0x0436C6820444ULL, TEST_TABLE);
    const uint64_t enc2 = nice_flor_s_encrypt(0x0436C6820445ULL, TEST_TABLE);

    TEST_ASSERT_NOT_EQUAL(enc1, enc2);
}

/*============================================================================*/
/* Main runner                                                                 */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_encrypt_decrypt_roundtrip);
    RUN_TEST(test_roundtrip_zero);
    RUN_TEST(test_roundtrip_max_44bit);
    RUN_TEST(test_roundtrip_counter_sweep);
    RUN_TEST(test_encrypt_changes_data);
    RUN_TEST(test_encrypt_deterministic);
    RUN_TEST(test_different_table_different_output);
    RUN_TEST(test_encrypt_output_upper_bits_zero);
    RUN_TEST(test_decrypt_wrong_table);
    RUN_TEST(test_different_plaintexts_different_ciphertexts);

    return UNITY_END();
}
