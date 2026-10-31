/* See COPYING.txt for license details. */
/*
 * m1_rng.h — 32-bit random source backed by the STM32H573 hardware TRNG.
 *
 * Replaces srand(HAL_GetTick())/rand() for anything that benefits from real
 * entropy (e.g. NFC nonces). The TRNG is brought up lazily via direct register
 * access (no HAL RNG driver is vendored in this tree). If the TRNG cannot be
 * started it transparently falls back to rand() so callers NEVER block or hang.
 */
#ifndef M1_RNG_H_
#define M1_RNG_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* @return a 32-bit random word (hardware TRNG, or rand() fallback). */
uint32_t m1_rng_get(void);

/* Fill bytes only from the hardware TRNG; fail rather than falling back to rand(). */
bool m1_rng_get_secure(uint32_t *value);
bool m1_crypto_random_bytes(uint8_t *buf, size_t len);

#endif /* M1_RNG_H_ */
