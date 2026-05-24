/* See COPYING.txt for license details. */

/**
 * @file  test_music_parser.c
 * @brief Unit tests for the FMF music parsing pure-logic module.
 *
 * Tests music_semitone_to_freq(), music_note_duration_ms(), and
 * music_parse_note_token() extracted from music_player.c.
 */

#include "unity.h"
#include "music_parser.h"
#include <string.h>

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* music_semitone_to_freq                                                     */
/*============================================================================*/

void test_freq_pause(void)
{
    TEST_ASSERT_EQUAL(0, music_semitone_to_freq(MUSIC_SEMITONE_PAUSE));
}

void test_freq_a4_is_440(void)
{
    /* A4 = semitone 57: octave 4 * 12 + 9 (A offset) = 57 */
    TEST_ASSERT_EQUAL(440, music_semitone_to_freq(57));
}

void test_freq_c4_is_262(void)
{
    /* C4 = semitone 48: octave 4 * 12 + 0 = 48 */
    TEST_ASSERT_EQUAL(262, music_semitone_to_freq(48));
}

void test_freq_c5_is_524(void)
{
    /* C5 = semitone 60: freq = 262 << 1 = 524 */
    TEST_ASSERT_EQUAL(524, music_semitone_to_freq(60));
}

void test_freq_c3_is_131(void)
{
    /* C3 = semitone 36: freq = 262 >> 1 = 131 */
    TEST_ASSERT_EQUAL(131, music_semitone_to_freq(36));
}

void test_freq_low_clamped(void)
{
    /* C0 = semitone 0: freq = 262 >> 4 = 16 → clamped to 130 */
    TEST_ASSERT_EQUAL(MUSIC_MIN_FREQ_HZ, music_semitone_to_freq(0));
}

void test_freq_high_clamped(void)
{
    /* C8 = semitone 96: freq = 262 << 4 = 4192 */
    /* A8 = semitone 105: freq = 440 << 4 = 7040 */
    /* B8 = semitone 107: freq = 494 << 4 = 7904 */
    /* These are all within 16000, but let's test a very high value */
    uint16_t f = music_semitone_to_freq(107);
    TEST_ASSERT_LESS_OR_EQUAL(MUSIC_MAX_FREQ_HZ, f);
    TEST_ASSERT_GREATER_THAN(0, f);
}

void test_freq_octave_boundaries(void)
{
    /* Each octave should double the frequency */
    uint16_t c4 = music_semitone_to_freq(48);
    uint16_t c5 = music_semitone_to_freq(60);
    TEST_ASSERT_EQUAL(c4 * 2, c5);
}

void test_freq_all_notes_in_octave4(void)
{
    /* Verify all 12 notes in octave 4 produce reasonable frequencies */
    uint16_t prev = 0;
    for (uint8_t i = 0; i < 12; i++)
    {
        uint16_t f = music_semitone_to_freq(48 + i);
        TEST_ASSERT_GREATER_THAN(prev, f);
        prev = f;
    }
}

/*============================================================================*/
/* music_note_duration_ms                                                     */
/*============================================================================*/

void test_duration_quarter_at_120bpm(void)
{
    /* Quarter note at 120 BPM = 60000/120 = 500 ms */
    TEST_ASSERT_EQUAL(500, music_note_duration_ms(4, 0, 120));
}

void test_duration_whole_at_120bpm(void)
{
    /* Whole note at 120 BPM = 60000*4/(1*120) = 2000 ms */
    TEST_ASSERT_EQUAL(2000, music_note_duration_ms(1, 0, 120));
}

void test_duration_half_at_120bpm(void)
{
    /* Half note at 120 BPM = 60000*4/(2*120) = 1000 ms */
    TEST_ASSERT_EQUAL(1000, music_note_duration_ms(2, 0, 120));
}

void test_duration_eighth_at_120bpm(void)
{
    /* Eighth note at 120 BPM = 60000*4/(8*120) = 250 ms */
    TEST_ASSERT_EQUAL(250, music_note_duration_ms(8, 0, 120));
}

