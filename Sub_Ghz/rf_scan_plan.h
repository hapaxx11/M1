/* See COPYING.txt for license details. */

/*
 * rf_scan_plan.h
 *
 * Frequency probe plan + cursor for the RF Rosetta Signal Identifier sweep.
 *
 * The Signal Identifier visits a curated list of common Sub-GHz ISM
 * frequencies, dwelling briefly on each to look for activity.  This module is
 * the pure-logic description of *what to visit and in what order* — the
 * ordered probe list plus a tiny cursor that steps through it and reports when
 * a full pass has completed.  It carries no hardware state, so the sweep
 * geometry is deterministic and unit-testable (tests/test_rf_scan_plan.c);
 * the SI4463 delegate only consumes it.
 *
 * Each point also records whether the radio must use the 915-style config /
 * frontend (>= 850 MHz) versus the 433-style path, mirroring the boundary the
 * Frequency Analyzer and Freq Scanner already use.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_SCAN_PLAN_H
#define RF_SCAN_PLAN_H

#include <stdint.h>
#include <stdbool.h>

/** The 850 MHz boundary between 433-style and 915-style radio config. */
#define RF_SCAN_915_BOUNDARY_HZ   850000000UL

/** One frequency to probe during the sweep. */
typedef struct {
    uint32_t    freq_hz;   /**< Centre frequency to tune (Hz) */
    bool        use_915;   /**< true: 915-style radio config + frontend */
    const char *label;     /**< Short display label, e.g. "433.92" */
} rf_scan_point_t;

/** Number of frequencies in the probe plan. */
uint16_t rf_scan_plan_count(void);

/** Probe point at @p idx, or NULL if out of range. */
const rf_scan_point_t *rf_scan_plan_point(uint16_t idx);

/** Cursor stepping through the plan. Zero-initialisation == reset. */
typedef struct {
    uint16_t idx;   /**< Index of the *next* point to return */
    uint32_t pass;  /**< Completed full passes over the plan */
} rf_scan_cursor_t;

/** Reset a cursor to the start of the plan. */
void rf_scan_cursor_reset(rf_scan_cursor_t *cur);

/** Current probe point the cursor points at (NULL if plan empty). */
const rf_scan_point_t *rf_scan_cursor_point(const rf_scan_cursor_t *cur);

/**
 * Advance the cursor to the next probe point.
 *
 * @return true when the advance wrapped past the end of the plan (i.e. a full
 *         sweep pass just completed and @p pass was incremented).
 */
bool rf_scan_cursor_advance(rf_scan_cursor_t *cur);

#endif /* RF_SCAN_PLAN_H */
