/* See COPYING.txt for license details. */

/**
 * @file   subghz_psa_bruteforce.h
 * @brief  PSA two-phase TEA brute-force key recovery — pure-logic module.
 *
 * Ports the brute-force search from the ProtoPirate reference
 * (RocketGod-git/ProtoPirate, protocols/psa_crypto_bf.c).  PSA keyfob frames
 * whose direct-XOR (type 0x23) decode path fails use a TEA-based scheme
 * (type 0x36); the only way to recover the cleartext serial/button/counter is
 * a key search over two 24-bit candidate ranges:
 *
 *   Phase 1 (key-derivation search, 0x23000000–0x23FFFFFF):
 *     each candidate seeds two TEA encryptions under a fixed key schedule to
 *     derive a per-candidate 128-bit working key, which decrypts the captured
 *     64-bit block.  Hit test: the candidate's low 24 bits echo in the
 *     plaintext top bits, plus an 8-bit additive CRC.
 *
 *   Phase 2 (XOR-key search, 0xF3000000–0xF3FFFFFF):
 *     the working key is a fixed schedule XORed with the candidate.  Same
 *     echo test, verified with a CRC-16 (poly 0x8005, init 0) over 6 bytes.
 *
 * The search needs only the captured 64-bit `Key` and 16-bit `Key_2` fields
 * and drives the protocol-agnostic `subghz_bruteforce_job_t` lifecycle for
 * progress/cancel accounting.  This file has no hardware dependencies and is
 * testable on the host.
 *
 * M1 Project — Hapax fork
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "subghz_bruteforce_job.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Constants                                                                   */
/*============================================================================*/

#define PSA_BF_TEA_DELTA   0x9E3779B9u
#define PSA_BF_TEA_ROUNDS  32u

/** Phase-1 candidate range [PSA_BF1_START, PSA_BF1_END). */
#define PSA_BF1_START      0x23000000u
#define PSA_BF1_END        0x24000000u
/** Phase-2 candidate range [PSA_BF2_START, PSA_BF2_END). */
#define PSA_BF2_START      0xF3000000u
#define PSA_BF2_END        0xF4000000u

/** Total candidate count across both phases (2 × 2^24). */
#define PSA_BF_TOTAL       ((PSA_BF1_END - PSA_BF1_START) + \
                            (PSA_BF2_END - PSA_BF2_START))

/** Phase selector for subghz_psa_bf_run_range(). */
typedef enum {
    PSA_BF_PHASE_1 = 1,  /**< key-derivation search */
    PSA_BF_PHASE_2 = 2,  /**< XOR-key search */
} subghz_psa_bf_phase_t;

/** Recovered frame fields, filled when the search finds the key. */
typedef struct {
    uint32_t serial;   /**< 24-bit keyfob serial */
    uint32_t seed;     /**< seed (same value as serial in the reference) */
    uint32_t counter;  /**< 28-bit rolling counter */
    uint32_t candidate;/**< candidate value that produced the hit */
    uint16_t crc;      /**< frame CRC byte from the decrypted block */
    uint8_t  button;   /**< 4-bit button id */
    uint8_t  type;     /**< always 0x36 on success */
} subghz_psa_bf_result_t;

/** Recovered-type marker written by a successful search. */
#define PSA_BF_RESULT_TYPE  0x36u

/** Precomputed per-round (key[sum&3] + sum) terms for fast TEA encrypt. */
typedef struct {
    uint32_t s0[PSA_BF_TEA_ROUNDS];
    uint32_t s1[PSA_BF_TEA_ROUNDS];
} subghz_psa_tea_schedule_t;

/*============================================================================*/
/* TEA primitives                                                              */
/*============================================================================*/

/** Fixed phase-1 "outer" TEA key schedule (from the ProtoPirate reference). */
extern const uint32_t subghz_psa_bf1_key_schedule[4];
/** Fixed phase-2 base key, XORed with the candidate (ProtoPirate reference). */
extern const uint32_t subghz_psa_bf2_key_schedule[4];

/**
 * @brief  Precompute the per-round (key[sum&3] + sum) schedule for the
 *         PSA TEA variant.  256 bytes of output; intended to be built once
 *         per search (or once per fixed schedule) on the worker stack.
 */
void subghz_psa_tea_build_schedule(const uint32_t key[4],
                                   subghz_psa_tea_schedule_t *out);