void test_duration_sixteenth_at_120bpm(void)
{
    /* 16th note at 120 BPM = 60000*4/(16*120) = 125 ms */
    TEST_ASSERT_EQUAL(125, music_note_duration_ms(16, 0, 120));
}

void test_duration_dotted_quarter(void)
{
    /* Dotted quarter at 120 BPM = 500 + 250 = 750 ms */
    TEST_ASSERT_EQUAL(750, music_note_duration_ms(4, 1, 120));
}

void test_duration_double_dotted(void)
{
    /* Double-dotted quarter at 120 BPM = 500 + 250 + 125 = 875 ms */
    TEST_ASSERT_EQUAL(875, music_note_duration_ms(4, 2, 120));
}

void test_duration_zero_bpm_fallback(void)
{
    TEST_ASSERT_EQUAL(250, music_note_duration_ms(4, 0, 0));
}

void test_duration_zero_duration_fallback(void)
{
    TEST_ASSERT_EQUAL(250, music_note_duration_ms(0, 0, 120));
}

void test_duration_minimum_clamp(void)
{
    /* Very fast: 32nd note at 300 BPM = 60000*4/(32*300) = 25 ms */
    uint32_t d = music_note_duration_ms(32, 0, 300);
    TEST_ASSERT_GREATER_OR_EQUAL(10, d);
}

/*============================================================================*/
/* music_parse_note_token                                                     */
/*============================================================================*/

void test_parse_simple_c(void)
{
    const char *s = "C";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    /* C at octave 4 = semitone 48 */
    TEST_ASSERT_EQUAL(48, note.semitone);
    TEST_ASSERT_EQUAL(4, note.duration);
    TEST_ASSERT_EQUAL(0, note.dots);
}

void test_parse_a4(void)
{
    const char *s = "A4";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 5, &note));
    /* A4 = semitone 57 (overrides default octave 5) */
    TEST_ASSERT_EQUAL(57, note.semitone);
}

void test_parse_duration_prefix(void)
{
    const char *s = "8C5";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(60, note.semitone); /* C5 */
    TEST_ASSERT_EQUAL(8, note.duration);
}

void test_parse_sharp(void)
{
    const char *s = "C#4";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    /* C#4 = semitone 49 */
    TEST_ASSERT_EQUAL(49, note.semitone);
}

void test_parse_underscore_sharp(void)
{
    /* FMF also uses _ as sharp indicator */
    const char *s = "F_4";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    /* F#4 = semitone 54 */
    TEST_ASSERT_EQUAL(54, note.semitone);
}

void test_parse_dotted(void)
{
    const char *s = "4C4.";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(48, note.semitone);
    TEST_ASSERT_EQUAL(4, note.duration);
    TEST_ASSERT_EQUAL(1, note.dots);
}

void test_parse_double_dotted(void)
{
    const char *s = "4C4..";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(2, note.dots);
}

void test_parse_pause(void)
{
    const char *s = "4P";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(MUSIC_SEMITONE_PAUSE, note.semitone);
    TEST_ASSERT_EQUAL(4, note.duration);
}

void test_parse_lowercase(void)
{
    const char *s = "c4";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(48, note.semitone);
}

void test_parse_leading_whitespace(void)
{
    const char *s = "  A4";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(57, note.semitone);
}

void test_parse_empty_string(void)
{
    const char *s = "";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_FALSE(music_parse_note_token(&cursor, 4, 4, &note));
}

void test_parse_comma_only(void)
{
    const char *s = ",";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_FALSE(music_parse_note_token(&cursor, 4, 4, &note));
}

void test_parse_invalid_letter(void)
{
    const char *s = "X4";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_FALSE(music_parse_note_token(&cursor, 4, 4, &note));
}

