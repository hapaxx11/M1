/* See COPYING.txt for license details. */

/*
 * subghz_freq_presets.h
 *
 * Sub-GHz frequency preset table — hardware-independent pure data.
 * Extracted from m1_sub_ghz.c so the table can be compiled and
 * validated on the host without any HAL or RTOS dependencies.
 *
 * When adding, removing, or reordering entries in subghz_freq_presets.c:
 *   1. Update SUBGHZ_FREQ_PRESET_COUNT to match the new array length.
 *   2. Verify SUBGHZ_FREQ_DEFAULT_IDX still points to 433.92 MHz.
 *   3. Keep SUBGHZ_FREQ_PRESET_CUSTOM == SUBGHZ_FREQ_PRESET_COUNT.
 *   4. Run the host-side test suite — test_subghz_freq_presets will
 *      catch any count mismatch, out-of-range entry, or sorting error.
 *   5. The table is already at the 64-preset ceiling a uint64_t freq
 *      mask can address (63 real presets + Custom == indices 0..63).
 *      Adding one more real preset trips the SUBGHZ_FREQ_PRESET_CUSTOM
 *      #error below at compile time; widen the freq-mask type in
 *      subghz_protocol_registry.[ch] and m1_subghz_scene_config.c (and
 *      the matching host test) before increasing COUNT past 63.
 *
 * When porting a new Sub-GHz protocol from Flipper/Momentum:
 *   - Check every frequency at which the protocol operates (e.g. 319.5 MHz
 *     for Magellan/GE/Interlogix NA sensors) and verify it is present in
 *     this table. If missing, ADD IT before registering the protocol.
 *   - Set the protocol's band flags (SubGhzProtocolFlag_315 / _433 / _868 /
 *     _300) to include every band where the protocol is actually used.
 *     A flag mismatch means "Add Manually" will not show the protocol in
 *     the correct band picker, and automated tests will catch it.
 */

#ifndef SUBGHZ_FREQ_PRESETS_H
#define SUBGHZ_FREQ_PRESETS_H

#include <stdint.h>

/* SubGhzProtocolFlag band flags are referenced by helper functions in this
 * module.  Include the registry header only for the flag definitions; the
 * actual protocol table is not needed here. */
#include "subghz_protocol_registry.h"

/* ── Preset table dimensions ──────────────────────────────────────────────── */

/** Total number of real frequency presets in the table. */
#define SUBGHZ_FREQ_PRESET_COUNT    63

/** Sentinel index representing a user-entered custom frequency (== COUNT). */
#define SUBGHZ_FREQ_PRESET_CUSTOM   63

/** Index of the factory-default frequency preset (433.92 MHz). */
#define SUBGHZ_FREQ_DEFAULT_IDX     40

/* subghz_protocol_freq_mask_for_registry() and the Config scene's
 * cfg_allowed_freq_mask cache pack one bit per preset index (0..CUSTOM)
 * into a uint64_t, so SUBGHZ_FREQ_PRESET_CUSTOM must stay <= 63 — the
 * table is already at that limit (63 real presets + Custom == 64 bits
 * used). Adding one more real preset would require widening every
 * freq-mask type (currently uint64_t) and every UINT64_C(1) << idx shift
 * across subghz_protocol_registry.[ch] and m1_subghz_scene_config.c, or
 * switching to a bitset array. This assert exists so that day the build
 * fails loudly instead of silently truncating/overflowing the mask.
 * See test_freq_preset_custom_fits_in_uint64_mask() in
 * tests/test_subghz_freq_presets.c for the matching host-side check. */
#if SUBGHZ_FREQ_PRESET_CUSTOM > 63
#error "SUBGHZ_FREQ_PRESET_CUSTOM no longer fits a uint64_t freq mask bit index — widen the freq-mask type (see subghz_protocol_freq_mask_for_registry and cfg_allowed_freq_mask) before adding more presets"
#endif

/* ── SI4463 hardware limits ────────────────────────────────────────────────── */

/** Lower PLL bound (Hz) — below this the synthesizer cannot lock. */
#ifndef SUBGHZ_MIN_FREQ_HZ
#define SUBGHZ_MIN_FREQ_HZ   300000000UL   /* 300.000 MHz */
#endif

