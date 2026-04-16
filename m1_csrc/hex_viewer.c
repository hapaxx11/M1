/* See COPYING.txt for license details. */

/**
 * @file   hex_viewer.c
 * @brief  Hex viewer pure-logic helpers — formatting and ASCII preview.
 *
 * Hardware-independent module.  Can be compiled on the host for unit testing.
 */

#include "hex_viewer.h"

#include <stdio.h>
#include <ctype.h>

void hex_viewer_format_row(const uint8_t *buf, uint16_t len,
                           uint32_t row_offset,
                           char *out, size_t out_len)
{
    size_t pos = 0U;
    int written;

    if (out_len == 0U)
    {
        return;
    }

    /* Address prefix — 4 hex digits + space */
    written = snprintf(out + pos, out_len - pos, "%04lX ",
                       (unsigned long)(row_offset & 0xFFFFUL));
    if (written < 0 || (size_t)written >= (out_len - pos))
    {
        out[out_len - 1U] = '\0';
        return;
    }
    pos += (size_t)written;

    /* Hex byte pairs — need at least 3 bytes remaining (2 hex digits + NUL) */
    for (uint16_t i = 0U; i < len; i++)
    {
        if ((out_len - pos) < 3U)
        {
            break;
        }
        written = snprintf(out + pos, out_len - pos, "%02X", buf[i]);
        if (written < 0 || (size_t)written >= (out_len - pos))
        {
            break;
        }
        pos += (size_t)written;
    }

    out[out_len - 1U] = '\0';
}

void hex_viewer_ascii_preview(const uint8_t *buf, uint16_t len,
                              char *out, size_t out_len)
{
    uint16_t count;

    if (out_len == 0U)
    {
        return;
    }

    count = (len < (uint16_t)(out_len - 1U)) ? len : (uint16_t)(out_len - 1U);
    for (uint16_t i = 0U; i < count; i++)
    {
        out[i] = isprint((int)buf[i]) ? (char)buf[i] : '.';
    }
    out[count] = '\0';
}
