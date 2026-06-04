/* See COPYING.txt for license details. */

/**
 * @file   nfc_classify.c
 * @brief  Pure-logic NFC-A device classification.
 *
 * Extracted from nfc_ctx.c (Phase B blocker removal).
 * Zero HAL/RTOS/display deps — fully testable on the host.
 */

#include "nfc_classify.h"
#include <string.h>

/* =========================================================================
 * nfc_classify_family  (was nfc_classify_family_from_nfca in nfc_ctx.c)
 * =========================================================================*/

uint8_t nfc_classify_family(uint8_t sak, const uint8_t atqa[2])
{
    /* MIFARE Classic variants */
    if (sak == 0x08 || sak == 0x18 || sak == 0x09)
        return NFC_CLASSIFY_FAM_CLASSIC;

    /* Ultralight / NTAG: SAK 00 + ATQA[0] 44 */
    if (sak == 0x00 && atqa && atqa[0] == 0x44)
        return NFC_CLASSIFY_FAM_ULTRALIGHT;

    /* DESFire */
    if (sak == 0x20)
        return NFC_CLASSIFY_FAM_DESFIRE;

    return NFC_CLASSIFY_FAM_CLASSIC; /* default fallback */
}

/* =========================================================================
 * nfc_classify_nfca  (pure-logic core of FillNfcContextFromDevice)
 * =========================================================================*/

void nfc_classify_nfca(const uint8_t *nfcid, uint8_t nfcid_len,
                       uint8_t atqa0, uint8_t atqa1, uint8_t sak,
                       nfc_classify_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));

    result->tech = NFC_CLASSIFY_TECH_A;

    /* UID: clamp to 10 bytes max; zero if nfcid is NULL */
    if (nfcid) {
        uint8_t len = nfcid_len;
        if (len > sizeof(result->uid)) len = (uint8_t)sizeof(result->uid);
        result->uid_len = len;
        if (len > 0) memcpy(result->uid, nfcid, len);
    }

    /* ATQA */
    result->atqa[0]  = atqa0;
    result->atqa[1]  = atqa1;
    result->has_atqa = true;

    /* SAK */
    result->sak     = sak;
    result->has_sak = true;

    /* Family classification */
    result->family = nfc_classify_family(sak, result->atqa);
}
