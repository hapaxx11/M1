/* See COPYING.txt for license details. */

/*
 * subghz_remote_analyzer.c
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

#include "subghz_remote_analyzer.h"

#define SUBGHZ_RA_JUMP_HZ 500000u /* |new - state| above this -> fast k */

void subghz_ra_reset(subghz_ra_state_t *st) {
    st->coarse.best_rssi = INT16_MIN;
    st->coarse.best_freq = 0;
    st->fine.best_rssi = INT16_MIN;
    st->fine.best_freq = 0;
    st->fil_val = 0;
    st->last_shown_freq = 0;
    st->last_shown_rssi = 0;
    st->last_shown_am = false;
    st->sample_hold = 0;
}

void subghz_ra_coarse_feed(subghz_ra_state_t *st, uint32_t freq_hz, int16_t rssi) {
    /* Strict '>' keeps the FIRST frequency on ties, like upstream. */
    if(rssi > st->coarse.best_rssi) {
        st->coarse.best_rssi = rssi;
        st->coarse.best_freq = freq_hz;
    }
}

bool subghz_ra_fine_analyze(const int16_t *sweep_rssi, uint8_t count,
                            uint32_t base_freq_hz, subghz_ra_state_t *st,
                            bool *is_am) {
    if(count < 3) return false;

    int16_t max_rssi = sweep_rssi[0];
    uint8_t max_idx = 0;
    for(uint8_t i = 1; i < count; i++) {
        if(sweep_rssi[i] > max_rssi) {
            max_rssi = sweep_rssi[i];
            max_idx = i;
        }
    }

    st->fine.best_rssi = max_rssi;
    st->fine.best_freq = base_freq_hz - SUBGHZ_RA_SWEEP_HALF_SPAN_HZ +
                         (uint32_t)max_idx * SUBGHZ_RA_SWEEP_STEP_HZ;

    /* Missing neighbor at an edge counts as equal to the peak, so an edge
     * peak is never AM (upstream: left = max_idx > 0 ? sweep[max_idx-1]
     * : max_rssi). */
    int16_t left = (max_idx > 0) ? sweep_rssi[max_idx - 1] : max_rssi;
    int16_t right = ((uint8_t)(max_idx + 1) < count) ? sweep_rssi[max_idx + 1] : max_rssi;

    *is_am = ((max_rssi - left) > SUBGHZ_RA_AM_NEIGHBOR_DROP_DB) &&
             ((max_rssi - right) > SUBGHZ_RA_AM_NEIGHBOR_DROP_DB);

    /* Carry this cycle's AM verdict for subghz_ra_deliver(); it becomes the
     * remembered frame's flag if the fine or coarse path delivers. */
    st->fine_is_am = *is_am;
    return true;
}

uint32_t subghz_ra_smooth(subghz_ra_state_t *st, uint32_t freq_hz) {
    if(st->fil_val == 0) {
        /* First sample after reset initializes the state directly (M1
         * deviation: upstream converges from an uninitialized filVal, which
         * undershoots early readings). */
        st->fil_val = freq_hz;
        return freq_hz;
    }

    int64_t diff = (int64_t)freq_hz - (int64_t)st->fil_val;
    uint32_t adiff = (diff >= 0) ? (uint32_t)diff : (uint32_t)(-diff);
    int64_t adj;
    if(adiff > SUBGHZ_RA_JUMP_HZ) {
        /* k = 0.9, round-half-up on the magnitude */
        adj = (diff * 9 + ((diff >= 0) ? 5 : -5)) / 10;
    } else {
        /* k = 0.03 */
        adj = (diff * 3 + ((diff >= 0) ? 50 : -50)) / 100;
    }
    st->fil_val = (uint32_t)((int64_t)st->fil_val + adj);
    return st->fil_val;
}

subghz_ra_frame_t subghz_ra_deliver(subghz_ra_state_t *st, int16_t trigger_dbm) {
    subghz_ra_frame_t frame = {.frequency = 0, .rssi = 0, .is_am = false, .signal_present = false};
    bool fine_ran = (st->fine.best_rssi != INT16_MIN);

    if(st->fine.best_rssi > trigger_dbm) {
        /* Fine peak above trigger wins. */
        st->sample_hold = SUBGHZ_RA_HOLD_FRAMES;
        st->last_shown_freq = st->fine.best_freq;
        st->last_shown_rssi = st->fine.best_rssi;
        st->last_shown_am = st->fine_is_am;
        frame.is_am = st->last_shown_am;
        frame.rssi = st->fine.best_rssi;
        /* subghz_ra_smooth() passes the first sample through unchanged and
         * initializes the state, so calling it unconditionally both honors
         * upstream's "smooth only when initialized" and engages the smoother. */
        frame.frequency = subghz_ra_smooth(st, st->fine.best_freq);
        frame.signal_present = true;
    } else if(st->coarse.best_rssi > trigger_dbm && st->sample_hold < 2) {
        /* Coarse peak above trigger while the hold counter is low. */
        st->sample_hold = SUBGHZ_RA_HOLD_FRAMES;
        st->last_shown_freq = st->coarse.best_freq;
        st->last_shown_rssi = st->coarse.best_rssi;
        st->last_shown_am = fine_ran ? st->fine_is_am : false;
        frame.is_am = st->last_shown_am;
        frame.rssi = st->coarse.best_rssi;
        frame.frequency = subghz_ra_smooth(st, st->coarse.best_freq);
        frame.signal_present = true;
    } else {
        if(st->sample_hold > 0) {
            st->sample_hold--;
            if(st->sample_hold == 3) {
                /* Upstream calls back exactly here with the remembered
                 * frequency/RSSI and signal_present=false. */
                frame.frequency = st->last_shown_freq;
                frame.rssi = st->last_shown_rssi;
                frame.is_am = st->last_shown_am;
                frame.signal_present = false;
            } else if(st->sample_hold == 0) {
                st->fil_val = 0;
            } else {
                /* Upstream does not call back on the other decaying counts,
                 * so the display keeps the last value; the per-cycle frame
                 * API delivers the remembered frame with signal_present=true. */
                frame.frequency = st->last_shown_freq;
                frame.rssi = st->last_shown_rssi;
                frame.is_am = st->last_shown_am;
                frame.signal_present = true;
            }
        } else {
            st->fil_val = 0;
        }
    }

    /* Upstream resets rssi_coarse/rssi_fine to -127 at the start of every
     * scan cycle; consuming the peaks here gives the same per-cycle
     * semantics (the caller re-feeds coarse and re-runs fine each cycle). */
    st->coarse.best_rssi = INT16_MIN;
    st->coarse.best_freq = 0;
    st->fine.best_rssi = INT16_MIN;
    st->fine.best_freq = 0;

    return frame;
}

uint32_t subghz_ra_nearest_frequency(const uint32_t *freqs, uint8_t count,
                                     uint32_t input_hz) {
    uint32_t prev_freq = 0;
    uint32_t result = 0;

    for(uint8_t i = 0; i < count; i++) {
        uint32_t current = freqs[i];
        if(current == 0) {
            continue;
        }
        if(current == input_hz) {
            result = current;
            break;
        }
        if(current > input_hz && prev_freq < input_hz) {
            /* Equal distance picks the lower frequency (upstream else-branch). */
            if(current - input_hz < input_hz - prev_freq) {
                result = current;
            } else {
                result = prev_freq;
            }
            break;
        }
        prev_freq = current;
    }

    return result;
}
