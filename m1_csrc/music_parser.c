/* See COPYING.txt for license details. */

/**
 * @file   music_parser.c
 * @brief  Pure-logic Flipper Music Format (FMF) parsing helpers.
 *
 * Hardware-independent.  No HAL, RTOS, or display dependencies.
 *
 * Extracted from music_player.c per the "Preferred Modularization Pattern"
 * (CLAUDE.md §Extract Pure Logic).
 */

#include "music_parser.h"
#include <ctype.h>

/*============================================================================*/
/* Octave 4 frequency table (equal temperament, A4 = 440 Hz, rounded)        */
/*============================================================================*/

static const uint16_t s_note_freq_oct4[12] = {
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494
};

/*============================================================================*/
/* music_semitone_to_freq                                                     */
/*============================================================================*/

uint16_t music_semitone_to_freq(uint8_t semitone)
{
    if (semitone == MUSIC_SEMITONE_PAUSE)
        return 0;

    uint8_t octave   = semitone / 12;
    uint8_t note_idx = semitone % 12;

    uint32_t freq;
    if (octave >= 4)
        freq = (uint32_t)s_note_freq_oct4[note_idx] << (octave - 4);
    else
        freq = (uint32_t)s_note_freq_oct4[note_idx] >> (4 - octave);

    if (freq < MUSIC_MIN_FREQ_HZ)
        freq = MUSIC_MIN_FREQ_HZ;
    if (freq > MUSIC_MAX_FREQ_HZ)
        freq = MUSIC_MAX_FREQ_HZ;

    return (uint16_t)freq;
}

/*============================================================================*/
/* music_note_duration_ms                                                     */
/*============================================================================*/

uint32_t music_note_duration_ms(uint8_t duration, uint8_t dots, uint32_t bpm)
{
    if (duration == 0 || bpm == 0)
        return 250;

    /* Base: quarter note = 60000/BPM ms; scale by 4/duration */
    uint32_t base_ms = (60000UL * 4UL) / ((uint32_t)duration * bpm);

    /* Apply dots: each dot adds half the remaining duration */
    uint32_t dot_ms = base_ms;
    for (uint8_t i = 0; i < dots && i < 4; i++)
    {
        dot_ms /= 2;
        base_ms += dot_ms;
    }
    if (base_ms < 10)
        base_ms = 10;

    return base_ms;
}

/*============================================================================*/
/* music_parse_note_token                                                     */
/*============================================================================*/

bool music_parse_note_token(const char **cursor,
                            uint32_t def_dur, uint32_t def_oct,
                            MusicNote_t *note_out)
{
    const char *p = *cursor;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t') p++;

    if (*p == '\0' || *p == ',')
        return false;

    /* --- duration (optional leading number) --- */
    uint32_t duration = 0;
    while (isdigit((unsigned char)*p))
    {
        duration = duration * 10 + (uint32_t)(*p - '0');
        p++;
    }

    /* --- note letter --- */
    if (*p == '\0' || *p == ',')
        return false;

    char note_char = (char)toupper((unsigned char)*p);
    if ((note_char < 'A' || note_char > 'G') && note_char != 'P')
        return false;
    p++;

    /* --- sharp --- */
    bool is_sharp = false;
    if (*p == '#' || *p == '_')
    {
        is_sharp = true;
        p++;
    }

    /* --- octave (optional number after note) --- */
    uint32_t octave = 0;
    bool has_octave = false;
    if (isdigit((unsigned char)*p))
    {
        while (isdigit((unsigned char)*p))
        {
            octave = octave * 10 + (uint32_t)(*p - '0');
            p++;
        }
        has_octave = true;
    }

    /* --- dots --- */
    uint32_t dots = 0;
    while (*p == '.')
    {
        dots++;
        p++;
    }

    /* Apply defaults */
    if (duration == 0) duration = def_dur ? def_dur : 4;
    if (!has_octave)   octave   = def_oct ? def_oct : 4;

    /* Clamp */
    if (duration < 1)  duration = 1;
    if (duration > 32) duration = 32;
    if (octave   > 8)  octave   = 8;
    if (dots     > 4)  dots     = 4;

    /* Build output */
    if (note_char == 'P')
    {
        note_out->semitone = MUSIC_SEMITONE_PAUSE;
    }
    else
    {
        static const int8_t semitone_offset[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
        uint8_t letter_idx = (uint8_t)(note_char - 'A');
        int8_t  base = semitone_offset[letter_idx];
        note_out->semitone = (uint8_t)((int32_t)octave * 12 + base + (is_sharp ? 1 : 0));
    }
    note_out->duration = (uint8_t)duration;
    note_out->dots     = (uint8_t)dots;

    /* Advance cursor past this token */
    *cursor = p;
    return true;
}
