/* See COPYING.txt for license details. */

/**
 * @file   ir_cust_name.c
 * @brief  Pure-logic helpers for custom IR remote file names.
 */

#include <stdint.h>
#include <string.h>
#include "ir_cust_name.h"

void ir_cust_sanitize_name(const char *in, char *out, size_t out_len)
{
    size_t j = 0;

    if (out == NULL || out_len == 0)
        return;

    if (in != NULL)
    {
        size_t i;
        for (i = 0; in[i] != '\0' && j < out_len - 1; i++)
        {
            unsigned char c = (unsigned char)in[i];

            if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' ||
                c == '?'  || c == '"' || c == '<'  || c == '>' || c == '|')
            {
                out[j++] = '_';
            }
            else
            {
                out[j++] = (char)c;
            }
        }
    }
    out[j] = '\0';

    /* Trim trailing spaces and dots (FAT does not like them). */
    while (j > 0 && (out[j - 1] == ' ' || out[j - 1] == '.'))
    {
        out[--j] = '\0';
    }

    if (j == 0)
    {
        strncpy(out, "Remote", out_len - 1);
        out[out_len - 1] = '\0';
    }
}
