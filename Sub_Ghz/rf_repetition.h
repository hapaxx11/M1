/* See COPYING.txt for license details. */

/*
 * rf_repetition.h
 *
 * Repeated-burst detection for RAW Sub-GHz captures — the "[x2]" repeat
 * badge from RF Rosetta (github.com/joelewis012/RF_Rosetta).
 *
 * Most Sub-GHz remotes transmit their payload several times in a row,
 * separated by a long inter-packet gap, so the receiver has multiple
 * chances to decode a clean copy.  Counting how many times the same burst
 * repeats is a useful physical fingerprint: it distinguishes a deliberate
 * multi-repeat remote transmission from a one-shot sensor beacon, and it
 * confirms that a captured signal is a genuine repeating transmission
 * rather than noise.
 *
 * This analyzer is a pure function over the same signed timing-sample array
 * that subghz_mod_suggest() and subghz_decode_raw_offline() consume:
 * alternating positive/negative int16_t values representing mark/space
 * durations in microseconds.  It depends on nothing but the sample array,
 * so it compiles on both ARM and host and is directly unit-tested by
 * tests/test_rf_repetition.c.
 *
 * Method (pure timing analysis):
 *   1. Split the capture into "bursts" at every inter-packet gap — a space
 *      whose absolute duration exceeds a multiple of the median in-band
 *      pulse duration (and an absolute floor).  A burst is the run of
 *      pulses between two such gaps.
 *   2. Group bursts by pulse-count similarity.  The size of the largest
 *      group is the repetition count; a lone burst reports count 1.
 *   3. Confidence reflects how many bursts agreed and how tightly their
 *      pulse counts clustered.
 *
 * M1 Project — Hapax fork
 */

#ifndef RF_REPETITION_H
#define RF_REPETITION_H

#include <stdint.h>
#include <stdbool.h>

/** Result of a repeated-burst analysis. */
typedef struct {
    uint8_t  count;         /**< Repeated-burst count (>=1). 1 = single / no repeat. */
    uint16_t burst_pulses;  /**< Representative pulses-per-burst (modal group). */
    uint8_t  burst_total;   /**< Total number of bursts found in the capture. */
    uint8_t  confidence;    /**< 0..100 — how strongly the bursts agreed. */
} rf_repetition_t;

/*============================================================================*/
/* Tunables (exposed for tests)                                               */
/*============================================================================*/

/* Durations shorter than this (µs) are treated as glitches/noise. */
#ifndef RF_REPETITION_NOISE_FLOOR
#define RF_REPETITION_NOISE_FLOOR    40
#endif

/* A space is an inter-packet gap when it exceeds this multiple of the median
 * in-band pulse duration. */
#ifndef RF_REPETITION_GAP_MULT
#define RF_REPETITION_GAP_MULT       8U
#endif

/* Absolute floor (µs) for the gap threshold, so a capture whose median pulse
 * is tiny still requires a genuinely long silence to split a burst. */
#ifndef RF_REPETITION_GAP_FLOOR
#define RF_REPETITION_GAP_FLOOR      2000U
#endif

/* A burst must contain at least this many pulses to count (rejects a stray
 * edge between two gaps). */
#ifndef RF_REPETITION_MIN_BURST_PULSES
#define RF_REPETITION_MIN_BURST_PULSES  6U
#endif

/* Two bursts are "the same" when their pulse counts are within this percent
 * of each other. */
#ifndef RF_REPETITION_BURST_TOL_PCT
#define RF_REPETITION_BURST_TOL_PCT  20U
#endif

/*============================================================================*/
/* API                                                                        */
/*============================================================================*/

/**
 * Detect repeated bursts in a RAW timing-sample array.
 *
 * @param raw_data   Array of signed timing samples (mark/space, µs).  The
 *                   sign selects mark vs. space; only spaces can be gaps.
 * @param raw_count  Number of samples in raw_data.
 * @return           Repetition result.  On NULL/empty/too-short input the
 *                   result is count 1, confidence 0, all-zero otherwise.
 */
rf_repetition_t rf_repetition_detect(const int16_t *raw_data,
                                     uint16_t raw_count);

#endif /* RF_REPETITION_H */
