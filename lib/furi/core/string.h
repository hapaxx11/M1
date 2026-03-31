/* See COPYING.txt for license details. */

/*
 * string.h — Minimal FuriString compatibility shim for M1.
 *
 * Provides a lightweight dynamic string type that mirrors the subset of
 * Momentum/Flipper's FuriString API actually used by Sub-GHz protocol
 * decoders.  This is NOT a full M*LIB reimplementation — only the
 * functions that protocol code calls are implemented.
 *
 * Supported API surface:
 *   furi_string_alloc()          — allocate empty string
 *   furi_string_alloc_printf()   — allocate + format
 *   furi_string_free()           — deallocate
 *   furi_string_reset()          — clear contents (keep allocation)
 *   furi_string_get_cstr()       — get const char* pointer
 *   furi_string_cat(a, b)        — append FuriString b to a
 *   furi_string_cat_str(a, s)    — append C-string s to a
 *   furi_string_cat_printf()     — append formatted text
 *   furi_string_printf()         — overwrite with formatted text
 *   furi_string_set(a, b)        — copy FuriString b into a
 *   furi_string_set_str(a, s)    — copy C-string s into a
 *   furi_string_size()           — current length
 *   furi_string_empty()          — test if empty
 *   furi_string_cmp_str()        — compare with C-string
 *   furi_string_equal_str()      — equality check with C-string
 *   furi_string_start_with_str() — prefix check
 *
 * Internal storage: heap-allocated char buffer with length + capacity
 * tracking.  Initial capacity is 64 bytes; grows by doubling.
 *
 * M1 Project — Hapax fork
 */

#ifndef FURI_STRING_H
#define FURI_STRING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Opaque type                                                                */
/*============================================================================*/

typedef struct FuriString FuriString;

/*============================================================================*/
/* Lifecycle                                                                  */
/*============================================================================*/

/** Allocate a new empty FuriString. */
FuriString* furi_string_alloc(void);

/** Allocate and initialise with printf-formatted content. */
FuriString* furi_string_alloc_printf(const char* format, ...)
    __attribute__((__format__(__printf__, 1, 2)));

/** Allocate and initialise by copying an existing FuriString. */
FuriString* furi_string_alloc_set(const FuriString* src);

/** Allocate and initialise by copying a C-string. */
FuriString* furi_string_alloc_set_str(const char* str);

/** Free a FuriString (NULL-safe). */
void furi_string_free(FuriString* s);

/*============================================================================*/
/* Modification                                                               */
/*============================================================================*/

/** Clear contents to empty string (keeps allocation). */
void furi_string_reset(FuriString* s);

/** Overwrite contents with printf-formatted text. */
int furi_string_printf(FuriString* s, const char* format, ...)
    __attribute__((__format__(__printf__, 2, 3)));

/** Append printf-formatted text to existing contents. */
int furi_string_cat_printf(FuriString* s, const char* format, ...)
    __attribute__((__format__(__printf__, 2, 3)));

/** Append another FuriString. */
void furi_string_cat(FuriString* s, const FuriString* other);

/** Append a C-string. */
void furi_string_cat_str(FuriString* s, const char* str);

/** Copy contents of another FuriString into this one. */
void furi_string_set(FuriString* s, const FuriString* src);

/** Copy a C-string into this FuriString. */
void furi_string_set_str(FuriString* s, const char* str);

/*============================================================================*/
/* Access                                                                     */
/*============================================================================*/

/** Get read-only C-string pointer.  Valid until next mutation. */
const char* furi_string_get_cstr(const FuriString* s);

/** Get current string length (excluding NUL terminator). */
size_t furi_string_size(const FuriString* s);

/** Test whether the string is empty. */
bool furi_string_empty(const FuriString* s);

/*============================================================================*/
/* Comparison                                                                 */
/*============================================================================*/

/** Compare with a C-string (strcmp semantics: <0, 0, >0). */
int furi_string_cmp_str(const FuriString* s, const char* str);

/** Equality check with a C-string. */
bool furi_string_equal_str(const FuriString* s, const char* str);

/** Test whether the string starts with the given C-string prefix. */
bool furi_string_start_with_str(const FuriString* s, const char* prefix);

/** Compare two FuriStrings (strcmp semantics). */
int furi_string_cmp(const FuriString* a, const FuriString* b);

/** Equality check for two FuriStrings. */
bool furi_string_equal(const FuriString* a, const FuriString* b);

#ifdef __cplusplus
}
#endif

#endif /* FURI_STRING_H */
