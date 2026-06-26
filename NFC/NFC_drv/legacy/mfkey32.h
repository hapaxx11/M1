/*
 * mfkey32.h - On-device MIFARE Classic Crypto-1 key recovery (mfkey32v2)
 *
 * Recovers a sector key from two captured reader authentication attempts
 * (uid, nt, {nr}, {ar}). Self-contained canonical Crapto-1 implementation —
 * deliberately does NOT reuse mfc_crypto1.c, whose cipher is a non-standard
 * reimplementation.
 *
 * Crapto-1 recovery algorithm is GPL (bla, Karsten Nohl et al.); this file is
 * a memory-bounded port for the STM32H573. The M1 firmware is GPLv3 (COPYING.txt),
 * so this is license-compatible.
 *
 * Memory: the recovery runs entirely out of a fixed static arena (no malloc,
 * no FreeRTOS heap use), so its footprint is deterministic and isolated.
 */

#ifndef MFKEY32_H_
#define MFKEY32_H_

#include <stdint.h>
#include <stdbool.h>

/* Periodic tick during recovery (~every 32768 inner iterations). Use it to
 * feed the watchdog, refresh a progress UI, and poll for cancel. Return false
 * to abort the recovery. msb_round is the current chunk (0..15). May be NULL. */
typedef bool (*mfkey32_tick_cb)(int msb_round, void *ctx);

/* Recover the 48-bit key from two mfkey32 samples (same sector + key type).
 * All inputs are as captured on the wire:
 *   uid  - card UID (4 bytes, big-endian as a u32)
 *   nt0/nt1   - the two plaintext tag nonces
 *   nr0/nr1   - the two ENCRYPTED reader nonces ({nr})
 *   ar0/ar1   - the two ENCRYPTED reader answers ({ar})
 * On success writes the 48-bit key (MSB-first in the low 48 bits) and returns
 * true. Returns false if no key is found or the tick callback aborts.
 * Runs for ~minutes on a 75 MHz MCU; pass a tick callback to stay responsive. */
bool mfkey32v2_recover(uint32_t uid,
                       uint32_t nt0, uint32_t nr0, uint32_t ar0,
                       uint32_t nt1, uint32_t nr1, uint32_t ar1,
                       uint64_t *key_out,
                       mfkey32_tick_cb tick, void *tick_ctx);

/* Built-in correctness gate. Runs a published known-answer vector
 * (uid 2a234f80 ... -> key a0a1a2a3a4a5) plus an internal round-trip.
 * Returns true only if both pass. Safe to call before any nonce capture. */
bool mfkey32_selftest(void);

#endif /* MFKEY32_H_ */