void test_parse_default_duration(void)
{
    const char *s = "C4";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 8, 4, &note));
    /* No duration prefix → uses default 8 */
    TEST_ASSERT_EQUAL(8, note.duration);
}

void test_parse_default_octave(void)
{
    const char *s = "C";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 5, &note));
    /* No octave → uses default 5; C5 = semitone 60 */
    TEST_ASSERT_EQUAL(60, note.semitone);
}

void test_parse_all_notes(void)
{
    /* Verify all note letters A-G parse without error */
    const char *notes[] = {"A4", "B4", "C4", "D4", "E4", "F4", "G4"};
    for (int i = 0; i < 7; i++)
    {
        const char *cursor = notes[i];
        MusicNote_t note = {0};
        TEST_ASSERT_TRUE_MESSAGE(music_parse_note_token(&cursor, 4, 4, &note),
                                 notes[i]);
    }
}

void test_parse_cursor_advance(void)
{
    /* Parse two notes separated by comma */
    const char *s = "C4,D4";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(48, note.semitone); /* C4 */

    /* cursor should now point at ',' */
    TEST_ASSERT_EQUAL(',', *cursor);

    /* Skip comma and parse next */
    cursor++;
    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(50, note.semitone); /* D4 */
}

void test_parse_complex_token(void)
{
    /* Full token: duration=16, note=F#, octave=5, double-dotted */
    const char *s = "16F#5..";
    const char *cursor = s;
    MusicNote_t note = {0};

    TEST_ASSERT_TRUE(music_parse_note_token(&cursor, 4, 4, &note));
    TEST_ASSERT_EQUAL(66, note.semitone); /* F#5 = 5*12+5+1 = 66 */
    TEST_ASSERT_EQUAL(16, note.duration);
    TEST_ASSERT_EQUAL(2, note.dots);
}

/*============================================================================*/
/* main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Frequency conversion */
    RUN_TEST(test_freq_pause);
    RUN_TEST(test_freq_a4_is_440);
    RUN_TEST(test_freq_c4_is_262);
    RUN_TEST(test_freq_c5_is_524);
    RUN_TEST(test_freq_c3_is_131);
    RUN_TEST(test_freq_low_clamped);
    RUN_TEST(test_freq_high_clamped);
    RUN_TEST(test_freq_octave_boundaries);
    RUN_TEST(test_freq_all_notes_in_octave4);

    /* Duration calculation */
    RUN_TEST(test_duration_quarter_at_120bpm);
    RUN_TEST(test_duration_whole_at_120bpm);
    RUN_TEST(test_duration_half_at_120bpm);
    RUN_TEST(test_duration_eighth_at_120bpm);
    RUN_TEST(test_duration_sixteenth_at_120bpm);
    RUN_TEST(test_duration_dotted_quarter);
    RUN_TEST(test_duration_double_dotted);
    RUN_TEST(test_duration_zero_bpm_fallback);
    RUN_TEST(test_duration_zero_duration_fallback);
    RUN_TEST(test_duration_minimum_clamp);

    /* Note token parsing */
    RUN_TEST(test_parse_simple_c);
    RUN_TEST(test_parse_a4);
    RUN_TEST(test_parse_duration_prefix);
    RUN_TEST(test_parse_sharp);
    RUN_TEST(test_parse_underscore_sharp);
    RUN_TEST(test_parse_dotted);
    RUN_TEST(test_parse_double_dotted);
    RUN_TEST(test_parse_pause);
    RUN_TEST(test_parse_lowercase);
    RUN_TEST(test_parse_leading_whitespace);
    RUN_TEST(test_parse_empty_string);
    RUN_TEST(test_parse_comma_only);
    RUN_TEST(test_parse_invalid_letter);
    RUN_TEST(test_parse_default_duration);
    RUN_TEST(test_parse_default_octave);
    RUN_TEST(test_parse_all_notes);
    RUN_TEST(test_parse_cursor_advance);
    RUN_TEST(test_parse_complex_token);

    return UNITY_END();
}
