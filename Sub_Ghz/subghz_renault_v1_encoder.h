/* See COPYING.txt for license details. */

/**
 * @file   subghz_renault_v1_encoder.h
 * @brief  Renault V1 (Hitag2) frame encoder — TX after seed recovery.
 *
 * Ports the seed-based re-encryption and upload (waveform) builder from the
 * ProtoPirate reference (RocketGod-git/ProtoPirate, protocols/renault_v1.c):
 * given the keyfob serial, a fresh rolling counter, the button, and the
 * recovered 32-bit seed (IV), produces a new valid 11-byte frame and the
 * 1295-entry (level, duration) upload describing the on-air waveform
 * (TE = 125 µs Manchester, 250 preamble pairs, 3 long + 3 short frames).
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Timing / upload constants (from the reference)                              */
/*============================================================================*/

#define RENAULT_V1_TE_US            125u
#define RENAULT_V1_HEADER_LOW_US    1500u
#define RENAULT_V1_HEADER_HIGH_US   1000u
#define RENAULT_V1_SHORT_GAP_US     21500u
#define RENAULT_V1_PREAMBLE_PAIRS   250u
#define RENAULT_V1_LONG_FRAMES      3u
#define RENAULT_V1_SHORT_FRAMES     3u
#define RENAULT_V1_UPLOAD_CAPACITY  1295u

/** One upload entry: a constant level held for `duration` microseconds. */
typedef struct {
    uint32_t duration;
    bool     level;  /* true = mark (high), false = space (low) */
} renault_v1_upload_entry_t;

/*============================================================================*/
/* Frame encryption (seed path)                                                */
/*============================================================================*/

/**
 * @brief  Build the 32-bit IV from a rolling counter, button and recovered
 *         seed (reference hitag2_build_iv layout).
 */
void subghz_renault_v1_build_iv(uint32_t cnt, uint8_t btn, uint32_t seed,
                                uint8_t iv[4]);

/**
 * @brief  Encrypt a fresh Renault V1 frame (reference hitag2_encrypt_frame).
 *
 * @param  serial  32-bit keyfob serial.
 * @param  cnt     Rolling counter (low 16 bits used).
 * @param  btn     4-bit button id.
 * @param  seed    Recovered 32-bit seed (from the brute-force search).
 * @param  out     Receives the 11-byte wire payload (with XOR8 checksum).
 * @param  iv      Receives the IV actually used (may be NULL).
 */
void subghz_renault_v1_encrypt_frame(uint32_t serial, uint32_t cnt,
                                     uint8_t btn, uint32_t seed,
                                     uint8_t out[11], uint8_t iv[4]);

/*============================================================================*/
/* Upload (waveform) builder                                                   */
/*============================================================================*/

/**
 * @brief  Build the upload waveform for one burst: 250 preamble pairs, then
 *         3 long frames (header + 16 header bits + 64 key bits + 24 key_2
 *         bits) and 3 short frames (header + 16 header bits + 10 short-key
 *         bits + 21.5 ms gap), Manchester at TE = 125 µs.
 *
 * @param  upload    Caller buffer, at least RENAULT_V1_UPLOAD_CAPACITY entries.
 * @param  capacity  Buffer capacity in entries.
 * @param  key       First 8 frame bytes, big-endian as u64.
 * @param  key2_24   Last 3 frame bytes as a 24-bit value.
 * @return Number of entries written (always RENAULT_V1_UPLOAD_CAPACITY), or 0
 *         on bad arguments / insufficient capacity.
 */
uint32_t subghz_renault_v1_build_upload(renault_v1_upload_entry_t *upload,
                                        uint32_t capacity, uint64_t key,
                                        uint32_t key2_24);

#ifdef __cplusplus
}
#endif
