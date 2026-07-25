/* See COPYING.txt for license details. */

/*
 * esp32_spi_bin.h
 *
 * Binary-safe copy helper for the ESP32 SPI-HD transport.
 *
 * The legacy AT-over-SPI receive path delivered every slave response as a
 * NUL-terminated C string (it used strcpy()/strlen()).  That is correct for
 * ESP-AT text responses, but it silently truncates *binary* frames — most
 * importantly the CD3 native M1_RPC frames (magic 0x4D31), whose 8-byte header
 * and little-endian fields routinely contain 0x00 bytes.  A strcpy() of such a
 * frame stops at the first NUL, corrupting the response and causing CD3 to be
 * misdetected as "Unknown (fallback)".
 *
 * esp32_spi_bin_copy() copies an explicit byte count (never scanning for a NUL
 * terminator), so embedded 0x00 bytes are preserved.  It is defined here as a
 * dependency-free static-inline so both the firmware transport and the host
 * unit tests share the exact same logic.
 *
 * M1 Project
 */

#ifndef ESP32_SPI_BIN_H_
#define ESP32_SPI_BIN_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief  Binary-safe copy of a received SPI-HD payload into a caller buffer.
 *
 * Copies exactly min(src_len, dst_cap) bytes from @p src to @p dst using a
 * length-based memcpy, so any embedded NUL bytes are preserved (unlike the
 * strcpy()/strlen() path this replaces).  No terminator is appended — the
 * caller owns the buffer and decides whether a trailing NUL is needed.
 *
 * @param  dst      Destination buffer (may be NULL only if @p dst_cap is 0)
 * @param  dst_cap  Capacity of @p dst in bytes
 * @param  src      Source buffer (may be NULL only if @p src_len is 0)
 * @param  src_len  Number of valid bytes in @p src
 * @return Number of bytes actually copied (min of @p src_len and @p dst_cap).
 */
static inline size_t esp32_spi_bin_copy(uint8_t *dst, size_t dst_cap,
                                        const uint8_t *src, size_t src_len)
{
    size_t n = (src_len < dst_cap) ? src_len : dst_cap;

    if (n == 0u || dst == NULL || src == NULL)
        return 0u;

    memcpy(dst, src, n);
    return n;
}

#endif /* ESP32_SPI_BIN_H_ */
