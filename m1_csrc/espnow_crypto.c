/* See COPYING.txt for license details. */

/*
 * espnow_crypto.c — App-layer authenticated encryption (Encrypt-then-MAC)
 *                   for the M1<->M1 peer link.  [Phase 4]
 *
 * See espnow_crypto.h for the envelope layout and key-schedule rationale.
 *
 * This translation unit contains only pure logic; the only external primitives
 * it uses are AES-256-CBC (m1_crypto) and SHA-256 (sha256.c).  It is fully
 * exercisable on the host (see tests/test_espnow_crypto.c) when m1_crypto.c is
 * compiled with -DM1_CRYPTO_SKIP_UID_FUNCTIONS and paired with the deterministic
 * m1_crypto_stub.c IV generator.
 */

#include "espnow_crypto.h"

#include <string.h>

#include "m1_crypto.h"   /* m1_crypto_encrypt_with_key / _decrypt_with_key */
#include "sha256.h"      /* Brad Conte SHA-256 (NFC/amiibo) */

/* ------------------------------------------------------------------------- */
/* HMAC-SHA256 (RFC 2104)                                                     */
/* ------------------------------------------------------------------------- */

#define HMAC_BLOCK_SIZE   64u   /* SHA-256 internal block size */
#define HMAC_DIGEST_SIZE  32u   /* SHA-256 output size */

void espnow_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                               const uint8_t *msg, size_t msg_len,
                               uint8_t out[32])
{
    SHA256_CTX ctx;
    uint8_t k0[HMAC_BLOCK_SIZE];
    uint8_t ipad[HMAC_BLOCK_SIZE];
    uint8_t opad[HMAC_BLOCK_SIZE];
    uint8_t inner[HMAC_DIGEST_SIZE];
    size_t i;

    /* Step 1: normalise the key to exactly HMAC_BLOCK_SIZE bytes (K0).
     * Keys longer than the block are hashed first; shorter keys are
     * zero-padded on the right. */
    memset(k0, 0, sizeof(k0));
    if (key_len > HMAC_BLOCK_SIZE) {
        sha256_init(&ctx);
        sha256_update(&ctx, key, key_len);
        sha256_final(&ctx, k0);            /* first 32 bytes, rest stay 0 */
    } else if (key_len > 0 && key != NULL) {
        memcpy(k0, key, key_len);
    }

    /* Step 2: build the inner/outer padded keys. */
    for (i = 0; i < HMAC_BLOCK_SIZE; i++) {
        ipad[i] = (uint8_t)(k0[i] ^ 0x36u);
        opad[i] = (uint8_t)(k0[i] ^ 0x5Cu);
    }

    /* Step 3: inner = H(ipad || msg). */
    sha256_init(&ctx);
    sha256_update(&ctx, ipad, HMAC_BLOCK_SIZE);
    if (msg_len > 0 && msg != NULL)
        sha256_update(&ctx, msg, msg_len);
    sha256_final(&ctx, inner);

    /* Step 4: out = H(opad || inner). */
    sha256_init(&ctx);
    sha256_update(&ctx, opad, HMAC_BLOCK_SIZE);
    sha256_update(&ctx, inner, HMAC_DIGEST_SIZE);
    sha256_final(&ctx, out);

    /* Scrub key-derived material from the stack. */
    memset(k0, 0, sizeof(k0));
    memset(ipad, 0, sizeof(ipad));
    memset(opad, 0, sizeof(opad));
    memset(inner, 0, sizeof(inner));
}

/* ------------------------------------------------------------------------- */
/* Key derivation                                                            */
/* ------------------------------------------------------------------------- */

espnow_crypto_status_t espnow_crypto_derive(const uint8_t *secret,
                                            size_t secret_len,
                                            espnow_crypto_key_t *out)
{
    SHA256_CTX ctx;
    uint8_t tag;

    if (secret == NULL || secret_len == 0 || out == NULL)
        return ESPNOW_CRYPTO_ERR_ARG;

    /* enc_key = SHA256(secret || 0x01) */
    tag = 0x01u;
    sha256_init(&ctx);
    sha256_update(&ctx, secret, secret_len);
    sha256_update(&ctx, &tag, 1u);
    sha256_final(&ctx, out->enc_key);

    /* mac_key = SHA256(secret || 0x02) */
    tag = 0x02u;
    sha256_init(&ctx);
    sha256_update(&ctx, secret, secret_len);
    sha256_update(&ctx, &tag, 1u);
    sha256_final(&ctx, out->mac_key);

    return ESPNOW_CRYPTO_OK;
}

/* ------------------------------------------------------------------------- */
/* Constant-time comparison                                                  */
/* ------------------------------------------------------------------------- */

/* Returns 0 if the two buffers are equal, non-zero otherwise.  Runs in time
 * independent of where the first difference (if any) occurs, so it does not
 * leak how many leading tag bytes matched. */
static int ct_memcmp(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0u;
    size_t i;

    for (i = 0; i < len; i++)
        diff |= (uint8_t)(a[i] ^ b[i]);

    return (int)diff;
}

/* ------------------------------------------------------------------------- */
/* Seal                                                                      */
/* ------------------------------------------------------------------------- */

