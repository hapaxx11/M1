/* See COPYING.txt for license details. */

/*
 * string.c — FuriString implementation for M1.
 *
 * Lightweight dynamic string backed by heap-allocated char buffer.
 * Uses m1_malloc/m1_free for allocation so it participates in M1's
 * critical-section-protected heap.
 *
 * M1 Project — Hapax fork
 */

#include "string.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "m1_core_config.h"

/*============================================================================*/
/* Internal representation                                                    */
/*============================================================================*/

#define FURI_STRING_INITIAL_CAPACITY 64u

struct FuriString {
    char*  data;      /**< NUL-terminated character buffer */
    size_t length;    /**< Current string length (excluding NUL) */
    size_t capacity;  /**< Allocated buffer size (including NUL) */
};

/*============================================================================*/
/* Internal helpers                                                           */
/*============================================================================*/

/** Ensure the buffer has room for at least `needed` total chars + NUL. */
static void furi_string_ensure_capacity(FuriString* s, size_t needed)
{
    size_t required = needed + 1; /* +1 for NUL terminator */
    if (required <= s->capacity) {
        return;
    }
    /* Grow by doubling until sufficient */
    size_t new_cap = s->capacity;
    if (new_cap == 0) {
        new_cap = FURI_STRING_INITIAL_CAPACITY;
    }
    while (new_cap < required) {
        new_cap *= 2;
    }
    char* new_data = (char*)m1_malloc(new_cap);
    if (!new_data) {
        return; /* allocation failure — embedded target, avoid crash */
    }
    if (s->data) {
        memcpy(new_data, s->data, s->length + 1);
        m1_free(s->data);
    } else {
        new_data[0] = '\0';
    }
    s->data     = new_data;
    s->capacity = new_cap;
}

/** Internal vprintf helper — appends formatted text at s->length. */
static int furi_string_vprintf_at(FuriString* s, size_t offset, const char* format, va_list args)
{
    /* First pass: measure required length */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        return needed;
    }

    furi_string_ensure_capacity(s, offset + (size_t)needed);
    if (!s->data) {
        return -1;
    }

    int written = vsnprintf(s->data + offset, s->capacity - offset, format, args);
    if (written > 0) {
        s->length = offset + (size_t)written;
    }
    return written;
}

/*============================================================================*/
/* Lifecycle                                                                  */
/*============================================================================*/

FuriString* furi_string_alloc(void)
{
    FuriString* s = (FuriString*)m1_malloc(sizeof(FuriString));
    if (!s) {
        return NULL;
    }
    s->data     = NULL;
    s->length   = 0;
    s->capacity = 0;
    furi_string_ensure_capacity(s, 0);
    return s;
}

FuriString* furi_string_alloc_printf(const char* format, ...)
{
    FuriString* s = furi_string_alloc();
    if (!s) {
        return NULL;
    }
    va_list args;
    va_start(args, format);
    furi_string_vprintf_at(s, 0, format, args);
    va_end(args);
    return s;
}

FuriString* furi_string_alloc_set(const FuriString* src)
{
    FuriString* s = furi_string_alloc();
    if (!s) {
        return NULL;
    }
    if (src && src->data) {
        furi_string_ensure_capacity(s, src->length);
        if (s->data) {
            memcpy(s->data, src->data, src->length + 1);
            s->length = src->length;
        }
    }
    return s;
}

FuriString* furi_string_alloc_set_str(const char* str)
{
    FuriString* s = furi_string_alloc();
    if (!s) {
        return NULL;
    }
    if (str) {
        size_t len = strlen(str);
        furi_string_ensure_capacity(s, len);
        if (s->data) {
            memcpy(s->data, str, len + 1);
            s->length = len;
        }
    }
    return s;
}

void furi_string_free(FuriString* s)
{
    if (!s) {
        return;
    }
    if (s->data) {
        m1_free(s->data);
    }
    m1_free(s);
}

/*============================================================================*/
/* Modification                                                               */
/*============================================================================*/

void furi_string_reset(FuriString* s)
{
    if (!s) {
        return;
    }
    s->length = 0;
    if (s->data) {
        s->data[0] = '\0';
    }
}

int furi_string_printf(FuriString* s, const char* format, ...)
{
    if (!s) {
        return -1;
    }
    va_list args;
    va_start(args, format);
    int ret = furi_string_vprintf_at(s, 0, format, args);
    va_end(args);
    return ret;
}

int furi_string_cat_printf(FuriString* s, const char* format, ...)
{
    if (!s) {
        return -1;
    }
    va_list args;
    va_start(args, format);
    int ret = furi_string_vprintf_at(s, s->length, format, args);
    va_end(args);
    return ret;
}

void furi_string_cat(FuriString* s, const FuriString* other)
{
    if (!s || !other || !other->data || other->length == 0) {
        return;
    }
    furi_string_ensure_capacity(s, s->length + other->length);
    if (s->data) {
        memcpy(s->data + s->length, other->data, other->length + 1);
        s->length += other->length;
    }
}

void furi_string_cat_str(FuriString* s, const char* str)
{
    if (!s || !str) {
        return;
    }
    size_t str_len = strlen(str);
    if (str_len == 0) {
        return;
    }
    furi_string_ensure_capacity(s, s->length + str_len);
    if (s->data) {
        memcpy(s->data + s->length, str, str_len + 1);
        s->length += str_len;
    }
}

void furi_string_set(FuriString* s, const FuriString* src)
{
    if (!s) {
        return;
    }
    if (!src || !src->data) {
        furi_string_reset(s);
        return;
    }
    furi_string_ensure_capacity(s, src->length);
    if (s->data) {
        memcpy(s->data, src->data, src->length + 1);
        s->length = src->length;
    }
}

void furi_string_set_str(FuriString* s, const char* str)
{
    if (!s) {
        return;
    }
    if (!str) {
        furi_string_reset(s);
        return;
    }
    size_t len = strlen(str);
    furi_string_ensure_capacity(s, len);
    if (s->data) {
        memcpy(s->data, str, len + 1);
        s->length = len;
    }
}

/*============================================================================*/
/* Access                                                                     */
/*============================================================================*/

const char* furi_string_get_cstr(const FuriString* s)
{
    if (!s || !s->data) {
        return "";
    }
    return s->data;
}

size_t furi_string_size(const FuriString* s)
{
    if (!s) {
        return 0;
    }
    return s->length;
}

bool furi_string_empty(const FuriString* s)
{
    return (!s || s->length == 0);
}

/*============================================================================*/
/* Comparison                                                                 */
/*============================================================================*/

int furi_string_cmp_str(const FuriString* s, const char* str)
{
    const char* a = furi_string_get_cstr(s);
    if (!str) {
        str = "";
    }
    return strcmp(a, str);
}

bool furi_string_equal_str(const FuriString* s, const char* str)
{
    return furi_string_cmp_str(s, str) == 0;
}

bool furi_string_start_with_str(const FuriString* s, const char* prefix)
{
    if (!s || !prefix) {
        return false;
    }
    size_t prefix_len = strlen(prefix);
    if (prefix_len > s->length) {
        return false;
    }
    return (s->data && memcmp(s->data, prefix, prefix_len) == 0);
}

int furi_string_cmp(const FuriString* a, const FuriString* b)
{
    return strcmp(furi_string_get_cstr(a), furi_string_get_cstr(b));
}

bool furi_string_equal(const FuriString* a, const FuriString* b)
{
    return furi_string_cmp(a, b) == 0;
}
