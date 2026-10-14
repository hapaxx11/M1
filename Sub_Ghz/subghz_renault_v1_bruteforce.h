/* See COPYING.txt for license details. */

/**
 * @file   subghz_renault_v1_bruteforce.h
 * @brief  Renault V1 Hitag2 IV (seed) brute-force recovery — pure-logic module.
 *
 * Ports the Hitag2 IV search from the ProtoPirate reference
 * (RocketGod-git/ProtoPirate, protocols/renault_v1.c).  M1's Renault V1
 * decoder already recovers serial/button/counter cipher-free; this search
 * recovers the 32-bit IV ("seed") needed to re-encode fresh frames, by
 * clocking the Hitag2 cipher over 2^18 candidate IVs until the produced
 * keystream equals the captured 32-bit hop word.
 *
 * The search input is the 11-byte capture frame: the 64-bit `Key` field
 * (big-endian) followed by the low 3 bytes of the `Key_2` field — exactly the
 * layout ProtoPirate writes to its captures, and the layout the M1 Renault V1
 * decoder persists for live captures.
 *
 * The search drives the protocol-agnostic `subghz_bruteforce_job_t` lifecycle
 * for progress/cancel accounting.  This file has no hardware dependencies and
 * is testable on the host.
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

/** Total IV candidate count (2^18). */
#define RENAULT_V1_BF_CANDIDATES  0x40000u

/** Capture frame length: 8-byte Key + 3-byte Key_2. */
#define RENAULT_V1_BF_FRAME_BYTES  11u

/*============================================================================*/
/* Frame preparation (ported verbatim from the reference)                      */
/*============================================================================*/

/**
 * @brief  Build the 11-byte search frame from the capture fields: 64-bit Key
 *         (big-endian) then the low 3 bytes of Key_2 (big-endian).
 */
void subghz_renault_v1_bf_frame_from_capture(uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                                             uint64_t key, uint32_t key2_low24);

/**
 * @brief  Extract the search constants from a capture frame: serial, the
 *         permuted serial bytes, the 4-byte hop target, and the fixed IV
 *         prefix bytes (iv0, fp).
 */
void subghz_renault_v1_bf_prepare(const uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                                  uint8_t serial_be[4], uint8_t perm[6],
                                  uint8_t hop_target[4],
                                  uint8_t *iv0, uint8_t *fp);

/*============================================================================*/
/* Hitag2 cipher (byte-oriented form, ported verbatim)                         */
/*============================================================================*/

/** @brief  Serial-byte permutation producing key-material bytes. */
void subghz_hitag2_serial_permute(const uint8_t serial[4], uint8_t perm[6]);

/**
 * @brief  Clock the Hitag2 cipher: 32 IV-absorption clocks + 32 keystream
 *         clocks.  @p keystream receives the 32-bit keystream word (the
 *         predicted hop for the given serial/IV).
 */
void subghz_hitag2_keystream(const uint8_t serial_be[4], const uint8_t perm[6],
                             const uint8_t iv[4], uint8_t keystream[4]);

/**
 * @brief  Extended form of subghz_hitag2_keystream() that also returns the
 *         final LFSR state byte (state[5] after 64 clocks) via
 *         @p state5_out (may be NULL).  The Renault V1 encoder needs it for
 *         the last hop byte.
 */
void subghz_hitag2_keystream_ex(const uint8_t serial_be[4],
                                const uint8_t perm[6], const uint8_t iv[4],
                                uint8_t keystream[4], uint8_t *state5_out);

/*============================================================================*/
/* Search                                                                      */
/*============================================================================*/

/**
 * @brief  Run the IV search over candidate range [start, end).
 *
 * Drives @p job through the generic lifecycle (progress, cancellation and
 * terminal states follow the job contract).  The caller must have armed the
 * job with subghz_bruteforce_job_begin(); on a hit the job's `result` field
 * carries the winning candidate index.
 *
 * @param  job     Armed lifecycle tracker (not NULL).
 * @param  frame   11-byte capture frame (Key BE + Key_2 low 3 bytes).
 * @param  start   First candidate to try (inclusive), < RENAULT_V1_BF_CANDIDATES.
 * @param  end     Last candidate to try (exclusive), <= RENAULT_V1_BF_CANDIDATES.
 * @param  iv_out  Receives the recovered 4-byte IV on a hit (may be NULL).
 * @return true when the search found the IV (job status FOUND).
 */
bool subghz_renault_v1_bf_run_range(subghz_bruteforce_job_t *job,
                                    const uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                                    uint32_t start, uint32_t end,
                                    uint8_t iv_out[4]);

/**
 * @brief  Run the full IV search (all RENAULT_V1_BF_CANDIDATES candidates),
 *         arming @p job unless it is already running.
 *
 * @return true when the search found the IV.
 */
bool subghz_renault_v1_bf_run(subghz_bruteforce_job_t *job,
                              const uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                              uint8_t iv_out[4]);

/*============================================================================*/
/* Result helpers                                                              */
/*============================================================================*/

/** @brief  Pack a recovered IV as a big-endian u32 (the `Seed` field). */
uint32_t subghz_renault_v1_bf_seed_from_iv(const uint8_t iv[4]);

/**
 * @brief  Slice the plaintext fields out of an 11-byte capture frame
 *         (serial, 4-bit button, 10-bit partial counter, 32-bit hop).
 *         Pure; no checksum validation (capture-time concern).
 */
void subghz_renault_v1_bf_unpack_frame(const uint8_t frame[RENAULT_V1_BF_FRAME_BYTES],
                                       uint32_t *serial, uint8_t *button,
                                       uint16_t *counter10, uint32_t *hop);

/**
 * @brief  Reconstruct the full rolling counter from the recovered seed
 *         (reference formula; the 10-bit frame counter is only partial).
 */
uint32_t subghz_renault_v1_bf_counter_from_seed(uint32_t seed);

#ifdef __cplusplus
}
#endif
