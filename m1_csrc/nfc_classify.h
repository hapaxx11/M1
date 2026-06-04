/* See COPYING.txt for license details. */

/**
 * @file   nfc_classify.h
 * @brief  Pure-logic NFC-A device classification — no HAL, RTOS, or display deps.
 *
 * Extracted from nfc_ctx.c following the Preferred Modularization Pattern.
 * All functions are testable on the host without any hardware stubs.
 *
 * Provides a hardware-independent classification step that takes primitive
 * NFC-A parameters (SAK, ATQA, UID) and produces a family code plus a
 * populated nfc_classify_result_t, which the caller can then push into
 * nfc_ctx or any other destination.
 *
 * M1 Project
 */

#ifndef NFC_CLASSIFY_H_
#define NFC_CLASSIFY_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Family constants — same values as nfc_ctx.h M1NFC_FAM_* */
#define NFC_CLASSIFY_FAM_CLASSIC      0
#define NFC_CLASSIFY_FAM_ULTRALIGHT   1
#define NFC_CLASSIFY_FAM_DESFIRE      2

/* Tech constants — same values as nfc_ctx.h M1NFC_TECH_* */
#define NFC_CLASSIFY_TECH_A           0

/* =========================================================================
 * Classification result
 * =========================================================================*/

/**
 * @brief  Result of classifying an NFC-A device from its anticollision data.
 *
 * All fields are primitive types — no pointers to hardware structs.
 * The caller is responsible for copying these into nfc_ctx or wherever needed.
 */
typedef struct {
    uint8_t tech;          /**< NFC_CLASSIFY_TECH_A (always 0 for NFC-A) */
    uint8_t family;        /**< NFC_CLASSIFY_FAM_CLASSIC / ULTRALIGHT / DESFIRE */
    uint8_t uid[10];       /**< UID bytes (up to 10) */
    uint8_t uid_len;       /**< Actual UID length */
    uint8_t atqa[2];       /**< ATQA bytes */
    uint8_t sak;           /**< SAK byte */
    bool    has_atqa;      /**< true (always set by classify) */
    bool    has_sak;       /**< true (always set by classify) */
} nfc_classify_result_t;

/* =========================================================================
 * Public API
 * =========================================================================*/

/**
 * Classify an NFC-A card family from SAK and ATQA values.
 *
 * Pure lookup — no side effects.
 *
 * @param sak   SAK (Select Acknowledge) byte.
 * @param atqa  2-byte ATQA array.
 * @return      Family code: NFC_CLASSIFY_FAM_CLASSIC / ULTRALIGHT / DESFIRE,
 *              or 0 (CLASSIC) as default for unrecognised SAK.
 */
uint8_t nfc_classify_family(uint8_t sak, const uint8_t atqa[2]);

/**
 * Build a full classification result from raw NFC-A anticollision data.
 *
 * Populates all fields of @p result: tech, family, uid, atqa, sak.
 * This is the pure-logic core of FillNfcContextFromDevice(), free of
 * rfalNfcDevice struct dependency.
 *
 * @param nfcid      NFCID1 byte array (from card selection).
 * @param nfcid_len  Length of @p nfcid (4, 7, or 10).
 * @param atqa0      ATQA anticollision info byte (sensRes.anticollisionInfo).
 * @param atqa1      ATQA platform info byte (sensRes.platformInfo).
 * @param sak        SAK byte (selRes.sak).
 * @param result     Output — zeroed and populated by this function.
 */
void nfc_classify_nfca(const uint8_t *nfcid, uint8_t nfcid_len,
                       uint8_t atqa0, uint8_t atqa1, uint8_t sak,
                       nfc_classify_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* NFC_CLASSIFY_H_ */
