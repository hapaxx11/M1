/* See COPYING.txt for license details. */

/**
 * @file   subghz_psa_encoder.h
 * @brief  PSA frame encoder — TX for type 0x23 (direct-XOR) and type 0x36
 *         (TEA) captures.
 *
 * Ports the encoder side of the ProtoPirate reference
 * (RocketGod-git/ProtoPirate, protocols/psa.c + psa_crypto.c):
 *  - Type 0x36: re-encrypts a fresh frame with the TEA working key derived
 * using the phase and candidate recovered by the brute-force search.
 *  - Type 0x23: inverse of the direct-XOR decode network, with the nibble
 *    search that makes the checksum self-consistent.
 *  - Upload builder: 325-entry waveform (80 preamble pairs @250µs, a
 *    250/500/250 transition, Manchester MSB-first Key1 (64b) + validation
 *    field (16b), 1000µs end mark/space).
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "subghz_upload_entry.h"
#include "subghz_psa_bruteforce.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Upload waveform entry count for one PSA burst. */
#define PSA_ENC_UPLOAD_CAPACITY  325u

/** One upload entry (shared waveform type). */
typedef subghz_upload_entry_t psa_upload_entry_t;

/**
 * @brief  Encode a type-0x36 (TEA) PSA frame into the 10-byte wire buffer.
 *
 * @param  serial      24-bit keyfob serial (recovered by the brute force).
 * @param  counter     28-bit rolling counter.
 * @param  button      4-bit button id.
 * @param  preserve01  Original capture's wire bytes 0..1 (carries the 0xA
 *                     marker nibble), or NULL to derive them.
 * @param  candidate   Candidate value recovered by the search.
 * @param  phase       Search phase that recovered the key.
 * @param  buffer      Output: 10 wire bytes (Key1 = [0..7], Key2 = [8..9]).
 */
void subghz_psa_encode_buffer36(uint32_t serial, uint32_t counter,
                                uint8_t button, const uint8_t preserve01[2],
                                uint32_t candidate,
                                subghz_psa_bf_phase_t phase,
                                uint8_t buffer[10]);

/**
 * @brief  Encode a type-0x23 (direct-XOR) PSA frame into the 10-byte wire
 *         buffer.
 *
 * @param  serial      24-bit keyfob serial.
 * @param  counter     16-bit rolling counter.
 * @param  crc         Payload CRC byte (wire byte 7 pre-XOR; any value the
 *                     receiver associates with the remote — use the capture's
 *                     CRC field when known, else 0).
 * @param  button      4-bit button id.
 * @param  key2        Original capture's 16-bit Key_2 (used as wire byte 9
 *                     when @p has_key_2).
 * @param  has_key2    Whether @p key2 carries a captured value.
 * @param  preserve01  Original capture's wire bytes 0..1, or NULL.
 * @param  buffer      Output: 10 wire bytes.
 */
void subghz_psa_encode_buffer23(uint32_t serial, uint16_t counter,
                                uint8_t crc, uint8_t button, uint16_t key2,
                                bool has_key2, const uint8_t preserve01[2],
                                uint8_t buffer[10]);

/**
 * @brief  Build the upload waveform for one PSA burst: 80 preamble pairs,
 *         transition, Manchester Key1 + validation field, end gap.
 *
 * @param  upload    Caller buffer, at least PSA_ENC_UPLOAD_CAPACITY entries.
 * @param  capacity  Buffer capacity in entries.
 * @param  buffer    The 10 wire bytes from an encode call.
 * @return Number of entries written (always PSA_ENC_UPLOAD_CAPACITY), or 0 on
 *         bad arguments / insufficient capacity.
 */
uint32_t subghz_psa_build_upload(psa_upload_entry_t *upload,
                                 uint32_t capacity, const uint8_t buffer[10]);

#ifdef __cplusplus
}
#endif
