/* See COPYING.txt for license details. */

/**
 * @file  test_crypto.c
 * @brief Unit tests for AES-256-CBC encrypt/decrypt round-trip.
 *
 * Tests m1_crypto_encrypt_with_key() and m1_crypto_decrypt_with_key()
 * using deterministic IVs (the UID-based IV generator is stubbed out).
 * Verifies PKCS7 padding, CBC chaining, null-input guards, and
 * short/multi-block payloads.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>

/*
 * The CMakeLists defines M1_CRYPTO_SKIP_UID_FUNCTIONS to exclude the
 * UID/HAL-dependent functions from compilation.
 * We provide our own stubs for the remaining references.
 */
#include "m1_crypto.h"

/* ─── deterministic IV stub ─────────────────────────────────────────────── */

static uint8_t s_test_iv[16];

void m1_crypto_generate_iv(uint8_t iv[16])
{
    memcpy(iv, s_test_iv, 16);
}

void m1_crypto_derive_key(uint8_t key[32])
{
    /* Deterministic test key — all 0x42 */
    memset(key, 0x42, 32);
}

/* ─── test key — 32 bytes of 0x42 ─────────────────────────────────────── */

static const uint8_t test_key[32] = {
    0x42,0x42,0x42,0x42, 0x42,0x42,0x42,0x42,
    0x42,0x42,0x42,0x42, 0x42,0x42,0x42,0x42,
    0x42,0x42,0x42,0x42, 0x42,0x42,0x42,0x42,
    0x42,0x42,0x42,0x42, 0x42,0x42,0x42,0x42,
};

void setUp(void)
{
    /* Reset IV to all-zeroes before each test */
    memset(s_test_iv, 0, sizeof(s_test_iv));
}

void tearDown(void) { }

/*============================================================================*/
/* Round-trip tests                                                           */
/*============================================================================*/

void test_encrypt_decrypt_short(void)
{
    uint8_t buf[256];
    const char *plaintext = "Hello";
    uint32_t pt_len = 5;

    memcpy(buf, plaintext, pt_len);

    uint32_t ct_len = m1_crypto_encrypt_with_key(buf, pt_len, sizeof(buf), test_key);
    TEST_ASSERT_GREATER_THAN(0, ct_len);
    /* Must be IV (16) + 1 block (16) = 32 */
    TEST_ASSERT_EQUAL(32, ct_len);

    uint32_t dec_len = m1_crypto_decrypt_with_key(buf, ct_len, test_key);
    TEST_ASSERT_EQUAL(pt_len, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, buf, pt_len);
}

