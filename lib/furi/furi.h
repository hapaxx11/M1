/* See COPYING.txt for license details. */

/*
 * furi.h — Aggregate header for the Furi compatibility layer on M1.
 *
 * Including <furi.h> or "furi.h" pulls in the subset of Momentum/Flipper's
 * Furi runtime that M1 implements.  This allows protocol code ported from
 * Momentum to compile with:
 *
 *     #include <furi.h>
 *
 * The current implementation covers:
 *   - FuriString         (dynamic string)
 *   - FURI_LOG_*         (logging macros → M1's m1_logdb_printf)
 *   - furi_assert/check  (assertion macros)
 *   - Common defines     (MAX, MIN, CLAMP, COUNT_OF, UNUSED, etc.)
 *
 * M1 Project — Hapax fork
 */

#ifndef FURI_H
#define FURI_H

#include "core/common_defines.h"
#include "core/check.h"
#include "core/log.h"
#include "core/string.h"

#endif /* FURI_H */
