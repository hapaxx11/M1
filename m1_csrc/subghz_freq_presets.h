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

/* ── Preset table dimensions ──────────────────────────────────────────────── */

/** Total number of real frequency presets in the table. */
#define SUBGHZ_FREQ_PRESET_COUNT    63

/** Sentinel index representing a user-entered custom frequency (== COUNT). */
#define SUBGHZ_FREQ_PRESET_CUSTOM   63

/** Index of the factory-default frequency preset (433.92 MHz). */
#define SUBGHZ_FREQ_DEFAULT_IDX     40

/* ── SI4463 hardware limits ────────────────────────────────────────────────── */

/** Lower PLL bound (Hz) — below this the synthesizer cannot lock. */
#define SUBGHZ_MIN_FREQ_HZ   300000000UL   /* 300.000 MHz */

/** Upper ISM edge (Hz). */
#define SUBGHZ_MAX_FREQ_HZ   928000000UL   /* 928.000 MHz */

/* ── Type definition ────────────────────────────────────────────────────────── */

/** One entry in the frequency preset table. */
typedef struct {
    uint32_t    freq_hz;   /**< Centre frequency in Hz */
    const char *label;     /**< Human-readable label, e.g. "433.92" */
} SubGhzFreqPreset;

/* ── Table declaration ──────────────────────────────────────────────────────── */

/** The frequency preset table (SUBGHZ_FREQ_PRESET_COUNT entries). */
extern const SubGhzFreqPreset subghz_freq_presets[SUBGHZ_FREQ_PRESET_COUNT];

#endif /* SUBGHZ_FREQ_PRESETS_H */
