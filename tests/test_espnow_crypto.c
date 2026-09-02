/* See COPYING.txt for license details. */

/*
 * test_espnow_crypto.c — Host unit tests for the Phase 4 authenticated-
 * encryption envelope (m1_csrc/espnow_crypto.c).
 *
 * Covers:
 *   - HMAC-SHA256 against a published RFC 4231 known-answer vector
 *   - key derivation determinism & separation (enc_key != mac_key)
 *   - seal/open round-trip across several plaintext lengths
 *   - tamper detection (type byte, IV, ciphertext, tag)
 *   - truncation / short-envelope rejection
 *   - wrong-key rejection (fails as AUTH before any decryption)
 *   - argument / size-limit guards
 *
 * m1_crypto.c is compiled with -DM1_CRYPTO_SKIP_UID_FUNCTIONS and paired with
 * the deterministic m1_crypto_stub.c IV generator so encryption is reproducible.
 */

#include "unity.h"

#include <string.h>

#include "espnow_crypto.h"

void setUp(void) {}
void tearDown(void) {}

/* --------------------------------------------------------------------- */
/* Helpers                                                               */
/* --------------------------------------------------------------------- */

static espnow_crypto_key_t make_key(const char *secret)
{
    espnow_crypto_key_t k;
    espnow_crypto_status_t st =
        espnow_crypto_derive((const uint8_t *)secret, strlen(secret), &k);
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_OK, st);
    return k;
}

/* --------------------------------------------------------------------- */
/* HMAC-SHA256 known-answer (RFC 4231, Test Case 2)                      */
/* --------------------------------------------------------------------- */

void test_hmac_rfc4231_case2(void)
{
    /* key = "Jefe", data = "what do ya want for nothing?" */
    const uint8_t expected[32] = {
        0x5b,0xdc,0xc1,0x46,0xbf,0x60,0x75,0x4e,
        0x6a,0x04,0x24,0x26,0x08,0x95,0x75,0xc7,
        0x5a,0x00,0x3f,0x08,0x9d,0x27,0x39,0x83,
        0x9d,0xec,0x58,0xb9,0x64,0xec,0x38,0x43
    };
    uint8_t out[32];

    espnow_crypto_hmac_sha256((const uint8_t *)"Jefe", 4,
                              (const uint8_t *)"what do ya want for nothing?", 28,
                              out);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, 32);
}

void test_hmac_long_key_is_hashed(void)
{
    /* Key longer than the 64-byte block must be hashed first; just verify it
     * produces a stable, non-zero digest and differs from a short key. */
    uint8_t longkey[80];
    uint8_t a[32], b[32];
    size_t i;

    for (i = 0; i < sizeof(longkey); i++)
        longkey[i] = (uint8_t)i;

    espnow_crypto_hmac_sha256(longkey, sizeof(longkey),
                              (const uint8_t *)"msg", 3, a);
    espnow_crypto_hmac_sha256(longkey, 8, (const uint8_t *)"msg", 3, b);
    TEST_ASSERT_NOT_EQUAL(0, memcmp(a, b, 32));
}

/* --------------------------------------------------------------------- */
/* Key derivation                                                        */
/* --------------------------------------------------------------------- */

void test_derive_is_deterministic(void)
{
    espnow_crypto_key_t a = make_key("shared-secret");
    espnow_crypto_key_t b = make_key("shared-secret");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a.enc_key, b.enc_key, ESPNOW_CRYPTO_KEY_LEN);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a.mac_key, b.mac_key, ESPNOW_CRYPTO_KEY_LEN);
}

void test_derive_enc_and_mac_keys_differ(void)
{
    espnow_crypto_key_t a = make_key("shared-secret");
    TEST_ASSERT_NOT_EQUAL(0,
        memcmp(a.enc_key, a.mac_key, ESPNOW_CRYPTO_KEY_LEN));
}

void test_derive_distinct_secrets_distinct_keys(void)
{
    espnow_crypto_key_t a = make_key("secret-A");
    espnow_crypto_key_t b = make_key("secret-B");
    TEST_ASSERT_NOT_EQUAL(0,
        memcmp(a.enc_key, b.enc_key, ESPNOW_CRYPTO_KEY_LEN));
}