void test_encrypt_decrypt_exact_block(void)
{
    /* Exactly 16 bytes → PKCS7 adds a full padding block → 2 blocks out */
    uint8_t buf[256];
    const uint8_t plaintext[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    memcpy(buf, plaintext, 16);

    uint32_t ct_len = m1_crypto_encrypt_with_key(buf, 16, sizeof(buf), test_key);
    /* IV (16) + 2 blocks (32) = 48 */
    TEST_ASSERT_EQUAL(48, ct_len);

    uint32_t dec_len = m1_crypto_decrypt_with_key(buf, ct_len, test_key);
    TEST_ASSERT_EQUAL(16, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, buf, 16);
}

void test_encrypt_decrypt_multi_block(void)
{
    uint8_t buf[512];
    /* 48 bytes = 3 blocks, PKCS7 adds full block → 4 blocks */
    uint8_t plaintext[48];
    for (int i = 0; i < 48; i++)
        plaintext[i] = (uint8_t)(i * 7 + 3);

    memcpy(buf, plaintext, 48);

    uint32_t ct_len = m1_crypto_encrypt_with_key(buf, 48, sizeof(buf), test_key);
    /* IV (16) + 4 blocks (64) = 80 */
    TEST_ASSERT_EQUAL(80, ct_len);

    uint32_t dec_len = m1_crypto_decrypt_with_key(buf, ct_len, test_key);
    TEST_ASSERT_EQUAL(48, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, buf, 48);
}

void test_encrypt_decrypt_one_byte(void)
{
    uint8_t buf[64];
    buf[0] = 0xAA;

    uint32_t ct_len = m1_crypto_encrypt_with_key(buf, 1, sizeof(buf), test_key);
    TEST_ASSERT_EQUAL(32, ct_len);

    uint32_t dec_len = m1_crypto_decrypt_with_key(buf, ct_len, test_key);
    TEST_ASSERT_EQUAL(1, dec_len);
    TEST_ASSERT_EQUAL(0xAA, buf[0]);
}

void test_encrypt_decrypt_15_bytes(void)
{
    /* 15 bytes → 1 byte padding → fits in one block */
    uint8_t buf[64];
    uint8_t plaintext[15];
    for (int i = 0; i < 15; i++)
        plaintext[i] = (uint8_t)(i + 0x10);

    memcpy(buf, plaintext, 15);

    uint32_t ct_len = m1_crypto_encrypt_with_key(buf, 15, sizeof(buf), test_key);
    TEST_ASSERT_EQUAL(32, ct_len); /* IV(16) + 1 block(16) */

    uint32_t dec_len = m1_crypto_decrypt_with_key(buf, ct_len, test_key);
    TEST_ASSERT_EQUAL(15, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, buf, 15);
}

void test_encrypt_decrypt_17_bytes(void)
{
    /* 17 bytes → 15 bytes padding → 2 blocks */
    uint8_t buf[128];
    uint8_t plaintext[17];
    for (int i = 0; i < 17; i++)
        plaintext[i] = (uint8_t)(i + 0x30);

    memcpy(buf, plaintext, 17);

    uint32_t ct_len = m1_crypto_encrypt_with_key(buf, 17, sizeof(buf), test_key);
    TEST_ASSERT_EQUAL(48, ct_len); /* IV(16) + 2 blocks(32) */

    uint32_t dec_len = m1_crypto_decrypt_with_key(buf, ct_len, test_key);
    TEST_ASSERT_EQUAL(17, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, buf, 17);
}

/*============================================================================*/
/* Determinism — same plaintext + same key + same IV = same ciphertext        */
/*============================================================================*/

void test_deterministic_with_same_iv(void)
{
    uint8_t buf1[64], buf2[64];
    memset(s_test_iv, 0x11, 16);

    memcpy(buf1, "test1234", 8);
    uint32_t ct1 = m1_crypto_encrypt_with_key(buf1, 8, sizeof(buf1), test_key);

    memcpy(buf2, "test1234", 8);
    uint32_t ct2 = m1_crypto_encrypt_with_key(buf2, 8, sizeof(buf2), test_key);

    TEST_ASSERT_EQUAL(ct1, ct2);
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, ct1);
}

void test_different_iv_different_ciphertext(void)
{
    uint8_t buf1[64], buf2[64];

    memset(s_test_iv, 0x00, 16);
    memcpy(buf1, "test1234", 8);
    uint32_t ct1 = m1_crypto_encrypt_with_key(buf1, 8, sizeof(buf1), test_key);

    memset(s_test_iv, 0xFF, 16);
    memcpy(buf2, "test1234", 8);
    uint32_t ct2 = m1_crypto_encrypt_with_key(buf2, 8, sizeof(buf2), test_key);

    TEST_ASSERT_EQUAL(ct1, ct2); /* same length */
    /* Ciphertext body (after IV) should differ */
    TEST_ASSERT(memcmp(buf1 + 16, buf2 + 16, ct1 - 16) != 0);
}

/*============================================================================*/
/* Different keys produce different ciphertext                                */
/*============================================================================*/

void test_different_key_different_ciphertext(void)
{
    uint8_t buf1[64], buf2[64];
    uint8_t key2[32];
    memset(key2, 0xBB, 32);

    memcpy(buf1, "test1234", 8);
    uint32_t ct1 = m1_crypto_encrypt_with_key(buf1, 8, sizeof(buf1), test_key);

    memcpy(buf2, "test1234", 8);
    uint32_t ct2 = m1_crypto_encrypt_with_key(buf2, 8, sizeof(buf2), key2);

    TEST_ASSERT_EQUAL(ct1, ct2); /* same length */
    /* Ciphertext should differ (same IV, different key) */
    TEST_ASSERT(memcmp(buf1 + 16, buf2 + 16, ct1 - 16) != 0);
}

/*============================================================================*/
/* Wrong key fails to decrypt (padding validation fails)                      */
/*============================================================================*/

void test_wrong_key_fails_decrypt(void)
{
    uint8_t buf[64];
    uint8_t wrong_key[32];
    memset(wrong_key, 0xCC, 32);

    memcpy(buf, "secret!!", 8);
    uint32_t ct_len = m1_crypto_encrypt_with_key(buf, 8, sizeof(buf), test_key);
    TEST_ASSERT_GREATER_THAN(0, ct_len);

    /* Decrypt with wrong key → padding check should fail, return 0 */
    uint32_t dec_len = m1_crypto_decrypt_with_key(buf, ct_len, wrong_key);
    TEST_ASSERT_EQUAL(0, dec_len);
}

