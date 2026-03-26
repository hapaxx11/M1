/* See COPYING.txt for license details. */

/*
 * music_player.c
 *
 * Flipper Music Format (.fmf) player for M1.
 *
 * Ported from Flipper Zero firmware (GPLv3):
 *   https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/music_player
 *   lib/music_worker/music_worker.c
 *
 * The note parsing algorithm (extract_number / extract_char / extract_sharp /
 * music_worker_parse_notes) is an independent re-implementation of the same
 * algorithm used in the Flipper music_worker.  The M1 implementation replaces
 * Furi OS primitives with FreeRTOS / STM32 HAL equivalents.
 *
 * M1 Project
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "stm32h5xx_hal.h"
#include "main.h"
#include "cmsis_os.h"

#include "music_player.h"
#include "m1_buzzer.h"
#include "m1_lcd.h"
#include "m1_display.h"
#include "m1_tasks.h"
#include "m1_lib.h"
#include "m1_file_browser.h"
#include "flipper_file.h"

/*************************** D E F I N E S ************************************/

#define MP_FILETYPE         "Flipper Music Format"
#define MP_SEMITONE_PAUSE   0xFF
#define MP_MAX_NOTES        512
#define MP_NOTE_GAP_MS      10    /* silence between notes (ms) */

/* Buzzer minimum working frequency (Hz) — determined by 16-bit TIM prescaler */
#define MP_MIN_FREQ_HZ      130

/************************** S T R U C T U R E S *******************************/

typedef struct {
    uint8_t semitone;   /* absolute semitone (0 = C0); 0xFF = pause */
    uint8_t duration;   /* 1/2/4/8/16/32 */
    uint8_t dots;       /* number of dots (0-3) */
} MusicNote_t;

/*************************** V A R I A B L E S ********************************/

/*
 * Frequencies (Hz) for notes C through B at octave 4 (equal temperament,
 * A4 = 440 Hz, rounded to nearest integer).
 *
 * Index = semitone within octave: 0=C 1=C# 2=D 3=D# 4=E 5=F 6=F# 7=G 8=G# 9=A 10=A# 11=B
 */
static const uint16_t note_freq_oct4[12] = {
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494
};

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static uint16_t semitone_to_freq(uint8_t semitone);
static uint32_t note_duration_ms(uint8_t duration, uint8_t dots, uint32_t bpm);
static bool     music_parse_fmf(const char *path,
                                uint32_t *bpm_out, uint32_t *def_dur_out,
                                uint32_t *def_oct_out,
                                MusicNote_t *notes, uint16_t *count_out);
static bool     music_play_file(const char *path);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/**
 * @brief  Convert an absolute semitone number to a buzzer frequency in Hz.
 *         Semitone 0 = C0, 48 = C4, 57 = A4 (440 Hz), etc.
 * @param  semitone  Absolute semitone index (0-107 for octaves 0-8)
 * @retval Frequency in Hz, or 0 for pause
 */
/*============================================================================*/
static uint16_t semitone_to_freq(uint8_t semitone)
{
    if (semitone == MP_SEMITONE_PAUSE)
        return 0;

    uint8_t octave   = semitone / 12;
    uint8_t note_idx = semitone % 12;

    uint32_t freq;
    if (octave >= 4)
        freq = (uint32_t)note_freq_oct4[note_idx] << (octave - 4);
    else
        freq = (uint32_t)note_freq_oct4[note_idx] >> (4 - octave);

    if (freq < MP_MIN_FREQ_HZ)
        freq = MP_MIN_FREQ_HZ;
    if (freq > 16000)
        freq = 16000;

    return (uint16_t)freq;
}

/*============================================================================*/
/**
 * @brief  Compute note duration in milliseconds.
 * @param  duration  Note value (1=whole, 2=half, 4=quarter, 8=eighth …)
 * @param  dots      Number of augmentation dots (each adds half of remainder)
 * @param  bpm       Beats per minute (quarter note = 1 beat)
 * @retval Duration in milliseconds
 */
/*============================================================================*/
static uint32_t note_duration_ms(uint8_t duration, uint8_t dots, uint32_t bpm)
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
/**
 * @brief  Parse an FMF note string token.
 *         Format: [duration] NOTE [#] [octave] [....]
 *         Token ends at the next comma or end of string.
 * @param  cursor     Pointer into the Notes value string (updated on return)
 * @param  def_dur    Default duration if token has none
 * @param  def_oct    Default octave if token has none
 * @param  note_out  Output note struct
 * @retval true if a valid note was parsed, false otherwise
 */
