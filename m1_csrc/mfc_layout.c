/* See COPYING.txt for license details. */

/**
 * @file   mfc_layout.c
 * @brief  Pure-logic Mifare Classic sector/block layout helpers.
 *
 * Extracted from nfc_poller.c (Phase B blocker removal).
 * Zero HAL/RTOS/crypto deps — fully testable on the host.
 */

#include "mfc_layout.h"
#include <string.h>

/* =========================================================================
 * mfc_layout_from_sak  (was static mfc_get_layout_from_sak in nfc_poller.c)
 * =========================================================================*/

mfc_layout_t mfc_layout_from_sak(uint8_t sak)
{
    mfc_layout_t lay;

    switch (sak) {
    case 0x09: /* Mini */
        lay.total_sectors = 5;
        lay.total_blocks  = 20;
        break;
    case 0x01: /* Classic 1K TNP3xxx */
    case 0x08: /* Classic 1K */
    case 0x28: /* Classic EV1 1K */
        lay.total_sectors = 16;
        lay.total_blocks  = 64;
        break;
    case 0x10: /* Plus 2K SL2 */
    case 0x19: /* Classic 2K */
        lay.total_sectors = 32;
        lay.total_blocks  = 128;
        break;
    case 0x11: /* Plus 4K SL2 */
    case 0x18: /* Classic 4K */
    case 0x38: /* Classic EV1 4K */
        lay.total_sectors = 40;
        lay.total_blocks  = 256;
        break;
    default:
        /* Fallback to 1K layout */
        lay.total_sectors = 16;
        lay.total_blocks  = 64;
        break;
    }
    return lay;
}

/* =========================================================================
 * mfc_sector_first_block  (was static mfc_sector_to_first_block)
 * =========================================================================*/

uint16_t mfc_sector_first_block(uint16_t sector)
{
    if (sector < 32)
        return (uint16_t)(sector * 4);
    else
        return (uint16_t)(128 + (sector - 32) * 16);
}

/* =========================================================================
 * mfc_sector_block_count
 * =========================================================================*/

uint16_t mfc_sector_block_count(uint16_t sector, uint16_t total_sectors)
{
    /* Sectors < 32 always have 4 blocks.
     * Sectors 32+ on 4K cards (total_sectors > 32) have 16 blocks. */
    if (sector < 32 || total_sectors <= 32)
        return 4;
    return 16;
}

/* =========================================================================
 * mfc_uid4_from_nfcid  (extracted from m1_read_mifareclassic)
 * =========================================================================*/

void mfc_uid4_from_nfcid(const uint8_t *nfcid, uint8_t nfcid_len,
                          uint8_t uid4_out[4])
{
    if (!nfcid || !uid4_out) {
        if (uid4_out) memset(uid4_out, 0, 4);
        return;
    }

    if (nfcid_len >= 7) {
        /* 7-byte UID: use bytes 3..6 (second cascade level) */
        memcpy(uid4_out, &nfcid[3], 4);
    } else if (nfcid_len >= 4) {
        memcpy(uid4_out, nfcid, 4);
    } else {
        memset(uid4_out, 0, 4);
        if (nfcid_len > 0) memcpy(uid4_out, nfcid, nfcid_len);
    }
}
