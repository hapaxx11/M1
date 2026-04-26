/* See COPYING.txt for license details. */

/*
 * m1_crypto_stub.c — Host-side stub for m1_crypto UID-dependent functions.
 *
 * When m1_csrc/m1_crypto.c is compiled with -DM1_CRYPTO_SKIP_UID_FUNCTIONS
 * the two hardware-dependent functions below are excluded from that
 * translation unit.  This stub provides deterministic replacements so that
 * the AES-256-CBC engine (m1_crypto_encrypt_with_key /
 * m1_crypto_decrypt_with_key) can be exercised on the host without access to
 * the STM32H5 Unique-ID registers or HAL_GetTick().
 *
 * The stub IV is fixed (all 0xAB bytes), making encrypt→decrypt roundtrips
 * fully deterministic and reproducible in unit tests.
 */

#include "m1_crypto.h"
#include <string.h>

/* m1_crypto_generate_iv — fixed deterministic IV for tests */
void m1_crypto_generate_iv(uint8_t iv[M1_CRYPTO_IV_SIZE])
{
    memset(iv, 0xAB, M1_CRYPTO_IV_SIZE);
}

/* m1_crypto_derive_key — fixed all-zeros device key for tests.
 * This is only called by m1_crypto_encrypt() / m1_crypto_decrypt()
 * (the UID-bound wrappers).  The keeloq keystore tests use
 * m1_crypto_encrypt_with_key() / m1_crypto_decrypt_with_key() directly,
 * so this stub is provided only to satisfy the linker. */
void m1_crypto_derive_key(uint8_t key[M1_CRYPTO_AES_KEY_SIZE])
{
    memset(key, 0, M1_CRYPTO_AES_KEY_SIZE);
}