/** Upper ISM edge (Hz). */
#ifndef SUBGHZ_MAX_FREQ_HZ
#define SUBGHZ_MAX_FREQ_HZ   928000000UL   /* 928.000 MHz */
#endif

/* ── Type definition ────────────────────────────────────────────────────────── */

/** One entry in the frequency preset table. */
typedef struct {
    uint32_t    freq_hz;   /**< Centre frequency in Hz */
    const char *label;     /**< Human-readable label, e.g. "433.92" */
} SubGhzFreqPreset;

/* ── Table declaration ──────────────────────────────────────────────────────── */

/** The frequency preset table (SUBGHZ_FREQ_PRESET_COUNT entries). */
extern const SubGhzFreqPreset subghz_freq_presets[SUBGHZ_FREQ_PRESET_COUNT];

/* ── Frequency hopper tables ────────────────────────────────────────────────── */

/**
 * Number of frequencies in each per-region hopper table.
 * Do not change — dwell-time math and the UI display are calibrated around 6.
 */
#define SUBGHZ_HOPPER_FREQ_COUNT  6

/**
 * Per-region hopper frequency tables.
 * All entries must be present in subghz_freq_presets[] — enforced by
 * test_subghz_hopper_freqs_in_presets() in test_subghz_freq_presets.c.
 */
extern const uint32_t subghz_hopper_freqs_NA[SUBGHZ_HOPPER_FREQ_COUNT];
extern const uint32_t subghz_hopper_freqs_EU[SUBGHZ_HOPPER_FREQ_COUNT];
extern const uint32_t subghz_hopper_freqs_ASIA[SUBGHZ_HOPPER_FREQ_COUNT];
extern const uint32_t subghz_hopper_freqs_OFF[SUBGHZ_HOPPER_FREQ_COUNT];

/**
 * Return the hopper frequency table for the given ISM region index:
 *   0 → North America   (315/345/390/433.92/434.42/915 MHz)
 *   1 → Europe          (433.42/433.92/434.07/434.42/868.35/868.95 MHz)
 *   2 → Asia/APAC       (315/330/345/433.92/434.42/868.35 MHz)
 *   3/other → Off       (wide cross-region fallback)
 *
 * This function is hardware-independent and reads no globals; the caller
 * supplies the region index (from m1_device_stat.config.ism_band_region).
 * Hopping reads ism_band_region at runtime via subghz_get_hopper_freqs_ext().
 */
const uint32_t *subghz_get_hopper_freqs(uint8_t ism_region);

/**
 * @brief Find the frequency preset index whose frequency equals @p freq_hz.
 *
 * Searches the real preset table only (indices 0..SUBGHZ_FREQ_PRESET_COUNT-1)
 * and never returns SUBGHZ_FREQ_PRESET_CUSTOM: this function has no
 * knowledge of the current user custom frequency, so a custom-only match
 * (or no match at all) is reported the same way, as -1.
 *
 * @param freq_hz  Frequency in Hz.
 * @return Preset index on success, -1 if not found.
 */
int16_t subghz_freq_preset_find_hz(uint32_t freq_hz);

/**
 * @brief Find the first frequency preset index whose frequency is within
 *        ±@p tolerance_hz of @p freq_hz.
 *
 * Used to map nominal protocol frequencies (which may differ by a few kHz
 * from the preset table entries) to a preset index.
 *
 * @param freq_hz      Target frequency in Hz.
 * @param tolerance_hz Maximum absolute difference in Hz.
 * @return Preset index on success, -1 if no preset is within tolerance.
 */
int16_t subghz_freq_preset_find_near_hz(uint32_t freq_hz,
                                         uint32_t tolerance_hz);

/**
 * @brief Map a SubGhzProtocolFlag band flag to its nominal centre frequency.
 *
 * Returns the canonical centre frequency for the 300/315/433/868 MHz band
 * flags.  This is a convenience for tests and UI filters; the actual preset
 * table may contain multiple entries within each band.
 *
 * @param flag  One of SubGhzProtocolFlag_300, _315, _433, _868.
 * @return Nominal centre frequency in Hz, or 0 for an unknown flag.
 */
uint32_t subghz_freq_preset_band_center(uint32_t flag);

#endif /* SUBGHZ_FREQ_PRESETS_H */
