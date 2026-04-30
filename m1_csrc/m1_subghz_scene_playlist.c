/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_playlist.c
 * @brief  Sub-GHz Playlist Scene — sequential .sub file transmitter.
 *
 * Reads a plain-text playlist (.txt) from /SUBGHZ/playlist/ containing
 * one "sub: <path>" entry per line, then transmits each referenced .sub
 * file in order using sub_ghz_replay_flipper_file().
 *
 * Playlist format (one entry per line):
 *   sub: /SUBGHZ/Tesla_charge_AM270.sub
 *   sub: /SUBGHZ/Tesla_charge_AM650.sub
 *
 * Lines not starting with "sub:" are ignored (comments, blank lines).
 * Flipper-style paths (/ext/subghz/...) are automatically remapped to
 * /SUBGHZ/... for compatibility with UberGuidoZ playlist collections.
 *
 * Controls:
 *   OK    = start / pause playback
 *   L/R   = adjust repeat count (1–9, then ∞)
 *   BACK  = stop playback & return to menu
 *   U/D   = scroll file list (when idle)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "ff.h"
#include "app_freertos.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_storage.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"
#include "m1_sub_ghz.h"
#include "subghz_playlist_parser.h"
#include "flipper_subghz.h"

/* sub_ghz_replay_flipper_file() and sub_ghz_replay_datafile() are declared in m1_sub_ghz.h */

/*============================================================================*/
/* Constants                                                                  */
/*============================================================================*/

#define PLAYLIST_DIR        "/SUBGHZ/playlist"
#define PLAYLIST_MAX_FILES  16
#define PLAYLIST_PATH_MAX   64
#define PLAYLIST_LINE_MAX   256

/** Playlist list area: progress bar sits at y=46, so list spans y=12..45 = 34px */
#define PLAYLIST_LIST_AREA_H   34

/** Maximum visible items in the file list (font-aware) */
#define PLAYLIST_LIST_VISIBLE  ((uint8_t)(PLAYLIST_LIST_AREA_H / m1_menu_item_h()))

/*============================================================================*/
/* Local state                                                                */
/*============================================================================*/

static uint8_t list_scroll = 0;     /**< Top visible item in file list */
static bool    browse_done = false; /**< File selected, in playback view */

/*============================================================================*/
/* Playlist parser                                                            */
/*============================================================================*/

/* remap_flipper_path() has been extracted to subghz_playlist_parser.c/h
 * as subghz_remap_flipper_path().  Included via subghz_playlist_parser.h. */

/**
 * @brief  Parse a playlist .txt file and populate app->playlist_files[].
 *
 * @param  app   Application context (playlist fields updated)
 * @param  path  Full FatFS path to the playlist .txt (e.g. "/SUBGHZ/playlist/Tesla.txt")
 * @retval true  At least one entry was loaded
 * @retval false Parse failed or file empty
 */
static bool playlist_parse(SubGhzApp *app, const char *path)
{
    FIL fil;
    char line[PLAYLIST_LINE_MAX];

    app->playlist_count = 0;

    /* Clear any leftover delay values from a previously loaded playlist */
    for (uint8_t i = 0; i < PLAYLIST_MAX_FILES; i++)
        app->playlist_delays[i] = 0;

    /* Create playlist directory on demand */
    f_mkdir("/SUBGHZ");
    f_mkdir(PLAYLIST_DIR);

    if (f_open(&fil, path, FA_READ) != FR_OK)
        return false;

    while (f_gets(line, sizeof(line), &fil))
    {
        if (app->playlist_count >= PLAYLIST_MAX_FILES)
            break;

        /* Strip trailing CR/LF */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
            line[--len] = '\0';

        /* Skip blank lines */
        if (len == 0)
            continue;

        /* Check for delay directive: "# delay: <ms>"
         * The delay applies to the NEXT entry added after this line.
         * If multiple delay lines appear consecutively, the last one wins. */
        if (line[0] == '#')
        {
            uint16_t dms = 0;
            if (subghz_playlist_parse_delay(line, &dms))
            {
                /* Record as the delay for the next sub: entry */
                if (app->playlist_count < PLAYLIST_MAX_FILES)
                    app->playlist_delays[app->playlist_count] = dms;
            }
            continue;
        }

        /* Look for "sub:" prefix */
        if (strncmp(line, "sub:", 4) != 0)
            continue;

        /* Extract path — skip "sub:" and leading whitespace */
        const char *p = line + 4;
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            continue;

        /* Remap Flipper paths and store */
        subghz_remap_flipper_path(p,
                           app->playlist_files[app->playlist_count],
                           PLAYLIST_PATH_MAX);

        app->playlist_count++;
    }

    f_close(&fil);
    return (app->playlist_count > 0);
}

/*============================================================================*/
/* Playback                                                                   */
/*============================================================================*/

