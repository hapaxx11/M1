/* See COPYING.txt for license details. */

/*
 * rf_timing_capture.h
 *
 * Phase 4A — Timing-element extraction for the RF Rosetta Signal Identifier.
 *
 * The sweep loop collects a short burst of RSSI samples while dwelling on an
 * active frequency.  This module converts that burst into a pseudo-timing array
 * (int16_t mark/space durations in µs) using the same sign convention as the
 * M1 RAW-capture pipeline:
 *
 *   positive sample → mark (signal above threshold)
 *   negative sample → space (signal at/below threshold)
 *
 * Consecutive samples of the same polarity are run-length-merged into one
 * element.  The resulting array is compatible with subghz_mod_suggest() and
 * rf_fingerprint_from_subghz_raw(), so the sweep can populate te_us,
 * pulse_count, est_bits, and repetition — the discriminating features that
 * were previously always zero.
 *
 * Hardware-independent — compiles on ARM and host, unit-tested by
 * tests/test_rf_timing_capture.c.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_TIMING_CAPTURE_H
#define RF_TIMING_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Maximum int16_t magnitude used to cap each individual timing element.
 *
 * Subghz_mod_suggest's GAP_CEIL is 10 000 µs; anything larger is treated as
 * an inter-packet gap and skipped.  We cap at INT16_MAX (32 767 µs ≈ 32 ms) so
 * long gaps are represented but do not overflow the int16_t field.  In
 * practice, once an element exceeds GAP_CEIL the analyser ignores it anyway.
 */
#define RF_TIMING_CAPTURE_MAX_US   32767

/**
 * Convert a burst of RSSI samples into a mark/space timing array.
 *
 * Each RSSI sample is classified as "active" (mark, rssi > threshold_dbm) or
 * "quiet" (space, rssi <= threshold_dbm).  Consecutive same-class samples are
 * merged; their combined duration is `n_samples × sample_period_us` µs,
 * capped at RF_TIMING_CAPTURE_MAX_US per element.
 *
 * The output array uses the sign convention of the M1 RAW pipeline: positive
 * values are mark durations, negative values are space durations.  The first
 * element reflects the polarity of the first RSSI sample.
 *
 * @param rssi_dbm         Array of measured RSSI values (dBm).  NULL is safe.
 * @param n                Number of samples in rssi_dbm.
 * @param threshold_dbm    RSSI level (dBm) above which a sample is "active".
 * @param sample_period_us Duration represented by each sample (µs).  Must be
 *                         > 0; values < 1 are treated as 1.
 * @param out              Output buffer for int16_t timing elements.  Must not
 *                         be NULL when n > 0.
 * @param out_max          Capacity of out (elements).
 *
 * @return Number of timing elements written to out (may be 0 if rssi_dbm is
 *         NULL, n == 0, or out_max == 0).
 */
uint16_t rf_timing_from_rssi_burst(
    const int16_t *rssi_dbm,
    uint16_t       n,
    int16_t        threshold_dbm,
    uint32_t       sample_period_us,
    int16_t       *out,
    uint16_t       out_max);

#endif /* RF_TIMING_CAPTURE_H */
