/* See COPYING.txt for license details. */

/**
 * @file   subghz_kia_v6_encoder.h
 * @brief  Kia V6 frame encoder — TX for captured/decoded Kia V6 keyfobs.
 *
 * Ports the encoder side of the ProtoPirate reference
 * (RocketGod-git/ProtoPirate, protocols/kia_v6.c): AES-128-ECB encryption of
 * the 16-byte payload with the keystore-derived key, packing into the three
 * on-air parts, and the 1928-entry upload builder (two messages: 640 and 38
 * preamble pairs, TE = 200 µs Manchester, all data bits inverted on air).
 *
 * Requires the injected Kia V6 keystore (m1_kia_v6_keys_builtin +
 * availability flag in subghz_protopirate_keys_builtin.h) — public builds
 * without the keystore cannot encrypt.
 *
 * This file has no hardware dependencies and is testable on the host.
 *
 * M1 Project — Hapax fork
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "subghz_upload_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Upload waveform entry count for one Kia V6 burst (both messages). */
#define KIA_V6_ENC_UPLOAD_SIZE  1928u

/**
 * @brief  AES S-box value, computed (GF(2^8) inverse + affine transform).
 *
 * The payload's byte 12 carries sbox[counter & 0xFF]; the vendored tiny-AES
 * table is translation-unit-static, so the value is computed instead.
 * Host-tested against the canonical AES S-box vectors.
 */
uint8_t subghz_kia_v6_aes_sbox(uint8_t value);

/**
 * @brief  Assemble the 16-byte AES key from the two keystore halves
 *         (XOR masks 0x84AF25FB / 0x638766AB per half, big-endian words).
 */
void subghz_kia_v6_aes_key(const uint64_t keys[2], uint8_t aes_key[16]);

/**
 * @brief  Encrypt a fresh Kia V6 frame (reference kia_v6_encrypt_payload).
 *
 * @param  fx      Fx field (0 when unknown).
 * @param  serial  24-bit keyfob serial.
 * @param  btn     4-bit button id.
 * @param  cnt     32-bit rolling counter.
 * @param  keys    The two 64-bit keystore halves.
 * @param  out     Receives the 144-bit wire frame as 18 bytes (2 metadata
 *                 bytes + 16 ciphertext bytes), pre-inversion.
 */
void subghz_kia_v6_encrypt_frame(uint8_t fx, uint32_t serial, uint8_t btn,
                                 uint32_t cnt, const uint64_t keys[2],
                                 uint8_t out[18]);

/**
 * @brief  Build the upload waveform: message with 640 preamble pairs, 400 µs
 *         low separator, message with 38 preamble pairs, 400 µs low.
 *         Per message: preamble, sync (low 200 / high 400 / low 200), then
 *         61 part1 bits + 64 part2 bits + 16 part3 bits, Manchester at
 *         TE = 200 µs, MSB first, all data bits inverted.
 *
 * @param  upload    Caller buffer, at least KIA_V6_ENC_UPLOAD_SIZE entries.
 * @param  capacity  Buffer capacity in entries.
 * @param  frame     The 18-byte frame from subghz_kia_v6_encrypt_frame().
 * @return Number of entries written (always KIA_V6_ENC_UPLOAD_SIZE), or 0 on
 *         bad arguments / insufficient capacity.
 */
uint32_t subghz_kia_v6_build_upload(subghz_upload_entry_t *upload,
                                    uint32_t capacity, const uint8_t frame[18]);

#ifdef __cplusplus
}
#endif
