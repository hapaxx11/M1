/* See COPYING.txt for license details. */

/*
 * rf_fingerprint.c
 *
 * Implementation of the sensor-agnostic RF fingerprint extractor.  See
 * rf_fingerprint.h for the rationale.  Pure logic, zero hardware deps.
 *
 * M1 Project — Hapax fork
 */

#include "rf_fingerprint.h"
#include "subghz_mod_suggest.h"
#include "rf_repetition.h"

#include <string.h>

/* Modulation-preset bandwidth/deviation table (Hz), indexed by the M1
 * subghz_mod_presets[] order: AM270, AM650, FM238, FM476.  The number in each
 * preset label is the OOK channel bandwidth (AM) or peak FSK deviation (FM) in
 * kHz — mirrors RF Rosetta's OOK-650kHz / FSK-238kHz / FSK-476kHz scan modes. */
static const uint32_t rf_preset_bw_hz[] = {
    270000U,    /* AM270 */
    650000U,    /* AM650 */
    238000U,    /* FM238 (deviation) */
    476000U,    /* FM476 (deviation) */
};
#define RF_PRESET_BW_COUNT \
    (sizeof(rf_preset_bw_hz) / sizeof(rf_preset_bw_hz[0]))

uint32_t rf_bandwidth_for_preset(uint8_t preset_idx)
{
    if (preset_idx >= RF_PRESET_BW_COUNT)
        return 0U;
    return rf_preset_bw_hz[preset_idx];
}

uint16_t rf_band_from_freq(uint32_t freq_hz)
{
    /* Ordered from lowest band up.  Ranges are deliberately generous so an
     * off-nominal capture (e.g. 434.0 vs 433.92) still lands in its band. */
    if (freq_hz >= 300000000U && freq_hz <= 313000000U) return RF_BAND_300;
    if (freq_hz >  313000000U && freq_hz <= 322000000U) return RF_BAND_315;
    if (freq_hz >  322000000U && freq_hz <= 348000000U) return RF_BAND_300;
    if (freq_hz >= 387000000U && freq_hz <= 464000000U) return RF_BAND_433;
    if (freq_hz >= 779000000U && freq_hz <= 901000000U) return RF_BAND_868;
    if (freq_hz >  901000000U && freq_hz <= 928000000U) return RF_BAND_915;
    if (freq_hz >= 2400000000U && freq_hz <= 2500000000U) return RF_BAND_2400;
    return 0U;
}

const char *rf_mod_family_str(rf_mod_family_t mod)
{
    switch (mod)
    {
        case RF_MOD_OOK: return "OOK/AM";
        case RF_MOD_FSK: return "FSK/FM";
        default:         return "?";
    }
}

/* Map the mod-suggest family enum onto the shared rf_mod_family_t. */
static rf_mod_family_t map_mod(SubGhzModSuggestType t)
{
    switch (t)
    {
        case SUBGHZ_MOD_SUGGEST_OOK: return RF_MOD_OOK;
        case SUBGHZ_MOD_SUGGEST_FSK: return RF_MOD_FSK;
        default:                     return RF_MOD_UNKNOWN;
    }
}

void rf_fingerprint_from_subghz_raw(const int16_t *raw_data,
                                    uint16_t       raw_count,
                                    uint32_t       freq_hz,
                                    uint8_t        preset_idx,
                                    int16_t        rssi_dbm,
                                    int16_t        noise_dbm,
                                    rf_fingerprint_t *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));
    out->sensor  = RF_SENSOR_SUBGHZ;
    out->freq_hz = freq_hz;
    out->band    = rf_band_from_freq(freq_hz);

    /* Modulation family + timing element via the #616 heuristic. */
    SubGhzModSuggestResult m = subghz_mod_suggest(raw_data, raw_count);
    out->mod            = map_mod(m.type);
    out->mod_confidence = (uint8_t)m.confidence;   /* 0..3 */
    out->te_us          = m.te;
    out->pulse_count    = m.pulse_count;

    /* If the caller supplied a modulation preset, prefer it as the definitive
     * modulation family (the operator chose the demod path) and derive
     * bandwidth from it.  The heuristic still supplies te and confidence. */
    out->bandwidth_hz = rf_bandwidth_for_preset(preset_idx);
    if (preset_idx < RF_PRESET_BW_COUNT)
    {
        /* Presets 0,1 are AM/OOK; 2,3 are FM/FSK. */
        out->mod = (preset_idx <= 1) ? RF_MOD_OOK : RF_MOD_FSK;
    }

    /* Repetition (the "[x2]" badge). */
    rf_repetition_t rep = rf_repetition_detect(raw_data, raw_count);
    out->repetition     = rep.count;
    out->rep_confidence = rep.confidence;

    /* Rough payload bit estimate: pulses in one burst, ~1 bit per pulse pair
     * for OOK PWM.  Use the per-burst pulse count when repetition was found so
     * repeats do not inflate the estimate. */
    uint16_t burst_pulses = (rep.burst_pulses > 0) ? rep.burst_pulses
                                                    : m.pulse_count;
    out->est_bits = (uint16_t)(burst_pulses / 2U);

    /* Link-quality fields. */
    out->rssi_dbm  = rssi_dbm;
    out->noise_dbm = noise_dbm;
    if (rssi_dbm != 0 && noise_dbm != 0)
        out->snr_db = (int16_t)(rssi_dbm - noise_dbm);
}
