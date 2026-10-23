/* See COPYING.txt for license details. */

/*
 * subghz_remote_analyzer.h
 *
 * Remote Analyzer detection logic (pure, host-testable): two-stage RSSI
 * sweep analysis, AM/FM classification, adaptive frequency smoothing and
 * the sample-hold delivery policy.
 *
 * Derived from the detection concepts of RocketGod-git/ProtoPirate
 * helpers/RemoteAnalyzer/protopirate_remote_analyzer_worker.c (GPLv3),
 * reworked for the SI4463 and M1's integer-dBm RSSI.  No CC1101 register
 * logic is carried over.
 *
 * M1 Project -- Hapax fork
 */

#ifndef SUBGHZ_REMOTE_ANALYZER_H
#define SUBGHZ_REMOTE_ANALYZER_H

#include <stdbool.h>
#include <stdint.h>

#define SUBGHZ_RA_SWEEP_POINTS         31u
#define SUBGHZ_RA_SWEEP_STEP_HZ        20000u
#define SUBGHZ_RA_SWEEP_HALF_SPAN_HZ   300000u
#define SUBGHZ_RA_TRIGGER_DBM          (-97)
#define SUBGHZ_RA_TRIGGER_MIN_DBM      (-110)
#define SUBGHZ_RA_TRIGGER_MAX_DBM      (-60)
#define SUBGHZ_RA_TRIGGER_STEP_DB      3
#define SUBGHZ_RA_AM_NEIGHBOR_DROP_DB  6
#define SUBGHZ_RA_HOLD_FRAMES          5u

typedef struct {
    int16_t  best_rssi;   /* INT16_MIN when no sample fed yet */
    uint32_t best_freq;
} subghz_ra_peak_t;

typedef struct {
    subghz_ra_peak_t coarse;
    subghz_ra_peak_t fine;
    bool fine_is_am;
    uint32_t fil_val;       /* adaptive smoother state; 0 = uninitialized */
    uint32_t last_shown_freq;
    int16_t  last_shown_rssi;
    bool     last_shown_am;
    uint8_t  sample_hold;
} subghz_ra_state_t;

typedef struct {
    uint32_t frequency;      /* 0 when no signal */
    int16_t  rssi;           /* dBm */
    bool     is_am;          /* steep fine-sweep neighbor drop -> AM */
    bool     signal_present;
} subghz_ra_frame_t;

void subghz_ra_reset(subghz_ra_state_t *st);

/** Adjust the user trigger by one step while keeping it within its limits. */
int16_t subghz_ra_adjust_trigger(int16_t trigger_dbm, bool increase);

/** Feed one coarse-stage sample (frequency, RSSI dBm). */
void subghz_ra_coarse_feed(subghz_ra_state_t *st, uint32_t freq_hz, int16_t rssi);

/**
 * Analyze the fine sweep collected around the coarse peak:
 * sweep_rssi[i] was measured at base_freq - SPAN + i*STEP.
 * Sets st->fine and *is_am (both immediate neighbors more than
 * SUBGHZ_RA_AM_NEIGHBOR_DROP_DB below the peak).
 *
 * @return false when fewer than 3 samples were collected.
 */
bool subghz_ra_fine_analyze(const int16_t *sweep_rssi, uint8_t count,
                            uint32_t base_freq_hz, subghz_ra_state_t *st,
                            bool *is_am);

/** A fine-sweep peak is usable only when it exceeds the detection threshold. */
bool subghz_ra_refinement_signal_valid(int16_t peak_rssi_dbm,
                                      int16_t threshold_dbm);

/** Adaptive exponential smoother (k = 0.9 on > 500 kHz jumps, else 0.03);
 * the first sample after a reset initializes the state directly (upstream
 * converges from 0, which undershoots early readings — fixed here). */
uint32_t subghz_ra_smooth(subghz_ra_state_t *st, uint32_t freq_hz);

/**
 * Delivery policy, one call per scan cycle.  Mirrors upstream: a fine peak
 * above the trigger wins; otherwise a coarse peak above the trigger while
 * the hold counter is low; otherwise the held frame, then signal-lost.
 * Reads st->coarse/st->fine and updates hold/smoother state.
 */
subghz_ra_frame_t subghz_ra_deliver(subghz_ra_state_t *st, int16_t trigger_dbm);

/** Round to the nearest frequency in a sorted ascending list (0 entries
 * are skipped).  Returns 0 when the list holds no usable frequency. */
uint32_t subghz_ra_nearest_frequency(const uint32_t *freqs, uint8_t count,
                                     uint32_t input_hz);

/*============================================================================*/
/* Smart ID refinement plan                                                   */
/*============================================================================*/

/**
 * Refinement dedup radius.  Two candidates closer than this have fine sweeps
 * (±300 kHz) that overlap enough that refining both is redundant — only the
 * stronger one is refined.
 */
#define SUBGHZ_RA_REFINE_DEDUP_HZ  500000u

/** One pre-scan candidate: a frequency that showed activity, plus its RSSI. */
typedef struct {
    uint32_t freq_hz;
    int16_t  rssi_dbm;
} subghz_ra_candidate_t;

/**
 * Pick which pre-scan candidates are worth a fine-sweep refinement, and in
 * which order.
 *
 * Selection is strongest-first (highest RSSI); a candidate is skipped when it
 * is out of [band_min_hz, band_max_hz] or within SUBGHZ_RA_REFINE_DEDUP_HZ of
 * an already-selected center.  RSSI ties keep the earlier candidate (strict
 * '>' comparison, matching subghz_ra_coarse_feed()).
 *
 * @param[in]  cands      Candidate array (NULL yields 0).
 * @param[in]  count      Number of candidates.
 * @param[in]  band_min_hz / band_max_hz  Inclusive operating limits.
 * @param[out] out_centers  Selected centers, strongest first.
 * @param[in]  max_out    Capacity of @p out_centers.
 * @return Number of centers written (<= max_out).
 */
uint8_t subghz_ra_build_refine_plan(const subghz_ra_candidate_t *cands,
                                    uint8_t count,
                                    uint32_t band_min_hz, uint32_t band_max_hz,
                                    uint32_t *out_centers, uint8_t max_out);

#endif /* SUBGHZ_REMOTE_ANALYZER_H */