/*============================================================================*/
static bool parse_note_token(const char **cursor,
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
        note_out->semitone = MP_SEMITONE_PAUSE;
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

/*============================================================================*/
/**
 * @brief  Parse an FMF file and populate the notes array.
 * @param  path       Full SD card path (e.g. "0:/Music/song.fmf")
 * @param  bpm_out    Parsed BPM
 * @param  def_dur_out  Parsed default duration
 * @param  def_oct_out  Parsed default octave
 * @param  notes      Caller-allocated array of MusicNote_t
 * @param  count_out  Number of notes parsed
 * @retval true on success, false on parse error
 */
/*============================================================================*/
static bool music_parse_fmf(const char *path,
                             uint32_t *bpm_out, uint32_t *def_dur_out,
                             uint32_t *def_oct_out,
                             MusicNote_t *notes, uint16_t *count_out)
{
    flipper_file_t ff;
    bool ok = false;

    *count_out = 0;
    *bpm_out   = 120;
    *def_dur_out = 4;
    *def_oct_out = 4;

    if (!ff_open(&ff, path))
        return false;

    /* Validate header */
    if (!ff_validate_header(&ff, MP_FILETYPE, 0))
        goto done;

    /* Read BPM */
    if (!ff_read_line(&ff) || !ff_parse_kv(&ff) ||
        strcmp(ff_get_key(&ff), "BPM") != 0)
        goto done;
    *bpm_out = (uint32_t)atoi(ff_get_value(&ff));
    if (*bpm_out == 0) *bpm_out = 120;

    /* Read Duration */
    if (!ff_read_line(&ff) || !ff_parse_kv(&ff) ||
        strcmp(ff_get_key(&ff), "Duration") != 0)
        goto done;
    *def_dur_out = (uint32_t)atoi(ff_get_value(&ff));
    if (*def_dur_out == 0) *def_dur_out = 4;

    /* Read Octave */
    if (!ff_read_line(&ff) || !ff_parse_kv(&ff) ||
        strcmp(ff_get_key(&ff), "Octave") != 0)
        goto done;
    *def_oct_out = (uint32_t)atoi(ff_get_value(&ff));

    /* Read Notes */
    if (!ff_read_line(&ff) || !ff_parse_kv(&ff) ||
        strcmp(ff_get_key(&ff), "Notes") != 0)
        goto done;

    {
        const char *cursor = ff_get_value(&ff);
        uint32_t def_dur = *def_dur_out;
        uint32_t def_oct = *def_oct_out;

        while (*cursor != '\0' && *count_out < MP_MAX_NOTES)
        {
            /* Skip whitespace */
            while (*cursor == ' ' || *cursor == '\t') cursor++;

            if (*cursor == '\0') break;

            MusicNote_t note;
            if (parse_note_token(&cursor, def_dur, def_oct, &note))
            {
                notes[*count_out] = note;
                (*count_out)++;
            }

            /* Advance past comma separator */
            while (*cursor == ' ' || *cursor == '\t') cursor++;
            if (*cursor == ',') cursor++;
        }
    }

    ok = (*count_out > 0);

done:
    ff_close(&ff);
    return ok;
}

/*============================================================================*/
/**
 * @brief  Play one FMF file.  Returns when playback is complete or the user
 *         presses BACK.
 * @param  path  Full path to the .fmf file
 * @retval true if playback completed normally, false if aborted or parse error
 */
/*============================================================================*/
static bool music_play_file(const char *path)
{
    MusicNote_t *notes = pvPortMalloc(MP_MAX_NOTES * sizeof(MusicNote_t));
    if (!notes)
        return false;

    uint32_t bpm, def_dur, def_oct;
    uint16_t note_count = 0;

    bool parsed = music_parse_fmf(path, &bpm, &def_dur, &def_oct, notes, &note_count);
    if (!parsed || note_count == 0)
    {
        vPortFree(notes);
        m1_message_box(&m1_u8g2, "Music Player", "Cannot parse", path, " OK ");
        return false;
    }

    /* Extract short display name (last path component, no extension) */
    const char *fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;
    char dispname[24];
    strncpy(dispname, fname, sizeof(dispname) - 1);
    dispname[sizeof(dispname) - 1] = '\0';
    char *dot = strrchr(dispname, '.');
    if (dot) *dot = '\0';

    S_M1_Buttons_Status bs;
    S_M1_Main_Q_t       q_item;
    BaseType_t          ret;
    bool                aborted = false;
    char                note_str[8];

    for (uint16_t i = 0; i < note_count && !aborted; i++)
    {
        MusicNote_t *n = &notes[i];
        uint32_t dur_ms = note_duration_ms(n->duration, n->dots, bpm);
        uint16_t freq   = semitone_to_freq(n->semitone);

        /* Build note label for display */
        if (n->semitone == MP_SEMITONE_PAUSE)
        {
            snprintf(note_str, sizeof(note_str), "Rest");
        }
        else
        {
            static const char *note_names[12] = {
                "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
            };
            uint8_t oct = n->semitone / 12;
            uint8_t idx = n->semitone % 12;
            snprintf(note_str, sizeof(note_str), "%s%u", note_names[idx], oct);
        }

        /* Progress bar (0-122 px) */
        uint8_t progress = (uint8_t)(((uint32_t)i * 122UL) / (uint32_t)note_count);

        /* Draw display */
        u8g2_FirstPage(&m1_u8g2);
        do {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 2, 10, "Music Player");
            u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 2, 22, dispname);

            /* Note and duration */
            u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
            u8g2_DrawStr(&m1_u8g2, 2, 38, note_str);

            /* Progress bar */
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawFrame(&m1_u8g2, 3, 50, 124, 8);
            if (progress > 0)
                u8g2_DrawBox(&m1_u8g2, 4, 51, progress, 6);

            u8g2_DrawStr(&m1_u8g2, 2, 64, "BACK to stop");
        } while (u8g2_NextPage(&m1_u8g2));

        /* Start note or silence */
        if (freq > 0)
            m1_buzzer_tone_start(freq);
        /* (silence: buzzer already stopped from previous note) */

        /* Wait for note duration, checking for BACK */
        uint32_t elapsed_ms = 0;
        uint32_t poll_ms    = 20;   /* poll interval */
        while (elapsed_ms < dur_ms && !aborted)
        {
            uint32_t wait = (dur_ms - elapsed_ms > poll_ms) ? poll_ms : (dur_ms - elapsed_ms);
            ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(wait));
            elapsed_ms += wait;

            if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
            {
                ret = xQueueReceive(button_events_q_hdl, &bs, 0);
                if (ret == pdTRUE &&
                    bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    aborted = true;
                }
            }
        }

        /* Stop tone and insert gap */
        m1_buzzer_tone_stop();
        if (!aborted)
            vTaskDelay(pdMS_TO_TICKS(MP_NOTE_GAP_MS));
    }

    /* End screen */
    if (!aborted)
    {
        u8g2_FirstPage(&m1_u8g2);
        do {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Music Player");
            u8g2_DrawStr(&m1_u8g2, 2, 40, dispname);
            u8g2_DrawStr(&m1_u8g2, 2, 56, "Finished.");
        } while (u8g2_NextPage(&m1_u8g2));
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    vPortFree(notes);
    return !aborted;
}

