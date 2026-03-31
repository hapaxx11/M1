/* See COPYING.txt for license details. */

/*
 * common_defines.h — Flipper/Momentum common macro definitions for M1.
 *
 * Provides the standard utility macros used throughout Flipper firmware.
 * These are referenced by protocol code and library headers alike.
 *
 * M1 Project — Hapax fork
 */

#ifndef FURI_COMMON_DEFINES_H
#define FURI_COMMON_DEFINES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Standard utility macros                                                    */
/*============================================================================*/

#ifndef MAX
#define MAX(a, b)                                           \
    ({                                                      \
        __typeof__(a) _a = (a);                             \
        __typeof__(b) _b = (b);                             \
        _a > _b ? _a : _b;                                  \
    })
#endif

#ifndef MIN
#define MIN(a, b)                                           \
    ({                                                      \
        __typeof__(a) _a = (a);                             \
        __typeof__(b) _b = (b);                             \
        _a < _b ? _a : _b;                                  \
    })
#endif

#ifndef CLAMP
#define CLAMP(x, upper, lower)                              \
    ({                                                      \
        __typeof__(x) _x = (x);                             \
        __typeof__(upper) _u = (upper);                     \
        __typeof__(lower) _l = (lower);                     \
        (_x > _u) ? _u : ((_x < _l) ? _l : _x);           \
    })
#endif

#ifndef COUNT_OF
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifndef STRINGIFY
#define STRINGIFY(x)       #x
#define STRINGIFY_VALUE(x) STRINGIFY(x)
#endif

/** Round up a value to the nearest multiple of alignment. */
#ifndef ALIGN_UP
#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#endif

/** Round down a value to the nearest multiple of alignment. */
#ifndef ALIGN_DOWN
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#endif

/** Byte swap macros (if not provided by platform) */
#ifndef FURI_SWAP_UINT16
#define FURI_SWAP_UINT16(x) __builtin_bswap16(x)
#endif

#ifndef FURI_SWAP_UINT32
#define FURI_SWAP_UINT32(x) __builtin_bswap32(x)
#endif

/*============================================================================*/
/* Compiler hints                                                             */
/*============================================================================*/

#ifndef FURI_WARN_UNUSED
#define FURI_WARN_UNUSED __attribute__((warn_unused_result))
#endif

#ifndef FURI_WEAK
#define FURI_WEAK __attribute__((weak))
#endif

#ifndef FURI_PACKED
#define FURI_PACKED __attribute__((packed))
#endif

#ifndef FURI_NORETURN
#define FURI_NORETURN __attribute__((noreturn))
#endif

#ifdef __cplusplus
}
#endif

#endif /* FURI_COMMON_DEFINES_H */