/**
 * @brief  Transmit the next file in the playlist.
 *
 * For M1 native NOISE .sgh files (produced by C3.12, SiN360, or Hapax),
 * uses sub_ghz_replay_datafile() to stream the original file directly —
 * no temp-file conversion needed.  For all other files (Flipper .sub RAW,
 * any PACKET/key file), falls back to sub_ghz_replay_flipper_file() which
 * handles the conversion transparently.
 *
 * Returns false when the playlist pass is finished.
 */
static bool playlist_transmit_next(SubGhzApp *app)
{
    if (app->playlist_current >= app->playlist_count)
        return false;

    const char *path = app->playlist_files[app->playlist_current];

    /* Probe file header to choose the most efficient replay path.
     * flipper_subghz_emulate_path() encodes the same decision used by the
     * saved scene's handle_action() so both callers always agree. */
    flipper_subghz_probe_t probe = {0};
    if (flipper_subghz_probe(path, &probe) &&
        flipper_subghz_emulate_path(probe.is_noise, probe.is_m1_native)
            == FLIPPER_SUBGHZ_EMULATE_DIRECT)
    {
        /* M1 native NOISE file — direct replay, no conversion */
        sub_ghz_replay_datafile(path, probe.frequency, probe.modulation);
    }
    else
    {
        /* Flipper .sub or PACKET/key file — convert via temp file */
        sub_ghz_replay_flipper_file(path);
    }

    /* sub_ghz_replay_*() calls menu_sub_ghz_exit() on return, which powers
     * off the SI4463 by deasserting ENA.  Restore the radio before the next
     * file transmits — otherwise recovery adds ~100ms latency per item. */
    menu_sub_ghz_init();

    /* Apply inter-signal delay (from "# delay: <ms>" playlist directive).
     * Delays are recorded on the next playlist entry during parsing, so
     * after transmitting the current signal we must consume the next
     * entry's delay, if there is a next entry.  This also guarantees
     * there is no delay after the final signal. */
    uint8_t next_index = app->playlist_current + 1;
    if (next_index < app->playlist_count)
    {
        uint16_t delay_ms = app->playlist_delays[next_index];
        if (delay_ms > 0)
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    app->playlist_current++;
    return true;
}

/**
 * @brief  Run one complete pass of the playlist.
 *
 * Returns false if playback was stopped (app->playlist_running cleared).
 */
static bool playlist_run_pass(SubGhzApp *app)
{
    app->playlist_current = 0;

    while (app->playlist_running && app->playlist_current < app->playlist_count)
    {
        app->need_redraw = true;
        playlist_transmit_next(app);
    }

    return app->playlist_running;
}

/*============================================================================*/
/* Helper: extract filename from path                                         */
/*============================================================================*/

static const char *basename_from_path(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/**
 * @brief  Open the playlist directory browser, parse the selected file.
 *
 * @retval true   A valid playlist was loaded.
 * @retval false  User cancelled or parse failed (caller should pop).
 */
static bool open_playlist_browser(SubGhzApp *app)
{
    /* Ensure playlist directory exists */
    f_mkdir("/SUBGHZ");
    f_mkdir(PLAYLIST_DIR);

    S_M1_file_info *f_info = storage_browse("0:" PLAYLIST_DIR);
    if (f_info && f_info->file_is_selected)
    {
        snprintf(app->playlist_path, sizeof(app->playlist_path),
                 PLAYLIST_DIR "/%s", f_info->file_name);

        if (playlist_parse(app, app->playlist_path))
        {
            browse_done = true;
            list_scroll = 0;
            return true;
        }
    }
    return false;
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    browse_done = false;
    list_scroll = 0;
    app->playlist_count = 0;
    app->playlist_current = 0;
    app->playlist_running = false;
    app->playlist_repeat_total = 1;
    app->playlist_repeat_done = 0;
    for (uint8_t i = 0; i < PLAYLIST_MAX_FILES; i++)
        app->playlist_delays[i] = 0;

    /* Open file browser immediately — no intermediate prompt screen */
    if (!open_playlist_browser(app))
    {
        subghz_scene_pop(app);
        return;
    }
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    /* Playback control mode (browse_done is always true when events arrive) */
    switch (event)
    {
        case SubGhzEventBack:
            app->playlist_running = false;
            /* Re-open file browser; pop if user cancels */
            if (!open_playlist_browser(app))
            {
                subghz_scene_pop(app);
                return true;
            }
            /* Reset playback state for the newly selected playlist */
            app->playlist_current = 0;
            app->playlist_repeat_done = 0;
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
        {
            if (!app->playlist_running)
            {
                /* Start playback */
                app->playlist_running = true;
                app->playlist_repeat_done = 0;
                app->need_redraw = true;

                /* Run playlist passes */
                bool keep_going = true;
                while (keep_going && app->playlist_running)
                {
                    keep_going = playlist_run_pass(app);
                    if (!app->playlist_running)
                        break;

                    app->playlist_repeat_done++;
                    app->need_redraw = true;

                    /* Check repeat limit */
                    if (app->playlist_repeat_total > 0 &&
                        app->playlist_repeat_done >= app->playlist_repeat_total)
                    {
                        break;
                    }
                }

                app->playlist_running = false;
                app->playlist_current = 0;
                app->need_redraw = true;
            }
            else
            {
                /* Stop playback */
                app->playlist_running = false;
                app->need_redraw = true;
            }
            return true;
        }

        case SubGhzEventLeft:
            /* Decrease repeat count (min 1) */
            if (!app->playlist_running)
            {
                if (app->playlist_repeat_total == 0)
                    app->playlist_repeat_total = 9;
                else if (app->playlist_repeat_total > 1)
                    app->playlist_repeat_total--;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventRight:
            /* Increase repeat count (max 9, then infinite=0) */
            if (!app->playlist_running)
            {
                if (app->playlist_repeat_total > 0 &&
                    app->playlist_repeat_total < 9)
                    app->playlist_repeat_total++;
                else if (app->playlist_repeat_total >= 9)
                    app->playlist_repeat_total = 0; /* infinite */
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventUp:
            if (!app->playlist_running && list_scroll > 0)
            {
                list_scroll--;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventDown:
            if (!app->playlist_running &&
                list_scroll + PLAYLIST_LIST_VISIBLE < app->playlist_count)
            {
                list_scroll++;
                app->need_redraw = true;
            }
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    app->playlist_running = false;
}

static void draw(SubGhzApp *app)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Playback mode (file browser opens directly on enter/back) */
    {
        /* Playback mode */
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);

        /* Header: playlist name + repeat info */
        {
            const char *pname = basename_from_path(app->playlist_path);
            char header[28];
            /* Truncate name to fit */
            snprintf(header, sizeof(header), "%.18s", pname);
            u8g2_DrawStr(&m1_u8g2, 2, 8, header);
        }

        /* Repeat counter on right side of header */
        {
            char rep[12];
            if (app->playlist_repeat_total == 0)
            {
                if (app->playlist_running)
                    snprintf(rep, sizeof(rep), "R:%u/Inf",
                             app->playlist_repeat_done + 1);
                else
                    snprintf(rep, sizeof(rep), "R:Inf");
            }
            else
            {
                if (app->playlist_running)
                    snprintf(rep, sizeof(rep), "R:%u/%u",
                             app->playlist_repeat_done + 1,
                             app->playlist_repeat_total);
                else
                    snprintf(rep, sizeof(rep), "R:%u",
                             app->playlist_repeat_total);
            }
            u8g2_DrawStr(&m1_u8g2, 90, 8, rep);
        }

        u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

        /* File list */
        const uint8_t item_h   = m1_menu_item_h();
        const uint8_t text_ofs = item_h - 1;
        u8g2_SetFont(&m1_u8g2, m1_menu_font());
        for (uint8_t i = 0; i < PLAYLIST_LIST_VISIBLE &&
                            (list_scroll + i) < app->playlist_count; i++)
        {
            uint8_t idx = list_scroll + i;
            uint8_t y = M1_MENU_AREA_TOP + i * item_h;
            const char *fname = basename_from_path(app->playlist_files[idx]);

            /* Highlight currently transmitting file */
            if (app->playlist_running && idx == app->playlist_current)
            {
                u8g2_DrawRBox(&m1_u8g2, 0, y, M1_MENU_TEXT_W, item_h, 2);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            }

            /* File index + name (truncated) */
            char line[26];
            snprintf(line, sizeof(line), "%u.%.20s", idx + 1, fname);
            u8g2_DrawStr(&m1_u8g2, 2, y + text_ofs, line);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }

        /* Progress bar (below file list, above bottom bar at y=52) */
        {
            uint8_t bar_y = 46;
            u8g2_DrawFrame(&m1_u8g2, 2, bar_y, 124, 4);
            if (app->playlist_count > 0)
            {
                uint8_t fill = (uint8_t)(
                    (uint32_t)app->playlist_current * 122 / app->playlist_count);
                if (fill > 0)
                    u8g2_DrawBox(&m1_u8g2, 3, bar_y + 1, fill, 2);
            }
        }

        /* Bottom bar */
        if (app->playlist_running)
        {
            subghz_button_bar_draw(
                arrowleft_8x8, "Stop",
                NULL, NULL,
                NULL, NULL);
        }
        else
        {
            subghz_button_bar_draw(
                NULL, NULL,
                NULL, "R-/R+",
                NULL, "OK:Play");
        }
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_playlist_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
