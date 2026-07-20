/* See COPYING.txt for license details. */

/*
 * subghz_mod_suggest.h
 *
 * Waveform-based modulation suggestion for Sub-GHz RAW captures.
 *
 * Inspired by RF Rosetta (github.com/joelewis012/RF_Rosetta) — when a
 * captured RAW signal cannot be matched to a known protocol, it is useful
 * to hint the operator whether the waveform *looks* like clean OOK/AM
 * keying or like the noisy jitter an FSK/FM signal produces when it is
 * demodulated with an OOK detector.
 *
 * The analyzer is a pure function over the same signed timing-sample array
 * that subghz_decode_raw_offline() consumes: alternating positive/negative
 * int16_t values representing mark/space durations in µs.  Only the
 * absolute durations are used.  Because it depends on nothing but the
 * sample array it is hardware-independent and compiles on both ARM and
 * host, and is directly unit-tested by tests/test_subghz_mod_suggest.c.
 *
 * IMPORTANT — what this can and cannot do:
 *   Distinguishing OOK from FSK from timing alone is fundamentally a
 *   heuristic (true separation needs frequency-domain / deviation data).
 *   The suggestion is therefore a *hint* with an explicit confidence
 *   level, never an authoritative classification.  A clean, well
 *   quantized OOK PWM waveform (short/long pulses that are integer
 *   multiples of a timing element) scores as OOK; a waveform dominated by
 *   irregular sub-timing-element jitter with no quantization scores as
 *   FSK.  Everything else is reported UNKNOWN.
 *
 * M1 Project — Hapax fork
 */

#ifndef SUBGHZ_MOD_SUGGEST_H
#define SUBGHZ_MOD_SUGGEST_H

#include <stdint.h>
#include <stdbool.h>

/** Suggested modulation family. */
typedef enum {
    SUBGHZ_MOD_SUGGEST_UNKNOWN = 0, /**< Not enough evidence either way */
    SUBGHZ_MOD_SUGGEST_OOK,         /**< Amplitude keying (OOK / AM) */
    SUBGHZ_MOD_SUGGEST_FSK          /**< Frequency keying (FSK / FM) */
} SubGhzModSuggestType;

/** Confidence in the suggestion. */
typedef enum {
    SUBGHZ_MOD_SUGGEST_CONF_NONE = 0, /**< No usable data */
    SUBGHZ_MOD_SUGGEST_CONF_LOW,
    SUBGHZ_MOD_SUGGEST_CONF_MEDIUM,
    SUBGHZ_MOD_SUGGEST_CONF_HIGH
} SubGhzModSuggestConfidence;

/** Result of a waveform analysis. */
typedef struct {
    SubGhzModSuggestType       type;        /**< Suggested modulation */
    SubGhzModSuggestConfidence confidence;  /**< How much to trust it */
    uint16_t                   te;          /**< Estimated timing element (µs); 0 if unknown */
    uint16_t                   pulse_count; /**< Number of in-band pulses analysed */
    uint8_t                    quant_pct;   /**< % of in-band pulses that quantize to n·te */
} SubGhzModSuggestResult;

/*============================================================================*/
/* Tunables (exposed for tests)                                               */
/*============================================================================*/

/* Durations shorter than this (µs) are treated as glitches/noise, not pulses. */
#ifndef SUBGHZ_MOD_SUGGEST_NOISE_FLOOR
#define SUBGHZ_MOD_SUGGEST_NOISE_FLOOR   40
#endif

/* Durations longer than this (µs) are inter-packet gaps, excluded from the
 * quantization analysis (a gap is not a symbol). */
#ifndef SUBGHZ_MOD_SUGGEST_GAP_CEIL
#define SUBGHZ_MOD_SUGGEST_GAP_CEIL      10000
#endif

/* Minimum in-band pulses required before any suggestion is made. */
#ifndef SUBGHZ_MOD_SUGGEST_MIN_PULSES
#define SUBGHZ_MOD_SUGGEST_MIN_PULSES    16
#endif

/*============================================================================*/
/* API                                                                        */
/*============================================================================*/

/**
 * Analyse a RAW timing-sample array and suggest a modulation family.
 *
 * @param raw_data   Array of signed timing samples (mark/space, µs).  The
 *                   sign is ignored; only |duration| is used.
 * @param raw_count  Number of samples in raw_data.
 * @return           Suggestion result.  On NULL/empty/too-few input the
 *                   result is type UNKNOWN, confidence NONE, all-zero.
 */
SubGhzModSuggestResult subghz_mod_suggest(const int16_t *raw_data,
                                          uint16_t raw_count);

/** Human-readable label for a suggestion type ("OOK/AM", "FSK/FM", "?"). */
const char *subghz_mod_suggest_type_str(SubGhzModSuggestType type);

/** Human-readable label for a confidence level ("low"/"med"/"high"/"-"). */
const char *subghz_mod_suggest_confidence_str(SubGhzModSuggestConfidence conf);

#endif /* SUBGHZ_MOD_SUGGEST_H */
