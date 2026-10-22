/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_proto_pirate_models.c
 * @brief  Sub-GHz Proto Pirate — Models DB scene.
 *
 * Lists entries from the ProtoPirate car-models database
 * ("ProtoPirate Car Models Database", format version 2) stored on the SD
 * card at "/SUBGHZ/models_db/models.txt".  The file text is read line by
 * line into a bounded static buffer and parsed with the pure-logic parser
 * in Sub_Ghz/subghz_models_db.c.
 *
 * States:
 *   - LIST          : scrollable selection list of model names (submenu
 *                     model pattern, same as the Proto Pirate menu scene).
 *   - INFO_NO_DB    : file missing or zero usable entries — plain info
 *                     screen; any button pops the scene.
 *   - INFO_BAD_FREQ : selected entry's frequency is not within ±500 kHz of
 *                     any M1 frequency preset — plain info screen; any
 *                     button pops the scene.
 *
 * On OK (LIST state) the receiver preset is resolved and applied:
 *   freq_idx = subghz_freq_preset_find_near_hz(entry.frequency_hz, 500 kHz)
 *   subghz_apply_config_ext(freq_idx, entry.mod)
 * then SubGhzSceneRead is pushed with the ProtoPirate config filter, exactly
 * like the "Receiver" item in the Proto Pirate menu.  No radio start/stop
 * happens here — the Read scene owns the radio lifecycle.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "m1_submenu.h"
#include "subghz_submenu_model.h"
#include "flipper_file.h"
#include "subghz_models_db.h"
#include "subghz_freq_presets.h"

/* Radio config helper (defined in m1_sub_ghz.c) */
extern void subghz_apply_config_ext(uint8_t freq_idx, uint8_t mod_idx);

/*============================================================================*/
/* Constants                                                                  */
/*============================================================================*/

/** SD path of the ProtoPirate car-models database (v2 text format). */
#define MODELS_DB_PATH          "/SUBGHZ/models_db/models.txt"

/** Bounded static buffer for the raw file text fed to the parser.  The
 * shipped database is ~1.5 KB; 2 KB keeps headroom for a few extra entries
 * while bounding static RAM.  Content past the cap is not parsed. */
#define MODELS_TEXT_CAP         2048u

/** Frequency tolerance when mapping an entry to an M1 preset (±500 kHz). */
#define MODELS_FREQ_TOL_HZ      500000UL

/*============================================================================*/
/* Scene-local state                                                          */
/*============================================================================*/

typedef enum {
    MODELS_STATE_LIST = 0,
    MODELS_STATE_INFO_NO_DB,
    MODELS_STATE_INFO_BAD_FREQ,
} models_state_t;

/** Parsed database — file-static so row labels can point into it. */
static subghz_models_db_t s_db;

/** Raw file text buffer (bounded; parsing stops at the cap). */
static char s_text[MODELS_TEXT_CAP];

/** Per-row label array for m1_submenu_draw(); points into s_db entries. */
static const char *s_labels[SUBGHZ_MODELS_DB_MAX_MODELS];

static subghz_submenu_model_t s_model;
static models_state_t s_state;

/** Name shown on the unsupported-frequency info screen (points into s_db). */
static const char *s_info_name;

/*============================================================================*/
/* Selection state (same pattern as the Proto Pirate menu scene)              */
/*============================================================================*/

static inline uint8_t models_get_saved_sel(const SubGhzApp *app)
{
    uint32_t s = subghz_scene_get_state(app, SubGhzSceneProtoPirateModels);
    uint8_t sel = (uint8_t)(s & 0xFFu);
    return (sel < s_db.count) ? sel : 0;
}

static inline void models_save_sel(SubGhzApp *app, uint8_t sel)
{
    subghz_scene_set_state(app, SubGhzSceneProtoPirateModels, (uint32_t)sel);
}

/*============================================================================*/
/* Load + parse                                                               */
/*============================================================================*/

/** Read the models file into s_text and parse it into s_db.
 *  Returns true when at least one usable entry was parsed. */
