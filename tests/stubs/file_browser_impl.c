/* See COPYING.txt for license details. */

/*
 * file_browser_impl.c — standalone copy of pure-logic file browser functions
 *                        for host-side testing.
 *
 * The production code lives in m1_file_browser.c, which has heavy
 * HAL/u8g2/FatFS dependencies.  This file mirrors the pure string
 * manipulation functions so they can be tested on the host.
 *
 * Keep in sync with m1_file_browser.c (fb_char_lower, fb_entry_cmp,
 * m1_fb_find_ext, m1_fb_get_file_type, m1_fb_dyn_strcat).
 */

#include "file_browser_impl.h"
#include <string.h>
#include <stdarg.h>
#include <assert.h>

/* Mirrors m1_file_browser.c::m1_fb_data_types */
const char fb_data_types[] = ".log.LOG.text.TEXT.txt.TXT";

int fb_char_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

int fb_entry_cmp(const void *a, const void *b)
{
    const fb_sorted_entry_t *ea = (const fb_sorted_entry_t *)a;
    const fb_sorted_entry_t *eb = (const fb_sorted_entry_t *)b;

    int a_dir = (ea->fattrib & FB_AM_DIR) ? 1 : 0;
    int b_dir = (eb->fattrib & FB_AM_DIR) ? 1 : 0;

    /* Directories come before files */
    if (a_dir != b_dir)
        return b_dir - a_dir;  /* dir (1) sorts before file (0) */

    /* Alphabetical within the same type (case-insensitive) */
    const char *pa = ea->fname;
    const char *pb = eb->fname;
    while (*pa && *pb)
    {
        int ca = fb_char_lower((unsigned char)*pa++);
        int cb = fb_char_lower((unsigned char)*pb++);
        if (ca != cb)
            return ca - cb;
    }
    return (unsigned char)*pa - (unsigned char)*pb;
}

uint8_t fb_find_ext(char *file_name, const char *file_ext)
{
    int k, f;

    f = 0;
    k = strlen(file_name);
    while (k)
    {
        if (file_name[k - 1] == '.')
        {
            f = 1;
            break;
        }
        k--;
    }
    if (!f || k == 1)
        return 0;
    if (strstr(file_ext, &file_name[k - 1]))
        return 1;

    return 0;
}

fb_file_ext_t fb_get_file_type(char *filename)
{
    if (fb_find_ext(filename, fb_data_types))
        return FB_F_EXT_DATA;

    return FB_F_EXT_OTHER;
}

uint8_t fb_dyn_strcat(char *buffer, uint8_t num, const char *format, ...)
{
    va_list pargs;
    uint8_t len, k;
    char *tmp_buffer;

    assert(num >= 1);

    va_start(pargs, format);
    len = 0;
    k = num;
    while (k)
    {
        tmp_buffer = va_arg(pargs, char *);
        len += strlen(tmp_buffer);
        k--;
    }

    va_end(pargs);

    if (num > 1)
        len += num - 1;

    if (!len)
        return 0;

    va_start(pargs, format);
    k = num;
    strcpy(buffer, "");
    while (k)
    {
        tmp_buffer = va_arg(pargs, char *);
        if (strlen(tmp_buffer))
        {
            strcat(buffer, tmp_buffer);
            if (k != 1)
                strcat(buffer, "/");
        }
        k--;
    }
    va_end(pargs);

    return strlen(buffer);
}