/** @brief  PSA-variant TEA encrypt using a precomputed schedule. */
void subghz_psa_tea_encrypt_scheduled(uint32_t *v0, uint32_t *v1,
                                      const subghz_psa_tea_schedule_t *sched);

/**
 * @brief  PSA-variant TEA encrypt with a raw 128-bit key.
 *
 * Equivalent to building a schedule from @p key and calling
 * subghz_psa_tea_encrypt_scheduled(); provided for test-vector synthesis and
 * one-off encryptions.
 */
void subghz_psa_tea_encrypt(uint32_t *v0, uint32_t *v1,
                            const uint32_t key[4]);

/** @brief  PSA-variant TEA decrypt with a raw 128-bit key. */
void subghz_psa_tea_decrypt(uint32_t *v0, uint32_t *v1,
                            const uint32_t key[4]);

/*============================================================================*/
/* Capture packing                                                             */
/*============================================================================*/

/**
 * @brief  Rebuild the 10-byte wire buffer from the saved capture fields.
 *
 * Inverse of the capture-side packing: bytes 0..7 are the 64-bit Key
 * (big-endian on air), bytes 8..9 are the 16-bit Key_2 (big-endian).
 */
void subghz_psa_bf_setup_byte_buffer(uint8_t buffer[10], uint32_t key1_low,
                                     uint32_t key1_high, uint16_t key2);

/**
 * @brief  Extract the 64-bit TEA ciphertext block (w0, w1) from the wire
 *         buffer built by subghz_psa_bf_setup_byte_buffer().
 */
void subghz_psa_bf_prepare_tea_data(const uint8_t buffer[10], uint32_t *w0,
                                    uint32_t *w1);

/*============================================================================*/
/* Integrity checks                                                            */
/*============================================================================*/

/**
 * @brief  Phase-1 hit CRC: additive 8-bit sum over the 4 bytes of v0 and the
 *         top 3 bytes of v1; must equal the low byte of v1.
 */
uint8_t subghz_psa_bf_tea_crc(uint32_t v0, uint32_t v1);

/**
 * @brief  Phase-2 hit CRC: CRC-16 (poly 0x8005, init 0, no reflection) over
 *         @p len bytes.
 */
uint16_t subghz_psa_bf_crc16(const uint8_t *buffer, uint32_t length);

/*============================================================================*/
/* Result extraction                                                           */
/*============================================================================*/

/**
 * @brief  Extract the recovered serial/button/counter/CRC fields from a
 *         decrypted 64-bit block (v0, v1) after a successful hit.
 */
void subghz_psa_bf_extract_result(uint32_t v0, uint32_t v1, uint32_t candidate,
                                  subghz_psa_bf_result_t *out);

/*============================================================================*/
/* Search                                                                      */
/*============================================================================*/

/**
 * @brief  Run one phase of the search over the candidate range [start, end).
 *
 * Drives @p job through the generic lifecycle: every candidate is reported
 * via subghz_bruteforce_job_advance() (progress, cancellation, and terminal
 * states all follow the job contract).  The caller must have armed the job
 * with subghz_bruteforce_job_begin(); on a hit the job's `result` field
 * carries the winning candidate value.
 *
 * @param  job     Armed lifecycle tracker (not NULL).
 * @param  key1    Captured 64-bit `Key` field.
 * @param  key2    Captured 16-bit `Key_2` field.
 * @param  phase   PSA_BF_PHASE_1 or PSA_BF_PHASE_2.
 * @param  start   First candidate to try (inclusive).
 * @param  end     Last candidate to try (exclusive); must be >= start and
 *                 within the phase's bounds.
 * @param  out     Receives the recovered fields on a hit (may be NULL).
 * @return true when the search found the key (job status FOUND).
 */
bool subghz_psa_bf_run_range(subghz_bruteforce_job_t *job, uint64_t key1,
                             uint16_t key2, subghz_psa_bf_phase_t phase,
                             uint32_t start, uint32_t end,
                             subghz_psa_bf_result_t *out);

/**
 * @brief  Run the full two-phase search (phase 1 over its whole range, then
 *         phase 2), arming @p job with PSA_BF_TOTAL candidates.
 *
 * @return true when the search found the key.
 */
bool subghz_psa_bf_run(subghz_bruteforce_job_t *job, uint64_t key1,
                       uint16_t key2, subghz_psa_bf_result_t *out);

#ifdef __cplusplus
}
#endif