static bool models_load(void)
{
    flipper_file_t ff;
    if (!ff_open(&ff, MODELS_DB_PATH))
        return false;

    size_t len = 0;
    while (ff_read_line(&ff))
    {
        size_t line_len = strlen(ff.line);
        /* Stop appending at the cap (keep room for the trailing NUL). */
        if (len + line_len + 1u >= sizeof(s_text))
            break;
        memcpy(s_text + len, ff.line, line_len);
        len += line_len;
        s_text[len++] = '\n';
    }
    ff_close(&ff);

    if (!subghz_models_db_parse(s_text, len, &s_db) || s_db.count == 0)
        return false;

    for (uint8_t i = 0; i < s_db.count; i++)
        s_labels[i] = s_db.entries[i].name;

    return true;
}

/*============================================================================*/
/* Draw                                                                       */
/*============================================================================*/

static void draw_info(void)
{
    char line[24];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Header */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 9, "Models DB");
    u8g2_DrawHLine(&m1_u8g2, 0, 11, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    if (s_state == MODELS_STATE_INFO_NO_DB)
    {
        u8g2_DrawStr(&m1_u8g2, 2, 26, "No models DB found");
        u8g2_DrawStr(&m1_u8g2, 2, 40, "Install to SD:");
        u8g2_DrawStr(&m1_u8g2, 2, 50, "SUBGHZ/models_db/");
    }
    else /* MODELS_STATE_INFO_BAD_FREQ */
    {
        u8g2_DrawStr(&m1_u8g2, 2, 26, "Unsupported frequency");
        if (s_info_name != NULL)
        {
            snprintf(line, sizeof(line), "%.21s", s_info_name);
            u8g2_DrawStr(&m1_u8g2, 2, 40, line);
        }
    }

    m1_u8g2_nextpage();
}

static void draw(SubGhzApp *app)
{
    (void)app;
    if (s_state != MODELS_STATE_LIST)
    {
        draw_info();
        return;
    }
    subghz_submenu_model_set_visible_count(&s_model,
                                           M1_MENU_VIS(s_db.count));
    m1_submenu_draw(&s_model, "Models DB", s_labels);
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    if (!models_load())
    {
        s_state = MODELS_STATE_INFO_NO_DB;
        app->need_redraw = true;
        return;
    }

    s_state = MODELS_STATE_LIST;
    subghz_submenu_model_init(&s_model,
                              s_db.count,
                              M1_MENU_VIS(s_db.count));
    subghz_submenu_model_set_selected(&s_model, models_get_saved_sel(app));
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    /* Info screens are not selection lists — any button pops the scene. */
    if (s_state != MODELS_STATE_LIST)
    {
        switch (event)
        {
            case SubGhzEventBack:
            case SubGhzEventOk:
            case SubGhzEventLeft:
            case SubGhzEventRight:
            case SubGhzEventUp:
            case SubGhzEventDown:
                subghz_scene_pop(app);
                return true;
            default:
                return false;
        }
    }

    subghz_submenu_model_set_visible_count(&s_model,
                                           M1_MENU_VIS(s_db.count));
    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            subghz_submenu_model_up(&s_model);
            models_save_sel(app, s_model.selected);
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            subghz_submenu_model_down(&s_model);
            models_save_sel(app, s_model.selected);
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
        {
            const subghz_model_entry_t *entry = &s_db.entries[s_model.selected];
            int16_t freq_idx =
                subghz_freq_preset_find_near_hz(entry->frequency_hz,
                                                MODELS_FREQ_TOL_HZ);
            if (freq_idx < 0)
            {
                s_info_name = entry->name;
                s_state = MODELS_STATE_INFO_BAD_FREQ;
                app->need_redraw = true;
                return true;
            }

            /* Apply the entry's receiver preset, then enter the Read scene
             * with the ProtoPirate config filter (same flow as the "Receiver"
             * menu item).  The Read scene owns the radio lifecycle. */
            app->freq_idx = (uint8_t)freq_idx;
            app->mod_idx  = entry->mod;
            subghz_models_disable_hopping(&app->hopping);
            subghz_apply_config_ext((uint8_t)freq_idx, entry->mod);
            app->config_filter_mode = SubGhzConfigFilterProtoPirate;
            subghz_scene_push(app, SubGhzSceneRead);
            return true;
        }

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    (void)app;
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_proto_pirate_models_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
