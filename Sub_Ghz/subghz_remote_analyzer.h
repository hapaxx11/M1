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

#endif /* SUBGHZ_REMOTE_ANALYZER_H */
