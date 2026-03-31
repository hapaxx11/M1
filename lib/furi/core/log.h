/* See COPYING.txt for license details. */

/*
 * log.h — Furi logging compatibility shim for M1.
 *
 * Maps Momentum/Flipper's FURI_LOG_* macros to M1's existing
 * m1_logdb_printf() logging infrastructure.  This allows ported
 * protocol code that uses FURI_LOG_E/W/I/D/T to compile without
 * changes.
 *
 * Flipper's logging API:
 *   FURI_LOG_E(tag, fmt, ...)  — error
 *   FURI_LOG_W(tag, fmt, ...)  — warning
 *   FURI_LOG_I(tag, fmt, ...)  — info
 *   FURI_LOG_D(tag, fmt, ...)  — debug
 *   FURI_LOG_T(tag, fmt, ...)  — trace
 *
 * M1's logging API:
 *   M1_LOG_E(tag, fmt, ...)    — error
 *   M1_LOG_W(tag, fmt, ...)    — warning
 *   M1_LOG_I(tag, fmt, ...)    — info
 *   M1_LOG_D(tag, fmt, ...)    — debug
 *   M1_LOG_T(tag, fmt, ...)    — trace
 *
 * The signatures are identical (tag, format, ...) so this is a direct
 * macro alias.
 *
 * M1 Project — Hapax fork
 */

#ifndef FURI_LOG_H
#define FURI_LOG_H

#include "m1_log_debug_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Log level enum — matches Flipper's FuriLogLevel                            */
/*============================================================================*/

typedef enum {
    FuriLogLevelDefault = 0, /**< Use default log level */
    FuriLogLevelNone    = 1, /**< No logging */
    FuriLogLevelError   = 2, /**< Critical errors */
    FuriLogLevelWarn    = 3, /**< Warnings */
    FuriLogLevelInfo    = 4, /**< Informational */
    FuriLogLevelDebug   = 5, /**< Debug messages */
    FuriLogLevelTrace   = 6, /**< Verbose trace */
} FuriLogLevel;

/*============================================================================*/
/* FURI_LOG_* macros — map directly to M1's m1_logdb_printf()                 */
/*                                                                            */
/* Both M1 and Flipper use: (tag, format, ...) as the signature, so the       */
/* mapping is trivial.                                                        */
/*============================================================================*/

#define FURI_LOG_E(tag, format, ...) \
    m1_logdb_printf(LOG_DEBUG_LEVEL_ERROR, tag, format "\r\n", ##__VA_ARGS__)

#define FURI_LOG_W(tag, format, ...) \
    m1_logdb_printf(LOG_DEBUG_LEVEL_WARN,  tag, format "\r\n", ##__VA_ARGS__)

#define FURI_LOG_I(tag, format, ...) \
    m1_logdb_printf(LOG_DEBUG_LEVEL_INFO,  tag, format "\r\n", ##__VA_ARGS__)

#define FURI_LOG_D(tag, format, ...) \
    m1_logdb_printf(LOG_DEBUG_LEVEL_DEBUG, tag, format "\r\n", ##__VA_ARGS__)

#define FURI_LOG_T(tag, format, ...) \
    m1_logdb_printf(LOG_DEBUG_LEVEL_TRACE, tag, format "\r\n", ##__VA_ARGS__)

/*============================================================================*/
/* Flipper raw logging function — maps to m1_logdb_printf()                   */
/*                                                                            */
/* FuriLogLevel values do NOT align with M1's S_M1_LogDebugLevel_t values,    */
/* so we map explicitly rather than casting.                                  */
/*============================================================================*/

static inline S_M1_LogDebugLevel_t furi_log_level_to_m1(int level) {
    switch (level) {
        case 2:  return LOG_DEBUG_LEVEL_ERROR; /* FuriLogLevelError */
        case 3:  return LOG_DEBUG_LEVEL_WARN;  /* FuriLogLevelWarn  */
        case 4:  return LOG_DEBUG_LEVEL_INFO;  /* FuriLogLevelInfo  */
        case 5:  return LOG_DEBUG_LEVEL_DEBUG; /* FuriLogLevelDebug */
        case 6:  return LOG_DEBUG_LEVEL_TRACE; /* FuriLogLevelTrace */
        default: return LOG_DEBUG_LEVEL_NONE;
    }
}

#define furi_log_print_format(level, tag, format, ...) \
    m1_logdb_printf(furi_log_level_to_m1(level), tag, format, ##__VA_ARGS__)

/*============================================================================*/
/* TAG definition helper                                                      */
/*============================================================================*/

/** Convention: define TAG at the top of each file for use with FURI_LOG_*. */
#ifndef TAG
#define TAG "furi"
#endif

#ifdef __cplusplus
}
#endif

#endif /* FURI_LOG_H */
