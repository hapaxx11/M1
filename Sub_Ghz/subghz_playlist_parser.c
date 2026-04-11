/* See COPYING.txt for license details. */

/*
 * subghz_playlist_parser.c
 *
 * Path remapping utility for Sub-GHz playlist files.
 * Extracted from m1_subghz_scene_playlist.c.
 *
 * M1 Project — Hapax fork
 */

#include <string.h>
#include <stdio.h>
#include "subghz_playlist_parser.h"

void subghz_remap_flipper_path(const char *src, char *dst, size_t dlen)
{
    if (!src || !dst || dlen == 0)
        return;

    /* Strip Flipper's /ext/ prefix: /ext/subghz/... → /subghz/... */
    if (strncmp(src, "/ext/", 5) == 0)
        src += 4;  /* skip "/ext" — keep the leading "/" */

    /* Uppercase the directory: /subghz/ → /SUBGHZ/ */
    if (strncmp(src, "/subghz/", 8) == 0)
    {
        snprintf(dst, dlen, "/SUBGHZ/%s", src + 8);
    }
    else
    {
        strncpy(dst, src, dlen - 1);
        dst[dlen - 1] = '\0';
    }
}
