/* See COPYING.txt for license details. */

/**
 * @file   nfc_ntag.c
 * @brief  Pure-logic NTAG / Ultralight helpers.
 *
 * Extracted from nfc_poller.c (Phase B remaining extraction).
 * Zero HAL/RTOS/FatFS deps — fully testable on the host.
 */

#include "nfc_ntag.h"

/* =========================================================================
 * ntag_page_count_from_size  (was inline in LogParsedNtagVersion)
 * =========================================================================*/

uint16_t ntag_page_count_from_size(uint8_t size_byte)
{
    switch (size_byte) {
    case 0x0F: return 45U;   /* NTAG213 — 144 B user memory */
    case 0x11: return 135U;  /* NTAG215 — 504 B user memory */
    case 0x13: return 231U;  /* NTAG216 — 888 B user memory */
    default:   return 0U;
    }
}

/* =========================================================================
 * ntag_generate_amiibo_pwd  (was static in nfc_poller.c)
 * =========================================================================*/

bool ntag_generate_amiibo_pwd(const uint8_t *uid, uint8_t uid_len,
                               uint8_t pwd_out[4], uint8_t pack_out[2])
{
    if (!uid || uid_len != 7 || !pwd_out || !pack_out)
        return false;

    pwd_out[0] = 0xAAu ^ uid[1] ^ uid[3];
    pwd_out[1] = 0x55u ^ uid[2] ^ uid[4];
    pwd_out[2] = 0xAAu ^ uid[3] ^ uid[5];
    pwd_out[3] = 0x55u ^ uid[4] ^ uid[6];

    pack_out[0] = 0x80u;
    pack_out[1] = 0x80u;

    return true;
}
