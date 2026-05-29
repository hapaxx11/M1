/* See COPYING.txt for license details. */

/**
 * @file   nfc_card_info.h
 * @brief  Pure-logic NFC card classification helpers — no HAL, RTOS, or display deps.
 *
 * Extracted from m1_nfc.c following the Preferred Modularization Pattern.
 * All functions are testable on the host via the stub-based extraction pattern.
 *
 * Covers:
 *   - nfc_manufacturer_name()  — UID byte 0 → ISO/IEC 7816-6 manufacturer string
 *   - nfc_sak_type_str()       — SAK + ATQA → NFC-A card-type string
 *   - nfc_uid_fmt()            — raw UID bytes → "AA BB CC …" hex string
 *   - nfc_uid_step()           — little-endian UID increment / decrement (fuzzer)
 *
 * M1 Project
 */

#ifndef NFC_CARD_INFO_H_
#define NFC_CARD_INFO_H_

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * Manufacturer lookup
 * =========================================================================*/

/**
 * Map the first byte of an NFC-A UID to a manufacturer name string.
 *
 * Based on ISO/IEC 7816-6 / JIS X 6319-4 registered manufacturer IDs.
 * Unrecognised values return "Unknown".
 *
 * @param mfr_byte  Byte 0 of the card UID.
 * @return          Constant string — never NULL.
 */
const char *nfc_manufacturer_name(uint8_t mfr_byte);

/* =========================================================================
 * NFC-A card-type classification
 * =========================================================================*/

/**
 * Return a human-readable NFC-A card-type string from SAK and ATQA.
 *
 * Covers the most common ISO 14443-3A types:
 *   MIFARE Classic 1K/4K/Mini/2K, MIFARE Plus,
 *   Ultralight / NTAG, DESFire / ISO-DEP, and compound types.
 * Returns "Other" for unrecognised combinations.
 *
 * @param sak   SAK (Select Acknowledge) byte.
 * @param atqa  2-byte ATQA (Answer To Request, Type A).
 * @return      Constant string — never NULL.
 */
const char *nfc_sak_type_str(uint8_t sak, const uint8_t atqa[2]);

/* =========================================================================
 * UID formatting
 * =========================================================================*/

/**
 * Format raw UID bytes as an upper-case space-separated hex string.
 *
 * Example: {0x04, 0xB7, 0x28} → "04 B7 28"
 *
 * Always null-terminates @p out when @p out_size > 0.
 * If @p out_size is too small the output is silently truncated.
 *
 * @param uid       Raw UID byte array.
 * @param uid_len   Number of valid bytes in @p uid (0–10).
 * @param out       Destination buffer.
 * @param out_size  Size of @p out in bytes including the null terminator.
 *                  Minimum useful size: uid_len * 3 (e.g. 31 for 10-byte UID).
 */
void nfc_uid_fmt(const uint8_t *uid, uint8_t uid_len,
                 char *out, size_t out_size);

/* =========================================================================
 * UID arithmetic (fuzzer helper)
 * =========================================================================*/

/**
 * Increment or decrement a UID value by one, least-significant byte first.
 *
 * Treats the UID as a big-endian integer and propagates carry/borrow
 * from the last byte toward byte 0.  Wraps silently at 0x00…0xFF limits.
 *
 * @param uid      UID byte array (modified in-place).
 * @param uid_len  Number of bytes in @p uid.
 * @param dir      +1 to increment, −1 to decrement, 0 is a no-op.
 */
void nfc_uid_step(uint8_t *uid, uint8_t uid_len, int8_t dir);

#endif /* NFC_CARD_INFO_H_ */
