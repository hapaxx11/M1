/* See COPYING.txt for license details. */

/**
 * @file   nfc_file_parse.h
 * @brief  Pure-logic NFC file body parser — no HAL, RTOS, FatFS, or display deps.
 *
 * Extracted from nfc_storage.c following the Preferred Modularization Pattern
 * (vtable reader approach, same as ir_block_reader_t in ir_signal_record.h).
 *
 * Provides:
 *   - nfc_line_reader_t     — vtable abstracting line-by-line I/O
 *   - nfc_parse_body()      — parse "Page N:" / "Block N:" dump body lines
 *   - nfc_parse_device_type()  — device-type string → tech/family/unit_size
 *   - nfc_parse_hex_bytes() — hex byte string → byte array
 *
 * All functions are testable on the host with an in-memory line reader.
 *
 * M1 Project
 */

#ifndef NFC_FILE_PARSE_H_
#define NFC_FILE_PARSE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Line reader vtable  (same design as ir_block_reader_t)
 * =========================================================================*/

/**
 * @brief  Minimal line reader vtable for nfc_parse_body().
 *
 * Abstracts the line-by-line reader so the parser can operate on any source
 * (FatFS file, in-memory string, UART stream, …) without a direct dependency
 * on nfcfio_t or FatFS.
 *
 * Contract:
 *   1. Call getline() to read the next line into @p buf (max @p bufsz bytes).
 *   2. Returns >=0 for bytes read, -1 for EOF, -2 for error.
 *   3. The returned line may include trailing newline characters.
 *
 * All function pointers must be non-NULL when passed to nfc_parse_body().
 */
typedef struct {
    int (*getline)(void *ctx, char *buf, size_t bufsz);
} nfc_line_reader_t;

/* =========================================================================
 * Parsed family info  (tech / family / unit_size)
 * =========================================================================*/

/**
 * @brief  NFC family info resolved from a device-type string.
 */
typedef struct {
    uint8_t  tech;        /**< M1NFC_TECH_A/B/F/V (same values as nfc_ctx.h) */
    uint8_t  family;      /**< M1NFC_FAM_CLASSIC/ULTRALIGHT/… */
    uint16_t unit_size;   /**< Page/block byte size (Type2=4, Classic=16) */
} nfc_family_info_t;

/* =========================================================================
 * Body-parse result codes  (mirrors nfc_storage_result_t values)
 * =========================================================================*/

/** Result codes for nfc_parse_body(). */
typedef enum {
    NFC_PARSE_OK            = 0,
    NFC_PARSE_ERR_IO        = 1,
    NFC_PARSE_ERR_FORMAT    = 2,
    NFC_PARSE_ERR_NO_BUFFER = 4
} nfc_parse_result_t;

/* =========================================================================
 * Body-parse output metadata
 * =========================================================================*/

/**
 * @brief  Metadata filled by nfc_parse_body() after parsing dump lines.
 */
typedef struct {
    uint16_t unit_size;        /**< Effective unit size used during parsing */
    uint32_t unit_count;       /**< Capacity in units (dump_buf_bytes / unit_size) */
    uint32_t max_seen_unit;    /**< Highest unit index actually stored */
} nfc_parse_body_meta_t;

/* =========================================================================
 * Public API
 * =========================================================================*/

/**
 * Parse a device-type string to tech / family / unit_size.
 *
 * Accepts both M1 format ("Classic", "Ultralight/NTAG") and
 * Flipper format ("Mifare Classic 1K", "NTAG215", "ISO14443-3A", etc.).
 *
 * @param s    Device type string (null-terminated).
 * @param out  Output structure (zeroed on entry by callee).
 * @return 0 on success, -1 if the string is unrecognised.
 */
int nfc_parse_device_type(const char *s, nfc_family_info_t *out);

/**
 * Parse hex byte sequence from a string.
 *
 * Supports "AA BB CC" (space-separated) and "AABBCC" (packed) formats.
 * Whitespace (space, tab, CR, LF) between nibble pairs is silently skipped.
 *
 * @param s        Input string containing hex bytes.
 * @param out      Output byte buffer.
 * @param max      Maximum number of bytes to write to @p out.
 * @param out_len  If non-NULL, receives the number of bytes actually parsed.
 * @return  0 on success,
 *          1 if trailing nibble (odd hex char count),
 *          2 if invalid character encountered,
 *          3 if @p out buffer is full before input is consumed.
 */
int nfc_parse_hex_bytes(const char *s, uint8_t *out, size_t max, size_t *out_len);

/**
 * Parse NFC dump body lines ("Page N: …" / "Block N: …") from a line reader.
 *
 * Reads lines via the vtable until EOF.  Each "Page N:" or "Block N:" line
 * has its hex payload stored at the corresponding offset in @p dump_buf.
 * Comment lines (starting with '#') and blank lines are silently skipped.
 *
 * @param ops             Non-NULL line reader vtable.
 * @param ctx             Opaque context pointer forwarded to vtable calls.
 * @param unit_size       Byte size of each unit (4 for Type2, 16 for Classic).
 * @param dump_buf        Output buffer for unit data.
 * @param dump_buf_bytes  Capacity of @p dump_buf in bytes.
 * @param valid_bits      Validity bitmap (1 bit per unit), or NULL.
 * @param valid_bits_bytes Size of @p valid_bits in bytes.
 * @param meta            Output metadata (filled on success).
 * @return NFC_PARSE_OK on success, error code on failure.
 */
nfc_parse_result_t nfc_parse_body(
        const nfc_line_reader_t *ops,
        void                    *ctx,
        uint16_t                 unit_size,
        uint8_t                 *dump_buf,
        uint32_t                 dump_buf_bytes,
        uint8_t                 *valid_bits,
        uint32_t                 valid_bits_bytes,
        nfc_parse_body_meta_t   *meta);

#ifdef __cplusplus
}
#endif

#endif /* NFC_FILE_PARSE_H_ */
