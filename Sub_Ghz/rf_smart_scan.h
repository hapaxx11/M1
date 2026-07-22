/* See COPYING.txt for license details. */

/*
 * rf_smart_scan.h
 *
 * Pure-logic helpers for Smart ID: converts raw frequency hits from the
 * quick pre-scan pass into an rf_scan_point_t probe list that the Signal
 * Identifier can iterate instead of the fixed ISM plan.
 *
 * Hardware-independent — compiles on ARM and host, unit-tested by
 * tests/test_rf_smart_scan.c.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_SMART_SCAN_H
#define RF_SMART_SCAN_H

#include <stdint.h>
#include <stdbool.h>
#include "rf_scan_plan.h"

/** Maximum number of active frequencies tracked in one Smart Scan pass. */
#define RF_SMART_SCAN_MAX_FREQS   16U

/** SI4463 sub-GHz lower bound — reject anything below this. */
#define RF_SMART_SCAN_FREQ_MIN_HZ  250000000UL

/** SI4463 sub-GHz upper bound — reject anything above this. */
#define RF_SMART_SCAN_FREQ_MAX_HZ  960000000UL

/**
 * Deduplication radius.
 *
 * Two entries are considered the same channel if they are within this many
 * Hz of each other.  Matches the 100 kHz step floor of the pre-scan.
 */
#define RF_SMART_SCAN_DEDUP_HZ     100000UL

/**
 * Return true if @p freq_hz falls within the SI4463 operating range
 * [RF_SMART_SCAN_FREQ_MIN_HZ, RF_SMART_SCAN_FREQ_MAX_HZ].
 */
bool rf_smart_scan_freq_valid(uint32_t freq_hz);

/**
 * Build an rf_scan_point_t probe list from an array of raw detected
 * frequencies.
 *
 * For each entry in @p freq_hz:
 *  - Skip if outside [RF_SMART_SCAN_FREQ_MIN_HZ, RF_SMART_SCAN_FREQ_MAX_HZ].
 *  - Skip if within RF_SMART_SCAN_DEDUP_HZ of an already-added entry.
 *  - Set @c use_915 = (freq_hz >= RF_SCAN_915_BOUNDARY_HZ).
 *  - Generate a short MHz label (e.g. "433.92").
 *  - Write to @p out[].
 *
 * @param[in]  freq_hz   Array of raw frequencies (Hz).
 * @param[in]  count     Number of entries in @p freq_hz.
 * @param[out] out       Caller-provided output buffer.
 * @param[in]  max_out   Capacity of @p out (entries, not bytes).
 * @return Number of probe points written to @p out.
 */
uint8_t rf_smart_scan_build_plan(const uint32_t *freq_hz, uint8_t count,
                                 rf_scan_point_t *out, uint8_t max_out);

#endif /* RF_SMART_SCAN_H */