espnow_crypto_status_t espnow_crypto_seal(const espnow_crypto_key_t *key,
                                          const uint8_t *plaintext,
                                          size_t pt_len,
                                          uint8_t *out,
                                          size_t out_cap,
                                          size_t *out_len)
{
    uint8_t tag[HMAC_DIGEST_SIZE];
    uint32_t enc_len;
    size_t body_len;   /* type + IV + ciphertext (everything MAC'd) */

    if (key == NULL || plaintext == NULL || out == NULL || out_len == NULL)
        return ESPNOW_CRYPTO_ERR_ARG;
    if (pt_len == 0)
        return ESPNOW_CRYPTO_ERR_ARG;
    if (pt_len > ESPNOW_CRYPTO_PLAINTEXT_MAX)
        return ESPNOW_CRYPTO_ERR_TOO_BIG;

    /* Ciphertext = ((pt_len / 16) + 1) * 16 (PKCS7 always adds a full block
     * when the length is an exact multiple). */
    {
        size_t ct_len = (((pt_len / ESPNOW_CRYPTO_BLOCK_LEN) + 1u) *
                         ESPNOW_CRYPTO_BLOCK_LEN);
        size_t need = 1u + ESPNOW_CRYPTO_IV_LEN + ct_len + ESPNOW_CRYPTO_TAG_LEN;
        if (need > out_cap)
            return ESPNOW_CRYPTO_ERR_BUF;
    }

    /* Layout: out[0] = type, out[1..] = IV||ciphertext, then tag. */
    out[0] = ESPNOW_APP_CRYPTO_BASE;

    /* Copy plaintext into the ciphertext region so m1_crypto can work in
     * place; it prepends the IV and applies PKCS7 padding. */
    memcpy(out + 1, plaintext, pt_len);
    enc_len = m1_crypto_encrypt_with_key(out + 1, (uint32_t)pt_len,
                                         (uint32_t)(out_cap - 1u),
                                         key->enc_key);
    if (enc_len == 0)
        return ESPNOW_CRYPTO_ERR_BUF;

    /* MAC over type || IV || ciphertext (Encrypt-then-MAC). */
    body_len = 1u + (size_t)enc_len;
    espnow_crypto_hmac_sha256(key->mac_key, ESPNOW_CRYPTO_KEY_LEN,
                              out, body_len, tag);
    memcpy(out + body_len, tag, ESPNOW_CRYPTO_TAG_LEN);

    *out_len = body_len + ESPNOW_CRYPTO_TAG_LEN;

    memset(tag, 0, sizeof(tag));
    return ESPNOW_CRYPTO_OK;
}

/* ------------------------------------------------------------------------- */
/* Open                                                                      */
/* ------------------------------------------------------------------------- */

espnow_crypto_status_t espnow_crypto_open(const espnow_crypto_key_t *key,
                                          const uint8_t *envelope,
                                          size_t env_len,
                                          uint8_t *out,
                                          size_t out_cap,
                                          size_t *out_len)
{
    uint8_t expect[HMAC_DIGEST_SIZE];
    uint8_t work[ESPNOW_CRYPTO_ENVELOPE_MAX];
    size_t body_len;   /* type + IV + ciphertext */
    size_t ct_region;  /* IV + ciphertext (what m1_crypto decrypts) */
    uint32_t dec_len;

    if (key == NULL || envelope == NULL || out == NULL || out_len == NULL)
        return ESPNOW_CRYPTO_ERR_ARG;

    /* Minimum envelope: type + IV + one cipher block + tag. */
    if (env_len < (1u + ESPNOW_CRYPTO_IV_LEN + ESPNOW_CRYPTO_BLOCK_LEN +
                   ESPNOW_CRYPTO_TAG_LEN))
        return ESPNOW_CRYPTO_ERR_FORMAT;
    if (env_len > ESPNOW_CRYPTO_ENVELOPE_MAX)
        return ESPNOW_CRYPTO_ERR_FORMAT;
    if (envelope[0] != ESPNOW_APP_CRYPTO_BASE)
        return ESPNOW_CRYPTO_ERR_FORMAT;

    body_len = env_len - ESPNOW_CRYPTO_TAG_LEN;
    ct_region = body_len - 1u;   /* strip the leading type byte */

    /* The IV+ciphertext region must be a whole number of AES blocks plus the
     * 16-byte IV. */
    if (ct_region < (ESPNOW_CRYPTO_IV_LEN + ESPNOW_CRYPTO_BLOCK_LEN))
        return ESPNOW_CRYPTO_ERR_FORMAT;
    if (((ct_region - ESPNOW_CRYPTO_IV_LEN) % ESPNOW_CRYPTO_BLOCK_LEN) != 0u)
        return ESPNOW_CRYPTO_ERR_FORMAT;

    /* Verify the tag BEFORE decrypting (Encrypt-then-MAC). */
    espnow_crypto_hmac_sha256(key->mac_key, ESPNOW_CRYPTO_KEY_LEN,
                              envelope, body_len, expect);
    if (ct_memcmp(expect, envelope + body_len, ESPNOW_CRYPTO_TAG_LEN) != 0) {
        memset(expect, 0, sizeof(expect));
        return ESPNOW_CRYPTO_ERR_AUTH;
    }

    /* Copy IV||ciphertext into a scratch buffer so decryption is in place
     * without mutating the caller's envelope. */
    memcpy(work, envelope + 1, ct_region);
    dec_len = m1_crypto_decrypt_with_key(work, (uint32_t)ct_region,
                                         key->enc_key);
    if (dec_len == 0) {
        memset(work, 0, sizeof(work));
        memset(expect, 0, sizeof(expect));
        return ESPNOW_CRYPTO_ERR_FORMAT;
    }

    if ((size_t)dec_len > out_cap) {
        memset(work, 0, sizeof(work));
        memset(expect, 0, sizeof(expect));
        return ESPNOW_CRYPTO_ERR_BUF;
    }

    memcpy(out, work, dec_len);
    *out_len = (size_t)dec_len;

    memset(work, 0, sizeof(work));
    memset(expect, 0, sizeof(expect));
    return ESPNOW_CRYPTO_OK;
}
