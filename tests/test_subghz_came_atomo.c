/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_came_atomo.c
 * @brief  Host tests for the CAME Atomo LFSR cipher (P4).
 *
 * The CAME Atomo cipher is self-contained (no external key material).
 * Tests validate encrypt/decrypt roundtrip, determinism, seed
 * sensitivity, and the LFSR keystream properties.
 */

#include "unity.h"
#include "subghz_came_atomo.h"

#include <string.h>
#include <stdbool.h>

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
    uint8_t plain[8] = { 0x10, 0x00, 0x42, 0x12, 0x34, 0x56, 0x78, 0x20 };
    uint8_t orig[8];
    memcpy(orig, plain, 8);

    came_atomo_encrypt(plain);
    came_atomo_decrypt(plain);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(orig, plain, 8);
}

/** Roundtrip with zero plaintext. */
static void test_roundtrip_zero(void)
{
    uint8_t plain[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t orig[8];
    memcpy(orig, plain, 8);

    came_atomo_encrypt(plain);
    came_atomo_decrypt(plain);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(orig, plain, 8);
}

/** Roundtrip with maximum value bytes. */
static void test_roundtrip_max(void)
{
    /* buff[0] is masked to 7 bits, so max = 0x7F for roundtrip fidelity.
     * Other bytes can be 0xFF. */
    uint8_t plain[8] = { 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t orig[8];
    memcpy(orig, plain, 8);

    came_atomo_encrypt(plain);
    came_atomo_decrypt(plain);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(orig, plain, 8);
}

/** Roundtrip over a range of buff[0] seed values. */
static void test_roundtrip_seed_sweep(void)
{
    for (unsigned seed = 0; seed < 128; ++seed)
    {
        uint8_t plain[8] = { (uint8_t)seed, 0xAB, 0xCD, 0x12, 0x34, 0x56, 0x78, 0x90 };
        uint8_t orig[8];
        memcpy(orig, plain, 8);

        came_atomo_encrypt(plain);
        came_atomo_decrypt(plain);

        TEST_ASSERT_EQUAL_HEX8_ARRAY(orig, plain, 8);
    }
}

/*============================================================================*/
/* Encryption is non-trivial                                                   */
/*============================================================================*/

/** Encrypted output must differ from plaintext (for non-trivial inputs). */
static void test_encrypt_changes_data(void)
{
    uint8_t plain[8] = { 0x10, 0x00, 0x42, 0x12, 0x34, 0x56, 0x78, 0x20 };
    uint8_t orig[8];
    memcpy(orig, plain, 8);

    came_atomo_encrypt(plain);

    /* At least one byte must differ. */
    bool differs = false;
    for (int i = 0; i < 8; ++i)
    {
        if (plain[i] != orig[i])
        {
            differs = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(differs);
}

/*============================================================================*/
/* Determinism                                                                 */
/*============================================================================*/

/** Same input → same output. */
static void test_encrypt_deterministic(void)
{
    uint8_t a[8] = { 0x10, 0x00, 0x42, 0x12, 0x34, 0x56, 0x78, 0x20 };
    uint8_t b[8];
    memcpy(b, a, 8);

    came_atomo_encrypt(a);
    came_atomo_encrypt(b);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(a, b, 8);
}

/*============================================================================*/
/* Different plaintexts produce different ciphertexts                           */
/*============================================================================*/

static void test_different_plaintexts_different_ciphertexts(void)
{
    uint8_t a[8] = { 0x10, 0x00, 0x42, 0x12, 0x34, 0x56, 0x78, 0x20 };
    uint8_t b[8] = { 0x10, 0x00, 0x42, 0x12, 0x34, 0x56, 0x78, 0x30 };

    came_atomo_encrypt(a);
    came_atomo_encrypt(b);

    bool differs = false;
    for (int i = 0; i < 8; ++i)
    {
        if (a[i] != b[i])
        {
            differs = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(differs);
}

/*============================================================================*/
/* buff[0] is masked to 7 bits after encrypt                                   */
/*============================================================================*/

static void test_encrypt_buff0_masked(void)
{
    uint8_t plain[8] = { 0x3A, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22 };
    came_atomo_encrypt(plain);

    /* After encrypt, buff[0] = (buff[0] ^ 0x05) & 0x7F — bit 7 is always 0. */
    TEST_ASSERT_EQUAL_HEX8(0, plain[0] & 0x80U);
}

/*============================================================================*/
/* Different seeds produce different ciphertexts                               */
/*============================================================================*/

static void test_different_seeds_different_output(void)
{
    uint8_t a[8] = { 0x10, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
    uint8_t b[8] = { 0x20, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };

    came_atomo_encrypt(a);
    came_atomo_encrypt(b);

    /* The body bytes (1-7) should differ due to different LFSR seeds. */
    bool body_differs = false;
    for (int i = 1; i < 8; ++i)
    {
        if (a[i] != b[i])
        {
            body_differs = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(body_differs);
}

/*============================================================================*/
/* Roundtrip with realistic protocol data                                      */
/*============================================================================*/

/** Test with a realistic CAME Atomo plaintext block:
 *  cnt_2=0x10, cnt=0x0042, serial=0x12345678, btn=0x2 */
static void test_realistic_data_roundtrip(void)
{
    /* Construct plaintext: cnt_2=0x10, cnt=0x0042, serial=0x12345678,
     * btn=0x2 → last byte = 0x20 */
    uint8_t plain[8] = { 0x10, 0x00, 0x42, 0x12, 0x34, 0x56, 0x78, 0x20 };
    uint8_t orig[8];
    memcpy(orig, plain, 8);

    /* Encrypt and verify it changed. */
    came_atomo_encrypt(plain);
    bool changed = (memcmp(orig, plain, 8) != 0);
    TEST_ASSERT_TRUE(changed);

    /* Decrypt and verify roundtrip. */
    came_atomo_decrypt(plain);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(orig, plain, 8);
}

/*============================================================================*/
/* Main runner                                                                 */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_encrypt_decrypt_roundtrip);
    RUN_TEST(test_roundtrip_zero);
    RUN_TEST(test_roundtrip_max);
    RUN_TEST(test_roundtrip_seed_sweep);
    RUN_TEST(test_encrypt_changes_data);
    RUN_TEST(test_encrypt_deterministic);
    RUN_TEST(test_different_plaintexts_different_ciphertexts);
    RUN_TEST(test_encrypt_buff0_masked);
    RUN_TEST(test_different_seeds_different_output);
    RUN_TEST(test_realistic_data_roundtrip);

    return UNITY_END();
}
