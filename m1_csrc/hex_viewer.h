/* See COPYING.txt for license details. */

/**
 * @file   hex_viewer.h
 * @brief  Hex viewer pure-logic helpers — formatting and ASCII preview.
 *
 * Hardware-independent module.  All functions are pure (no side effects,
 * no HAL/RTOS/FatFS dependencies) and can be tested on the host.
 */

#ifndef HEX_VIEWER_H_
#define HEX_VIEWER_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @brief  Format one hex-dump row: "ADDR XXXXXXXX..."
 *
 * Writes an address prefix (4 hex digits + space) followed by up to
 * @p len contiguous hex byte pairs into @p out (no spaces between bytes).
 * The output is always NUL-terminated.
 *
 * @param buf        Source data bytes for this row.
 * @param len        Number of valid bytes in @p buf (0..row_bytes).
 * @param row_offset Absolute file offset of this row (only low 16 bits shown).
 * @param out        Destination buffer for the formatted string.
 * @param out_len    Size of @p out in bytes (must be > 0).
 */
void hex_viewer_format_row(const uint8_t *buf, uint16_t len,
                           uint32_t row_offset,
                           char *out, size_t out_len);

/**
 * @brief  Build a printable ASCII preview string from raw bytes.
 *
 * Each byte that satisfies isprint() is copied as-is; non-printable bytes
 * are replaced with '.'.  The output is always NUL-terminated.
 *
 * @param buf      Source data bytes.
 * @param len      Number of bytes in @p buf.
 * @param out      Destination buffer for the ASCII string.
 * @param out_len  Size of @p out in bytes (must be > 0).
 */
void hex_viewer_ascii_preview(const uint8_t *buf, uint16_t len,
                              char *out, size_t out_len);

#endif /* HEX_VIEWER_H_ */
