/* See COPYING.txt for license details. */

/*
 * rf_sweep_display.h
 *
 * Pure-logic display formatting helpers for the Signal Identifier sweep report.
 * Transforms rf_sweep_hit_t data into truncated, annotated display strings
 * suitable for the 128-pixel-wide Nokia-font screen.
 *
 * Hardware-independent — compiles on ARM and host, unit-tested by
 * tests/test_rf_sweep_display.c.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_SWEEP_DISPLAY_H
#define RF_SWEEP_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "rf_sweep.h"

/**
 * Maximum display line length (including NUL terminator).
 * At ~6 px per char with the Nokia font, 128 px ≈ 21 chars usable.
 */
#define RF_SWEEP_DISP_LINE_LEN  32U

/**
 * Format a single sweep-hit row for on-screen rendering.
 *
 * Output format (space-padded, NUL-terminated):
 *   "{freq} {sec}{name} {conf}%"
 *
 * Where:
 *   - freq  = MHz with 2 decimal places (e.g. "433.92")
 *   - sec   = security prefix: "F:" (fixed), "R:" (rolling), "E:" (encrypted),
 *             or "" (unknown)
 *   - name  = protocol name with parenthetical tags stripped, truncated to fit
 *   - conf  = confidence percentage (shown as "?" when hits < min_hits)
 *
 * @param[out] buf       Output buffer (must be >= RF_SWEEP_DISP_LINE_LEN bytes).
 * @param      buf_len   Size of buf.
 * @param      hit       The sweep hit to format.
 * @param      min_hits  Minimum hit count before showing numeric confidence;
 *                       below this threshold, "?" is shown instead.
 */
void rf_sweep_display_format_hit(char *buf, uint16_t buf_len,
                                 const rf_sweep_hit_t *hit,
                                 uint8_t min_hits);

/**
 * Strip parenthetical tags from a signal name and copy the result into buf.
 *
 * Removes substrings matching " (xxx)" from the name.  For example:
 *   "Weather Station (868)" → "Weather Station"
 *   "Car Key Fob (fixed)"   → "Car Key Fob"
 *   "Garage/Gate (rolling)" → "Garage/Gate"
 *
 * @param[out] buf       Output buffer.
 * @param      buf_len   Size of buf (includes NUL terminator space).
 * @param      name      Source signal name (NULL-safe → empty output).
 */
void rf_sweep_display_strip_tags(char *buf, uint16_t buf_len, const char *name);

/**
 * Return a short security prefix string for a given security posture.
 *
 * @return "F:" for fixed, "R:" for rolling, "E:" for encrypted, "" for unknown.
 */
const char *rf_sweep_display_security_prefix(rf_security_t sec);

#endif /* RF_SWEEP_DISPLAY_H */
