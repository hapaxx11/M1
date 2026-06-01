/* See COPYING.txt for license details. */

/**
 * @file   nfc_ndef_parse.h
 * @brief  Pure-logic NDEF message parser — no HAL, RTOS, FatFS, or display deps.
 *
 * Parses NDEF TLV records (URI and Text) from raw T2T tag memory into
 * human-readable text.  Complement to `nfc_ndef_encode.h`.
 *
 * Covers:
 *   - ndef_parse_records()  — multi-record NDEF → newline-separated text
 *
 * M1 Project
 */

#ifndef NFC_NDEF_PARSE_H_
#define NFC_NDEF_PARSE_H_

#include <stdint.h>

/**
 * @brief  Parse NDEF records (URI/Text) into human-readable text.
 *
 * Decodes an NDEF message containing URI and/or Text records.
 * Each decoded record is appended as a line; lines are separated by '\n'.
 * The trailing newline (if any) is removed before returning.
 *
 * URI records use the NFC Forum URI RTD prefix table (36 entries)
 * to expand compressed prefixes.  Text records skip the language-code
 * header and output the UTF-8 payload.
 *
 * @param ndef_msg   Pointer to the raw NDEF message bytes (starting at
 *                   the first record header byte, NOT the TLV type byte).
 * @param ndef_len   Length of the NDEF message in bytes.
 * @param out        Output buffer for the decoded text.
 * @param out_size   Size of @p out in bytes (including null terminator).
 *                   Must be >= 1.
 * @return           Number of characters written (excluding null terminator),
 *                   or 0 if no records were decoded or inputs are invalid.
 */
uint16_t ndef_parse_records(const uint8_t *ndef_msg, uint16_t ndef_len,
                            char *out, uint16_t out_size);

#endif /* NFC_NDEF_PARSE_H_ */