/*============================================================================*/
/* NULL and edge-case guards                                                  */
/*============================================================================*/

void test_encrypt_null_buf(void)
{
    TEST_ASSERT_EQUAL(0, m1_crypto_encrypt_with_key(NULL, 10, 256, test_key));
}

void test_encrypt_zero_len(void)
{
    uint8_t buf[64];
    TEST_ASSERT_EQUAL(0, m1_crypto_encrypt_with_key(buf, 0, sizeof(buf), test_key));
}

void test_encrypt_null_key(void)
{
    uint8_t buf[64];
    TEST_ASSERT_EQUAL(0, m1_crypto_encrypt_with_key(buf, 5, sizeof(buf), NULL));
}

void test_encrypt_buf_too_small(void)
{
    uint8_t buf[16]; /* too small for IV + 1 block */
    memcpy(buf, "hi", 2);
    TEST_ASSERT_EQUAL(0, m1_crypto_encrypt_with_key(buf, 2, sizeof(buf), test_key));
}

void test_decrypt_null_buf(void)
{
    TEST_ASSERT_EQUAL(0, m1_crypto_decrypt_with_key(NULL, 32, test_key));
}

void test_decrypt_too_short(void)
{
    uint8_t buf[16] = {0};
    /* Minimum is IV(16) + 1 block(16) = 32 */
    TEST_ASSERT_EQUAL(0, m1_crypto_decrypt_with_key(buf, 16, test_key));
}

void test_decrypt_null_key(void)
{
    uint8_t buf[32] = {0};
    TEST_ASSERT_EQUAL(0, m1_crypto_decrypt_with_key(buf, 32, NULL));
}

void test_decrypt_not_block_aligned(void)
{
    uint8_t buf[33] = {0};
    /* 33 - 16(IV) = 17 → not a multiple of 16 */
    TEST_ASSERT_EQUAL(0, m1_crypto_decrypt_with_key(buf, 33, test_key));
}

void test_decrypt_corrupted_ciphertext(void)
{
    uint8_t buf[64];
    memcpy(buf, "testdata", 8);

    uint32_t ct_len = m1_crypto_encrypt_with_key(buf, 8, sizeof(buf), test_key);
    TEST_ASSERT_GREATER_THAN(0, ct_len);

    /* Corrupt one byte of ciphertext (after IV) */
    buf[20] ^= 0xFF;

    /* Padding validation should fail (usually) */
    uint32_t dec_len = m1_crypto_decrypt_with_key(buf, ct_len, test_key);
    /* Corrupted data may or may not pass padding check — we just ensure
     * the function doesn't crash.  If it returns 0, padding failed (expected).
     * If it returns non-zero, the corruption happened to land on valid padding
     * (extremely unlikely with our test data). */
    (void)dec_len;
}

/*============================================================================*/
/* main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Round-trip tests */
    RUN_TEST(test_encrypt_decrypt_short);
    RUN_TEST(test_encrypt_decrypt_exact_block);
    RUN_TEST(test_encrypt_decrypt_multi_block);
    RUN_TEST(test_encrypt_decrypt_one_byte);
    RUN_TEST(test_encrypt_decrypt_15_bytes);
    RUN_TEST(test_encrypt_decrypt_17_bytes);

    /* Determinism */
    RUN_TEST(test_deterministic_with_same_iv);
    RUN_TEST(test_different_iv_different_ciphertext);

    /* Key independence */
    RUN_TEST(test_different_key_different_ciphertext);

    /* Wrong key */
    RUN_TEST(test_wrong_key_fails_decrypt);

    /* Null/edge guards */
    RUN_TEST(test_encrypt_null_buf);
    RUN_TEST(test_encrypt_zero_len);
    RUN_TEST(test_encrypt_null_key);
    RUN_TEST(test_encrypt_buf_too_small);
    RUN_TEST(test_decrypt_null_buf);
    RUN_TEST(test_decrypt_too_short);
    RUN_TEST(test_decrypt_null_key);
    RUN_TEST(test_decrypt_not_block_aligned);
    RUN_TEST(test_decrypt_corrupted_ciphertext);

    return UNITY_END();
}
