/* See COPYING.txt for license details. */

/**
 * @file   music_parser.h
 * @brief  Pure-logic Flipper Music Format (FMF) parsing helpers.
 *
 * Hardware-independent module.  No HAL, RTOS, display, or filesystem
 * dependencies.  Can be tested on the host.
 *
 * Extracted from music_player.c per the "Preferred Modularization Pattern"
 * (CLAUDE.md §Extract Pure Logic).
 */

#ifndef MUSIC_PARSER_H_
#define MUSIC_PARSER_H_

#include <stdint.h>
#include <stdbool.h>

/*============================================================================*/
/* Constants                                                                  */
/*============================================================================*/

/** Semitone index that means "pause / rest" instead of a pitched note. */
#define MUSIC_SEMITONE_PAUSE   0xFF

/** Minimum buzzer frequency (Hz); lower pitches are clamped to this. */
#define MUSIC_MIN_FREQ_HZ     130

/** Maximum buzzer frequency (Hz); higher pitches are clamped to this. */
#define MUSIC_MAX_FREQ_HZ     16000

/*============================================================================*/
/* Types                                                                      */
/*============================================================================*/

/** A single parsed note from an FMF Notes line. */
typedef struct {
    uint8_t semitone;   /**< Absolute semitone (0 = C0); 0xFF = pause */
    uint8_t duration;   /**< Note value: 1=whole, 2=half, 4=quarter, 8=eighth … */
    uint8_t dots;       /**< Number of augmentation dots (0..4) */
} MusicNote_t;

/*============================================================================*/
/* Pure functions                                                             */
/*============================================================================*/

/**
 * @brief  Convert an absolute semitone number to a buzzer frequency in Hz.
 *
 * Uses equal-temperament tuning with A4 = 440 Hz.
 * Semitone 0 = C0, 48 = C4, 57 = A4, etc.
 *
 * @param  semitone  Absolute semitone index (0–107 for octaves 0–8).
 *                   Pass MUSIC_SEMITONE_PAUSE (0xFF) for a rest.
 * @return Frequency in Hz, clamped to [MUSIC_MIN_FREQ_HZ, MUSIC_MAX_FREQ_HZ].
 *         Returns 0 for pause.
 */
uint16_t music_semitone_to_freq(uint8_t semitone);

/**
 * @brief  Compute note duration in milliseconds from BPM and note value.
 *
 * A quarter note at the given BPM is the base beat.
 * Each augmentation dot adds half the remaining duration.
 *
 * @param  duration  Note value (1 = whole … 32 = thirty-second).
 * @param  dots      Number of dots (0..4).
 * @param  bpm       Beats per minute (quarter note = 1 beat).
 * @return Duration in milliseconds (minimum 10 ms).
 *         Returns 250 if duration or bpm is 0 (safety fallback).
 */
uint32_t music_note_duration_ms(uint8_t duration, uint8_t dots, uint32_t bpm);

/**
 * @brief  Parse a single FMF note token from a string.
 *
 * Token format: [duration] NOTE [# | _] [octave] [....] , ...
 *
 * @param  cursor    Pointer into the Notes value string; updated to point past
 *                   the consumed token on success.
 * @param  def_dur   Default duration when none is specified in the token.
 * @param  def_oct   Default octave when none is specified in the token.
 * @param  note_out  Populated with the parsed note on success.
 * @return true if a valid note was parsed; false on end-of-string or error.
 */
bool music_parse_note_token(const char **cursor,
                            uint32_t def_dur, uint32_t def_oct,
                            MusicNote_t *note_out);

#endif /* MUSIC_PARSER_H_ */
