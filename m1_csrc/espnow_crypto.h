/* See COPYING.txt for license details. */

/*
 * espnow_crypto.h — App-layer authenticated encryption for the M1<->M1 peer
 *                   link (ESP-NOW compatibility layer).  [Phase 4]
 *
 * PURE LOGIC MODULE — no HAL, no FreeRTOS, no hardware access of its own.
 * It builds on top of two pieces of already-tested code:
 *
 *   - m1_crypto_encrypt_with_key() / m1_crypto_decrypt_with_key()
 *         AES-256-CBC + PKCS7 (m1_csrc/m1_crypto.c)
 *   - sha256_*()  (NFC/amiibo/sha256.c, Brad Conte public-domain impl)
 *
 * from which this module composes an Encrypt-then-MAC (EtM) authenticated
 * envelope.  EtM is the construction recommended for combining an unauthenticated
 * cipher (CBC) with a MAC: the MAC is computed over the ciphertext, and it is
 * verified BEFORE any decryption is attempted, so tampered or forged frames are
 * rejected without exposing the CBC decryptor to attacker-controlled input.
 *
 * ---------------------------------------------------------------------------
 * Envelope layout (this is the app-layer plaintext handed to espnow_chunk for
 * fragmentation; the whole thing is what travels over ESP-NOW):
 *
 *     +------+---------------------+------------------+-----------------+
 *     | type | IV (16)             | ciphertext (16n) | tag (16)        |
 *     +------+---------------------+------------------+-----------------+
 *      0xE0    m1_crypto output......................   truncated HMAC
 *
 *   byte 0            : type = ESPNOW_APP_CRYPTO_BASE (0xE0)
 *   byte 1..16        : IV (16 bytes)         }  as produced by
 *   byte 17..17+16n-1 : ciphertext (PKCS7)    }  m1_crypto_encrypt_with_key()
 *   last 16 bytes     : authentication tag = first 16 bytes of
 *                       HMAC-SHA256(mac_key, type || IV || ciphertext)
 *
 * The tag authenticates the type byte and the entire IV+ciphertext, so neither
 * the framing nor the payload can be altered without detection.
 *
 * ---------------------------------------------------------------------------
 * Key schedule.  A single shared secret (produced by the pairing handshake —
 * e.g. the confirm code combined with both peer MAC addresses) is expanded into
 * two independent 32-byte keys so the same key is never used for both roles:
 *
 *     enc_key = SHA256( secret || 0x01 )
 *     mac_key = SHA256( secret || 0x02 )
 *
 * Distinct keys for encryption and authentication is a standard requirement of
 * the Encrypt-then-MAC construction.
 */

#ifndef ESPNOW_CRYPTO_H_
#define ESPNOW_CRYPTO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "espnow_appmsg.h"   /* ESPNOW_APP_CRYPTO_BASE */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Sizes                                                                     */
/* ------------------------------------------------------------------------- */

/** AES / HMAC key length (bytes). */
#define ESPNOW_CRYPTO_KEY_LEN        32u

/** IV length prepended by m1_crypto_encrypt_with_key(). */
#define ESPNOW_CRYPTO_IV_LEN         16u

/** AES block size (PKCS7 padding granularity). */
#define ESPNOW_CRYPTO_BLOCK_LEN      16u

/** Truncated authentication-tag length (bytes) — 128-bit HMAC-SHA256. */
#define ESPNOW_CRYPTO_TAG_LEN        16u

/** Envelope framing overhead not counting the ciphertext body:
 *  type(1) + IV(16) + tag(16). */
#define ESPNOW_CRYPTO_OVERHEAD \
    (1u + ESPNOW_CRYPTO_IV_LEN + ESPNOW_CRYPTO_TAG_LEN)

/**
 * Maximum plaintext length that always yields an envelope that fits inside a
 * single reassembled app message (ESPNOW_CHUNK_MSG_MAX == 240):
 *
 *   envelope = 1 + 16 + ciphertext + 16
 *   ciphertext = ((plaintext / 16) + 1) * 16   (PKCS7 always adds >= 1 byte)
 *
 * With plaintext = 176 the ciphertext is 192 (12 blocks) and the envelope is
 * 1 + 16 + 192 + 16 = 225 <= 240.  Using 176 keeps a comfortable margin.
 */
#define ESPNOW_CRYPTO_PLAINTEXT_MAX  176u

/** Largest possible sealed envelope, for buffer sizing by callers. */
#define ESPNOW_CRYPTO_ENVELOPE_MAX \
    (ESPNOW_CRYPTO_OVERHEAD + \
     (((ESPNOW_CRYPTO_PLAINTEXT_MAX / ESPNOW_CRYPTO_BLOCK_LEN) + 1u) * \
      ESPNOW_CRYPTO_BLOCK_LEN))

