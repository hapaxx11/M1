/* See COPYING.txt for license details. */

/**
 * @file   mfc_layout.h
 * @brief  Pure-logic Mifare Classic sector/block layout helpers —
 *         no HAL, RTOS, crypto, or display deps.
 *
 * Extracted from nfc_poller.c following the Preferred Modularization Pattern.
 * All functions are testable on the host without any hardware stubs.
 *
 * Covers:
 *   - mfc_layout_from_sak()       — SAK → sectors + blocks layout
 *   - mfc_sector_first_block()    — sector index → first block number
 *   - mfc_sector_block_count()    — sector index → blocks-in-sector
 *   - mfc_uid4_from_nfcid()       — extract 4-byte UID for Crypto-1 auth
 *
 * M1 Project
 */

#ifndef MFC_LAYOUT_H_
#define MFC_LAYOUT_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Layout info
 * =========================================================================*/

/**
 * @brief  Mifare Classic card layout resolved from SAK byte.
 */
typedef struct {
    uint16_t total_sectors;   /**< Number of sectors (5, 16, 32, or 40) */
    uint16_t total_blocks;    /**< Total block count (20, 64, 128, or 256) */
} mfc_layout_t;

/**
 * Determine Mifare Classic layout from the SAK byte.
 *
 * Recognised SAK values:
 *   0x09 → Mini (5 sectors / 20 blocks)
 *   0x01, 0x08, 0x28 → 1K (16/64)
 *   0x10, 0x19 → 2K (32/128)
 *   0x11, 0x18, 0x38 → 4K (40/256)
 * Unrecognised SAK falls back to 1K layout.
 *
 * @param sak  SAK byte from NFC-A card selection.
 * @return     Layout info (sectors + blocks).
 */
mfc_layout_t mfc_layout_from_sak(uint8_t sak);

/**
 * Compute the first block number for a given sector index.
 *
 * Sectors 0–31 have 4 blocks each; sectors 32–39 have 16 blocks each
 * (for 4K cards).
 *
 * @param sector  Sector index (0-based).
 * @return        First block number in the sector.
 */
uint16_t mfc_sector_first_block(uint16_t sector);

/**
 * Return the number of blocks in a sector.
 *
 * @param sector        Sector index (0-based).
 * @param total_sectors Total sectors in the card (from mfc_layout_from_sak).
 * @return              4 for small sectors, 16 for large sectors (≥32 on 4K).
 */
uint16_t mfc_sector_block_count(uint16_t sector, uint16_t total_sectors);

/**
 * Extract the 4-byte UID used for Crypto-1 authentication.
 *
 * For 7-byte UIDs, bytes 3–6 (second cascade level) are used.
 * For 4-byte UIDs, bytes 0–3 are used directly.
 *
 * @param nfcid      Full NFCID1 byte array.
 * @param nfcid_len  Length of @p nfcid (4 or 7 typically).
 * @param uid4_out   Output buffer (must be at least 4 bytes).
 */
void mfc_uid4_from_nfcid(const uint8_t *nfcid, uint8_t nfcid_len,
                          uint8_t uid4_out[4]);

#ifdef __cplusplus
}
#endif

#endif /* MFC_LAYOUT_H_ */
