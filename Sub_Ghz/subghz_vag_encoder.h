/* See COPYING.txt for license details. */

/**
 * @file   subghz_vag_encoder.h
 * @brief  VAG frame encoder — TX for captured/decoded VAG keyfobs.
 *
 * Ports the encoder side of the ProtoPirate reference
 * (RocketGod-git/ProtoPirate, protocols/vag.c): the plaintext block is
 * serial (4B BE) | counter (3B LE) | button byte, encrypted with AUT64
 * (types 1/3/4, keystore required) or the fixed TEA schedule (type 2), then
 * emitted as the type-specific Manchester upload.
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

/** Upload entry counts per burst. */
#define VAG_ENC_UPLOAD_T12  635u   /* types 1/2 */
#define VAG_ENC_UPLOAD_T34  518u   /* types 3/4 (frame sent twice) */

/** Auto-select the AUT64 key by trial-decrypting the original capture. */
#define VAG_ENC_KEY_AUTO  0xFFu

/**
 * @brief  Button/dispatch mapping (reference vag_btn_to_byte +
 *         vag_get_dispatch_byte).  M1 reports buttons normalized to 1/2/4
 *         (unlock/lock/boot); the wire button byte is btn << 4.
 */
uint8_t subghz_vag_button_byte(uint8_t btn);
uint8_t subghz_vag_dispatch_byte(uint8_t btn_byte, uint8_t type);
uint8_t subghz_vag_effective_type(uint8_t type, uint8_t type_byte);
uint16_t subghz_vag_parse_key2(const char *bytes);

/**
 * @brief  Build the encrypted VAG frame for TX (reference
 *         vag_encoder_build_type1/2/3_4 frame portion).
 *
 * @param  serial         32-bit serial.
 * @param  cnt            24-bit rolling counter (already advanced by the
 *                        caller).
 * @param  btn            Normalized button (1/2/4).
 * @param  type           VAG frame type 1-4 (a Passat type-1 capture —
 *                        type_byte 0x00 — is emitted as type 2, as upstream).
 * @param  type_byte      Original capture's Key1 top byte (manufacturer tag).
 * @param  key_idx        AUT64 keystore index (0..2) or VAG_ENC_KEY_AUTO to
 *                        trial-decrypt @p orig_key1/@p orig_key2 with each
 *                        key and pick the one whose fields match.
 * @param  orig_key1      Original capture's Key1 (logical, uninverted).
 * @param  orig_key2      Original capture's Key2 (16-bit).
 * @param  packed_keys    AUT64 keystore blob (16-byte records), may be NULL
 *                        when @p keys_available is false.
 * @param  keys_available Whether the keystore holds real keys.
 * @param  key1_out       Receives the new Key1 (logical).
 * @param  key2_out       Receives the new Key2 (16-bit).
 * @return true on success (false: type 1/3/4 without usable keys).
 */
bool subghz_vag_encrypt_frame(uint32_t serial, uint32_t cnt, uint8_t btn,
                              uint8_t type, uint8_t type_byte,
                              uint8_t key_idx, uint64_t orig_key1,
                              uint16_t orig_key2, const uint8_t *packed_keys,
                              bool keys_available, uint64_t *key1_out,
                              uint16_t *key2_out);

/**
 * @brief  Build the upload waveform for the encrypted frame.
 *
 * Types 1/2: 220 preamble pairs @300 µs, sync (low/high 300), 16-bit prefix
 * (0xAF3F / 0xAF1C), inverted Key1 (64b) + Key2 (16b), 6000 µs gap.
 * Types 3/4: twice — 45 pairs @500 µs, sync (high 1000, low 500), 3 pairs
 * @750 µs, non-inverted Key1 + Key2, 10000 µs gap.
 *
 * @return Entry count (VAG_ENC_UPLOAD_T12 / _T34), or 0 on bad arguments.
 */
uint32_t subghz_vag_build_upload(subghz_upload_entry_t *upload,
                                 uint32_t capacity, uint8_t type,
                                 uint64_t key1, uint16_t key2);

#ifdef __cplusplus
}
#endif
