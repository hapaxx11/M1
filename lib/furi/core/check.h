/* See COPYING.txt for license details. */

/*
 * check.h — Furi assertion compatibility shim for M1.
 *
 * Provides Flipper/Momentum-compatible assertion macros:
 *   furi_assert(cond)   — debug-only assertion (compiled out in release)
 *   furi_check(cond)    — always-on assertion (never compiled out)
 *   furi_crash(msg)     — unconditional crash with message
 *
 * On M1 (bare-metal STM32), these map to tight infinite loops that are
 * observable via a debugger.  The optional message parameter is stored
 * in a volatile variable so it can be inspected at the breakpoint.
 *
 * M1 Project — Hapax fork
 */

#ifndef FURI_CHECK_H
#define FURI_CHECK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Crash handler                                                              */
/*============================================================================*/

/**
 * Unconditional crash.
 *
 * On embedded targets: enter an infinite loop with interrupts disabled.
 * The message pointer is stored in a volatile local so a debugger can
 * inspect it.
 */
#ifndef furi_crash
#define furi_crash(msg)                                                     \
    do {                                                                    \
        volatile const char* _furi_crash_msg __attribute__((unused)) = msg; \
        __disable_irq();                                                    \
        while (1) { __NOP(); }                                             \
    } while (0)
#endif

/*============================================================================*/
/* furi_check — always-on assertion (never compiled out)                       */
/*============================================================================*/

/**
 * Production assertion.  Fires even in release builds.
 * Use for critical invariants that must never be violated.
 */
#ifndef furi_check
#define furi_check(__cond)                                                  \
    do {                                                                    \
        if (!(__cond)) {                                                    \
            furi_crash("furi_check failed: " #__cond);                     \
        }                                                                   \
    } while (0)
#endif

/*============================================================================*/
/* furi_assert — debug-only assertion (compiled out in release)                */
/*============================================================================*/

/**
 * Debug assertion.  Active only when NDEBUG is NOT defined.
 * Use for non-critical sanity checks during development.
 */
#ifdef NDEBUG
#define furi_assert(__cond) ((void)0)
#else
#ifndef furi_assert
#define furi_assert(__cond)                                                 \
    do {                                                                    \
        if (!(__cond)) {                                                    \
            furi_crash("furi_assert failed: " #__cond);                    \
        }                                                                   \
    } while (0)
#endif
#endif /* NDEBUG */

/*============================================================================*/
/* Convenience: UNUSED macro                                                  */
/*============================================================================*/

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* FURI_CHECK_H */
