/* See COPYING.txt for license details. */

/*
 * nfc_ndef_encode.h
 *
 * Pure-logic NDEF (NFC Data Exchange Format) encoder for M1.
 *
 * Builds NDEF TLV payloads suitable for writing to Type 2 Tags
 * (NTAG213/215/216).  Hardware-independent — no FatFS, no RTOS,
 * no display.
 *
 * Supports:
 *  - URI records (with standard URI prefix compression)
 *  - Text records (UTF-8, with language code)
 *  - WiFi Simple Config records (SSID + password)
 *  - Phone number records (tel: URI)
 *  - vCard (basic: name + phone)
 *
 * M1 Project
 */

#ifndef NFC_NDEF_ENCODE_H_
#define NFC_NDEF_ENCODE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief  Maximum NDEF message size (fits in NTAG216 user area).
 *
 * NTAG216 has 888 bytes of user memory, minus 4 bytes for CC.
 * The TLV wrapper adds 2 bytes (type + length for short records,
 * or 4 bytes for >254 payload), plus the Terminator TLV (1 byte).
 */
#define NDEF_MAX_MESSAGE_SIZE  872

/**
 * @brief  Standard URI identifier codes (NFC Forum URI RTD).
 *
 * These compress common URI prefixes into a single byte at the start
 * of the payload, saving space on small tags.
 */
typedef enum {
    NDEF_URI_NONE           = 0x00,  /**< No prepending — full URI in payload */
    NDEF_URI_HTTP_WWW       = 0x01,  /**< http://www. */
    NDEF_URI_HTTPS_WWW      = 0x02,  /**< https://www. */
    NDEF_URI_HTTP           = 0x03,  /**< http:// */
    NDEF_URI_HTTPS          = 0x04,  /**< https:// */
    NDEF_URI_TEL            = 0x05,  /**< tel: */
    NDEF_URI_MAILTO         = 0x06,  /**< mailto: */
    /* Entries 0x07–0x23 exist but are rarely used; add as needed */
} ndef_uri_code_t;

/**
 * @brief  Build a URI NDEF record wrapped in a TLV for T2T.
 *
 * Output format (short record, single message):
 *   [0x03][Len][0xD1][0x01][PL_len]['U'][uri_code][uri_data...][0xFE]
 *
 * @param out       Output buffer (must be at least NDEF_MAX_MESSAGE_SIZE)
 * @param out_size  Size of output buffer
 * @param uri_code  URI prefix code (see ndef_uri_code_t)
 * @param uri       URI string WITHOUT the prefix (e.g. "example.com")
 * @return          Total bytes written (including TLV + terminator), or 0 on error
 */
size_t ndef_encode_uri(uint8_t *out, size_t out_size,
                       ndef_uri_code_t uri_code, const char *uri);

/**
 * @brief  Auto-detect URI prefix and build a URI NDEF record.
 *
 * Automatically strips known prefixes (http://www., https://www.,
 * http://, https://, tel:, mailto:) and uses the corresponding
 * uri_code for compression.
 *
 * @param out       Output buffer
 * @param out_size  Size of output buffer
 * @param full_uri  Full URI string (e.g. "https://example.com")
 * @return          Total bytes written, or 0 on error
 */
size_t ndef_encode_uri_auto(uint8_t *out, size_t out_size, const char *full_uri);

/**
 * @brief  Build a Text NDEF record wrapped in a TLV for T2T.
 *
 * Text record payload:
 *   [status_byte][lang_code...][text...]
 *   status_byte = (UTF-8 << 7) | lang_code_len
 *
 * @param out       Output buffer
 * @param out_size  Size of output buffer
 * @param lang      Language code (e.g. "en", "fr").  Max 6 chars.
 * @param text      Text content (UTF-8)
 * @return          Total bytes written, or 0 on error
 */
size_t ndef_encode_text(uint8_t *out, size_t out_size,
                        const char *lang, const char *text);

/**
 * @brief  Build a WiFi Simple Config NDEF record for T2T.
 *
 * Encodes SSID + password as a WiFi credential that Android/iOS can
 * read to auto-connect.
 *
 * @param out       Output buffer
 * @param out_size  Size of output buffer
 * @param ssid      WiFi SSID
 * @param password  WiFi password (or NULL for open network)
 * @param auth_wpa2 true = WPA2-Personal, false = Open
 * @return          Total bytes written, or 0 on error
 */
size_t ndef_encode_wifi(uint8_t *out, size_t out_size,
                        const char *ssid, const char *password,
                        bool auth_wpa2);

/**
 * @brief  Build a phone number URI NDEF record for T2T.
 *
 * Convenience wrapper around ndef_encode_uri() with NDEF_URI_TEL.
 *
 * @param out       Output buffer
 * @param out_size  Size of output buffer
 * @param phone     Phone number string (e.g. "+1234567890")
 * @return          Total bytes written, or 0 on error
 */
size_t ndef_encode_phone(uint8_t *out, size_t out_size, const char *phone);

/**
 * @brief  Calculate number of T2T pages needed to write the NDEF data.
 *
 * Each T2T page is 4 bytes.  NDEF data starts at page 4 on NTAG tags.
 *
 * @param ndef_size  Total NDEF TLV size (as returned by ndef_encode_*)
 * @return           Number of pages needed (starting at page 4)
 */
static inline uint16_t ndef_pages_needed(size_t ndef_size)
{
    return (uint16_t)((ndef_size + 3) / 4);
}

#endif /* NFC_NDEF_ENCODE_H_ */
