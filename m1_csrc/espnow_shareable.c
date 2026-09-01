/* See COPYING.txt for license details. */

/**
 * @file   espnow_shareable.c
 * @brief  Capture-sharing helpers for the ESP-NOW sender side — pure logic.
 *
 * See espnow_shareable.h.
 *
 * M1 Project
 */

#include "espnow_shareable.h"

#include <string.h>

/* Lower-case an ASCII byte (avoids locale/ctype in host tests). */
static char to_lower_ascii(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* Return a pointer to the extension (after the last '.') of the basename, or
 * NULL if there is no extension.  Ignores dots in directory components. */
static const char *find_extension(const char *name)
{
    const char *base = name;
    const char *ext  = NULL;
    for (const char *p = name; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
            ext  = NULL;              /* reset — dots before base don't count */
        } else if (*p == '.') {
            ext = p + 1;
        }
    }
    /* A leading-dot basename ("/.foo") has no real extension. */
    if (ext != NULL && ext == base + 1 && base[0] == '.')
        return NULL;
    return ext;
}

/* Case-insensitive compare of @p a against lower-case literal @p lit. */
static bool ext_equals(const char *a, const char *lit)
{
    size_t i = 0;
    for (; a[i] && lit[i]; ++i) {
        if (to_lower_ascii(a[i]) != lit[i])
            return false;
    }
    return a[i] == '\0' && lit[i] == '\0';
}

espnow_share_kind_t espnow_share_classify(const char *name)
{
    if (name == NULL)
        return ESPNOW_SHARE_KIND_UNKNOWN;

    const char *ext = find_extension(name);
    if (ext == NULL || *ext == '\0')
        return ESPNOW_SHARE_KIND_UNKNOWN;

    if (ext_equals(ext, "sub"))
        return ESPNOW_SHARE_KIND_SUBGHZ;
    if (ext_equals(ext, "nfc"))
        return ESPNOW_SHARE_KIND_NFC;
    if (ext_equals(ext, "rfid"))
        return ESPNOW_SHARE_KIND_RFID;
    if (ext_equals(ext, "ir"))
        return ESPNOW_SHARE_KIND_IR;

    return ESPNOW_SHARE_KIND_UNKNOWN;
}

bool espnow_share_is_shareable(const char *name)
{
    return espnow_share_classify(name) != ESPNOW_SHARE_KIND_UNKNOWN;
}

bool espnow_share_basename(const char *path, char *out, size_t out_cap)
{
    if (path == NULL || out == NULL || out_cap == 0u)
        return false;

    const char *base = path;
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }

    size_t i = 0;
    for (; base[i] && i < out_cap - 1u; ++i)
        out[i] = base[i];
    out[i] = '\0';
    return true;
}

bool espnow_share_name_is_safe(const char *name, size_t max_len)
{
    if (name == NULL)
        return false;

    size_t len = strlen(name);
    if (len == 0u || len > max_len)
        return false;

    /* No hidden / relative names. */
    if (name[0] == '.')
        return false;

    for (size_t i = 0; i < len; ++i) {
        char c = name[i];
        if (c == '/' || c == '\\')
            return false;                       /* no path separators */
        if (c == '.' && i + 1u < len && name[i + 1u] == '.')
            return false;                       /* no ".." traversal */
    }
    return true;
}

bool espnow_share_recv_path(const char *name, char *out, size_t out_cap)
{
    if (name == NULL || out == NULL || out_cap == 0u)
        return false;

    static const char dir[] = ESPNOW_SHARE_RECV_DIR "/";
    size_t dir_len  = sizeof(dir) - 1u;         /* excludes NUL */
    size_t name_len = strlen(name);

    if (dir_len + name_len + 1u > out_cap)
        return false;

    memcpy(out, dir, dir_len);
    memcpy(out + dir_len, name, name_len);
    out[dir_len + name_len] = '\0';
    return true;
}