/*============================================================================*/
/**
 * @brief  Music Player entry point.
 *         Presents a file browser rooted at SD:/Music, lets the user choose
 *         a .fmf file, plays it, then returns to the browser.  Pressing BACK
 *         in the browser exits to the main menu.
 */
/*============================================================================*/
void music_player_run(void)
{
    S_M1_Buttons_Status bs;
    S_M1_Main_Q_t       q_item;
    BaseType_t          ret;
    char                filepath[128];

    /* Ensure directory exists */
    if (!m1_fb_check_existence(MUSIC_PLAYER_DIR))
        m1_fb_make_dir(MUSIC_PLAYER_DIR);

    /* Init file browser rooted at Music directory */
    S_M1_file_browser_hdl *fb = m1_fb_init(&m1_u8g2);

    free(fb->info.dir_name);
    fb->info.dir_name = malloc(strlen(MUSIC_PLAYER_DIR) + 1);
    strcpy(fb->info.dir_name, MUSIC_PLAYER_DIR);
    fb->dir_level = 1;
    fb->listing_index_buffer = realloc(fb->listing_index_buffer, 2 * sizeof(uint16_t));
    fb->row_index_buffer     = realloc(fb->row_index_buffer,     2 * sizeof(uint16_t));
    fb->listing_index_buffer[0] = 0;
    fb->row_index_buffer[0]     = 0;
    fb->listing_index_buffer[1] = 0;
    fb->row_index_buffer[1]     = 0;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    m1_u8g2_nextpage();

    m1_fb_display(NULL);

    /* File browser event loop */
    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE)          continue;
        if (q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

        ret = xQueueReceive(button_events_q_hdl, &bs, 0);
        if (ret != pdTRUE)          continue;

        /* BACK exits to main menu */
        if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            m1_fb_deinit();
            break;
        }

        /* Pass button to browser */
        S_M1_file_info *f_info = m1_fb_display(&bs);
        if (f_info->status == FB_OK && f_info->file_is_selected)
        {
            snprintf(filepath, sizeof(filepath), "%s/%s",
                     f_info->dir_name, f_info->file_name);

            /* Play the selected file */
            music_play_file(filepath);

            /* Return to file browser */
            m1_fb_display(NULL);
        }
    }

    xQueueReset(main_q_hdl);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