/* ------------------------------------------------------------------------- */
/* Types                                                                     */
/* ------------------------------------------------------------------------- */

/** Derived per-session key material. */
typedef struct {
    uint8_t enc_key[ESPNOW_CRYPTO_KEY_LEN];  /**< AES-256 key. */
    uint8_t mac_key[ESPNOW_CRYPTO_KEY_LEN];  /**< HMAC-SHA256 key. */
} espnow_crypto_key_t;

/** Result codes for espnow_crypto_seal() / espnow_crypto_open(). */
typedef enum {
    ESPNOW_CRYPTO_OK = 0,          /**< Success. */
    ESPNOW_CRYPTO_ERR_ARG,         /**< NULL pointer / bad length argument. */
    ESPNOW_CRYPTO_ERR_TOO_BIG,     /**< Plaintext exceeds PLAINTEXT_MAX. */
    ESPNOW_CRYPTO_ERR_BUF,         /**< Output buffer too small. */
    ESPNOW_CRYPTO_ERR_FORMAT,      /**< Envelope malformed (bad type/length). */
    ESPNOW_CRYPTO_ERR_AUTH         /**< Authentication tag mismatch (tampered
                                        or wrong key). */
} espnow_crypto_status_t;

/* ------------------------------------------------------------------------- */
/* Key derivation                                                            */
/* ------------------------------------------------------------------------- */

/**
 * Expand a shared secret into an independent encryption key and MAC key.
 *
 * @param secret      Shared-secret bytes (e.g. pairing confirm material).
 * @param secret_len  Length of @p secret in bytes (must be > 0).
 * @param out         Destination key pair (must be non-NULL).
 * @return ESPNOW_CRYPTO_OK on success, ESPNOW_CRYPTO_ERR_ARG otherwise.
 */
espnow_crypto_status_t espnow_crypto_derive(const uint8_t *secret,
                                            size_t secret_len,
                                            espnow_crypto_key_t *out);

/* ------------------------------------------------------------------------- */
/* HMAC-SHA256 (exposed for testing / reuse)                                 */
/* ------------------------------------------------------------------------- */

/**
 * Compute the full 32-byte HMAC-SHA256 of @p msg under @p key.
 *
 * @param key     MAC key bytes.
 * @param key_len MAC key length.
 * @param msg     Message bytes (may be NULL only if @p msg_len is 0).
 * @param msg_len Message length.
 * @param out     32-byte output digest.
 */
void espnow_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                               const uint8_t *msg, size_t msg_len,
                               uint8_t out[32]);

/* ------------------------------------------------------------------------- */
/* Seal / open                                                               */
/* ------------------------------------------------------------------------- */

/**
 * Encrypt-then-MAC seal a plaintext into a self-authenticating envelope.
 *
 * @param key         Derived key pair.
 * @param plaintext   Plaintext bytes (length 1..ESPNOW_CRYPTO_PLAINTEXT_MAX).
 * @param pt_len      Plaintext length.
 * @param out         Output buffer for the envelope.
 * @param out_cap     Capacity of @p out (use ESPNOW_CRYPTO_ENVELOPE_MAX).
 * @param out_len     Receives the produced envelope length on success.
 * @return ESPNOW_CRYPTO_OK, or an error status.
 */
espnow_crypto_status_t espnow_crypto_seal(const espnow_crypto_key_t *key,
                                          const uint8_t *plaintext,
                                          size_t pt_len,
                                          uint8_t *out,
                                          size_t out_cap,
                                          size_t *out_len);

/**
 * Verify and open an envelope produced by espnow_crypto_seal().
 *
 * The authentication tag is checked (in constant time) BEFORE decryption; a
 * mismatch returns ESPNOW_CRYPTO_ERR_AUTH and nothing is written to @p out.
 *
 * @param key        Derived key pair.
 * @param envelope   Envelope bytes.
 * @param env_len    Envelope length.
 * @param out        Output buffer for the recovered plaintext.
 * @param out_cap    Capacity of @p out.
 * @param out_len    Receives the recovered plaintext length on success.
 * @return ESPNOW_CRYPTO_OK, or an error status.
 */
espnow_crypto_status_t espnow_crypto_open(const espnow_crypto_key_t *key,
                                          const uint8_t *envelope,
                                          size_t env_len,
                                          uint8_t *out,
                                          size_t out_cap,
                                          size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_CRYPTO_H_ */
