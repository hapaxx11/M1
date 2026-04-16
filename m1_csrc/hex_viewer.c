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
    uint16_t pos = 0U;

    if (out_len == 0U)
    {
        return;
    }

    /* Address prefix — 4 hex digits + space */
    pos += (uint16_t)snprintf(out + pos, out_len - pos, "%04lX ",
                              (unsigned long)(row_offset & 0xFFFFUL));

    /* Hex byte pairs */
    for (uint16_t i = 0U; i < len && pos < (uint16_t)(out_len - 3U); i++)
    {
        pos += (uint16_t)snprintf(out + pos, out_len - pos, "%02X", buf[i]);
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
