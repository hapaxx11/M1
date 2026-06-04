/* See COPYING.txt for license details. */

/**
 * @file   nfc_ntag.h
 * @brief  Pure-logic NTAG / Ultralight helpers — no HAL, RTOS, or FatFS deps.
 *
 * Extracted from nfc_poller.c (Phase B remaining extraction).
 * All functions are testable on the host without hardware stubs.
 *
 * Covers:
 *   - ntag_page_count_from_size()    — GET_VERSION size byte → total page count
 *   - ntag_generate_amiibo_pwd()     — Amiibo XOR-derived PWD + PACK
 *
 * M1 Project
 */

#ifndef NFC_NTAG_H_
#define NFC_NTAG_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * NTAG version / size helpers
 * =========================================================================*/

/**
 * Map the storage-size byte from NTAG GET_VERSION (byte 6) to a page count.
 *
 * Known mappings:
 *   0x0F → 45  (NTAG213, 144 B user memory)
 *   0x11 → 135 (NTAG215, 504 B user memory)
 *   0x13 → 231 (NTAG216, 888 B user memory)
 *
 * @param size_byte  Byte 6 of the 8-byte GET_VERSION response.
 * @return           Total page count, or 0 for an unrecognised size code.
 */
uint16_t ntag_page_count_from_size(uint8_t size_byte);

/* =========================================================================
 * Amiibo password derivation
 * =========================================================================*/

/**
 * Derive the 4-byte PWD and 2-byte PACK for an Amiibo NTAG215 tag.
 *
 * The derivation is a purely deterministic XOR function of the 7-byte UID
 * (no crypto, no HMAC).  Used by m1_t2t_read_ntag() to authenticate
 * password-protected Amiibo tags.
 *
 * Algorithm (from Amiibo documentation):
 *   pwd[0] = 0xAA ^ uid[1] ^ uid[3]
 *   pwd[1] = 0x55 ^ uid[2] ^ uid[4]
 *   pwd[2] = 0xAA ^ uid[3] ^ uid[5]
 *   pwd[3] = 0x55 ^ uid[4] ^ uid[6]
 *   pack[0] = 0x80
 *   pack[1] = 0x80
 *
 * @param uid      7-byte UID of the NTAG215 tag.
 * @param uid_len  Must be exactly 7; returns false otherwise.
 * @param pwd_out  Output buffer for the 4-byte password.
 * @param pack_out Output buffer for the 2-byte PACK.
 * @return         true on success, false if uid_len != 7 or any pointer is NULL.
 */
bool ntag_generate_amiibo_pwd(const uint8_t *uid, uint8_t uid_len,
                               uint8_t pwd_out[4], uint8_t pack_out[2]);

#ifdef __cplusplus
}
#endif

#endif /* NFC_NTAG_H_ */