void test_derive_rejects_bad_args(void)
{
    espnow_crypto_key_t k;
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_ARG,
        espnow_crypto_derive(NULL, 4, &k));
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_ARG,
        espnow_crypto_derive((const uint8_t *)"x", 0, &k));
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_ARG,
        espnow_crypto_derive((const uint8_t *)"x", 1, NULL));
}

/* --------------------------------------------------------------------- */
/* Round-trip                                                            */
/* --------------------------------------------------------------------- */

static void roundtrip_len(size_t n)
{
    espnow_crypto_key_t key = make_key("session-key-material");
    uint8_t pt[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    uint8_t rec[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    size_t env_len = 0, rec_len = 0;
    size_t i;

    for (i = 0; i < n; i++)
        pt[i] = (uint8_t)(0x40u + (i & 0x3Fu));

    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_OK,
        espnow_crypto_seal(&key, pt, n, env, sizeof(env), &env_len));
    /* Type byte and tag must be present. */
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_APP_CRYPTO_BASE, env[0]);
    TEST_ASSERT_TRUE(env_len >= ESPNOW_CRYPTO_OVERHEAD + ESPNOW_CRYPTO_BLOCK_LEN);

    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_OK,
        espnow_crypto_open(&key, env, env_len, rec, sizeof(rec), &rec_len));
    TEST_ASSERT_EQUAL_size_t(n, rec_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(pt, rec, n);
}

void test_roundtrip_one_byte(void)          { roundtrip_len(1); }
void test_roundtrip_exact_block(void)       { roundtrip_len(16); }
void test_roundtrip_odd_length(void)        { roundtrip_len(37); }
void test_roundtrip_max_length(void)        { roundtrip_len(ESPNOW_CRYPTO_PLAINTEXT_MAX); }

/* --------------------------------------------------------------------- */
/* Tamper detection                                                      */
/* --------------------------------------------------------------------- */

static void seal_sample(espnow_crypto_key_t *key, uint8_t *env, size_t *env_len)
{
    const uint8_t pt[] = "peer-link-secret-payload";
    *key = make_key("tamper-key");
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_OK,
        espnow_crypto_seal(key, pt, sizeof(pt) - 1u, env,
                           ESPNOW_CRYPTO_ENVELOPE_MAX, env_len));
}

void test_tamper_ciphertext_detected(void)
{
    espnow_crypto_key_t key;
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    uint8_t rec[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    size_t env_len = 0, rec_len = 0;

    seal_sample(&key, env, &env_len);
    env[1 + ESPNOW_CRYPTO_IV_LEN] ^= 0x01u;   /* flip a ciphertext bit */
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_AUTH,
        espnow_crypto_open(&key, env, env_len, rec, sizeof(rec), &rec_len));
}

void test_tamper_iv_detected(void)
{
    espnow_crypto_key_t key;
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    uint8_t rec[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    size_t env_len = 0, rec_len = 0;

    seal_sample(&key, env, &env_len);
    env[1] ^= 0x80u;   /* flip an IV bit */
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_AUTH,
        espnow_crypto_open(&key, env, env_len, rec, sizeof(rec), &rec_len));
}

void test_tamper_type_detected(void)
{
    espnow_crypto_key_t key;
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    uint8_t rec[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    size_t env_len = 0, rec_len = 0;

    seal_sample(&key, env, &env_len);
    env[0] = ESPNOW_APP_CRYPTO_BASE + 1u;   /* still in crypto range 0xE0..0xEF */
    /* Format check passes only for exact base; a changed type is rejected as
     * format (fast path) — but even within range the MAC would catch it. */
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_FORMAT,
        espnow_crypto_open(&key, env, env_len, rec, sizeof(rec), &rec_len));
}

void test_tamper_tag_detected(void)
{
    espnow_crypto_key_t key;
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    uint8_t rec[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    size_t env_len = 0, rec_len = 0;

    seal_sample(&key, env, &env_len);
    env[env_len - 1u] ^= 0xFFu;   /* flip the last tag byte */
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_AUTH,
        espnow_crypto_open(&key, env, env_len, rec, sizeof(rec), &rec_len));
}

/* --------------------------------------------------------------------- */
/* Wrong key                                                             */
/* --------------------------------------------------------------------- */

void test_wrong_key_rejected(void)
{
    espnow_crypto_key_t key;
    espnow_crypto_key_t other = make_key("a-different-secret");
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    uint8_t rec[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    size_t env_len = 0, rec_len = 0;

    seal_sample(&key, env, &env_len);
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_AUTH,
        espnow_crypto_open(&other, env, env_len, rec, sizeof(rec), &rec_len));
}

/* --------------------------------------------------------------------- */
/* Format / size guards                                                  */
/* --------------------------------------------------------------------- */

void test_open_rejects_short_envelope(void)
{
    espnow_crypto_key_t key = make_key("k");
    uint8_t env[8] = { ESPNOW_APP_CRYPTO_BASE };
    uint8_t rec[16];
    size_t rec_len = 0;
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_FORMAT,
        espnow_crypto_open(&key, env, sizeof(env), rec, sizeof(rec), &rec_len));
}

void test_open_rejects_truncated_body(void)
{
    espnow_crypto_key_t key;
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    uint8_t rec[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    size_t env_len = 0, rec_len = 0;

    seal_sample(&key, env, &env_len);
    /* Drop one byte so the IV+ciphertext region is no longer block-aligned. */
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_FORMAT,
        espnow_crypto_open(&key, env, env_len - 1u, rec, sizeof(rec), &rec_len));
}

void test_open_rejects_wrong_type(void)
{
    espnow_crypto_key_t key;
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    uint8_t rec[ESPNOW_CRYPTO_PLAINTEXT_MAX];
    size_t env_len = 0, rec_len = 0;

    seal_sample(&key, env, &env_len);
    env[0] = ESPNOW_APP_MSG_BASE;   /* outside the crypto range */
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_FORMAT,
        espnow_crypto_open(&key, env, env_len, rec, sizeof(rec), &rec_len));
}

void test_seal_rejects_oversize_plaintext(void)
{
    espnow_crypto_key_t key = make_key("k");
    uint8_t pt[ESPNOW_CRYPTO_PLAINTEXT_MAX + 1];
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX + 32];
    size_t env_len = 0;
    memset(pt, 0x11, sizeof(pt));
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_TOO_BIG,
        espnow_crypto_seal(&key, pt, sizeof(pt), env, sizeof(env), &env_len));
}

void test_seal_rejects_small_buffer(void)
{
    espnow_crypto_key_t key = make_key("k");
    const uint8_t pt[] = "hello";
    uint8_t env[8];
    size_t env_len = 0;
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_BUF,
        espnow_crypto_seal(&key, pt, sizeof(pt) - 1u, env, sizeof(env), &env_len));
}

void test_seal_rejects_null_and_zero(void)
{
    espnow_crypto_key_t key = make_key("k");
    const uint8_t pt[] = "hi";
    uint8_t env[ESPNOW_CRYPTO_ENVELOPE_MAX];
    size_t env_len = 0;
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_ARG,
        espnow_crypto_seal(NULL, pt, 2, env, sizeof(env), &env_len));
    TEST_ASSERT_EQUAL(ESPNOW_CRYPTO_ERR_ARG,
        espnow_crypto_seal(&key, pt, 0, env, sizeof(env), &env_len));
}

/* --------------------------------------------------------------------- */
/* Runner                                                                */
/* --------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_hmac_rfc4231_case2);
    RUN_TEST(test_hmac_long_key_is_hashed);

    RUN_TEST(test_derive_is_deterministic);
    RUN_TEST(test_derive_enc_and_mac_keys_differ);
    RUN_TEST(test_derive_distinct_secrets_distinct_keys);
    RUN_TEST(test_derive_rejects_bad_args);

    RUN_TEST(test_roundtrip_one_byte);
    RUN_TEST(test_roundtrip_exact_block);
    RUN_TEST(test_roundtrip_odd_length);
    RUN_TEST(test_roundtrip_max_length);

    RUN_TEST(test_tamper_ciphertext_detected);
    RUN_TEST(test_tamper_iv_detected);
    RUN_TEST(test_tamper_type_detected);
    RUN_TEST(test_tamper_tag_detected);

    RUN_TEST(test_wrong_key_rejected);

    RUN_TEST(test_open_rejects_short_envelope);
    RUN_TEST(test_open_rejects_truncated_body);
    RUN_TEST(test_open_rejects_wrong_type);
    RUN_TEST(test_seal_rejects_oversize_plaintext);
    RUN_TEST(test_seal_rejects_small_buffer);
    RUN_TEST(test_seal_rejects_null_and_zero);

    return UNITY_END();
}
