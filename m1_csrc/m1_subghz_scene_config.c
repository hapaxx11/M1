/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_config.c
 * @brief  Sub-GHz Config Scene — frequency, modulation, hopping, sound settings.
 *
 * Navigation:
 *   UP/DOWN   = select item
 *   LEFT/RIGHT = change value (clearly shown with ◀▶ arrows)
 *   BACK      = exit config (always)
 *   OK        = change value (same as RIGHT)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"

/*============================================================================*/
/* Frequency & modulation preset tables (shared with m1_sub_ghz.c)            */
/* These are defined in m1_sub_ghz.c — we reference them via extern.          */
/*============================================================================*/

/* Frequency preset count & labels — must match m1_sub_ghz.c */
#define CFG_FREQ_COUNT  17
#define CFG_MOD_COUNT    4

/* We need these string labels for display — pull from the same source */
extern const char *subghz_freq_labels[CFG_FREQ_COUNT];
extern const char *subghz_mod_labels[CFG_MOD_COUNT];

/*============================================================================*/
/* Config items                                                               */
/*============================================================================*/

#define CFG_ITEMS      4
#define CFG_FREQUENCY  0
#define CFG_HOPPING    1
#define CFG_MODULATION 2
#define CFG_SOUND      3

static uint8_t cfg_sel = 0;

static const char *cfg_item_labels[CFG_ITEMS] = {
    "Frequency:",
    "Hopping:",
    "Modulation:",
    "Sound:",
};

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

static const char *get_value_text(SubGhzApp *app, uint8_t item)
{
    static char freq_buf[16];
    switch (item)
    {
        case CFG_FREQUENCY:
            if (subghz_freq_labels)
                return subghz_freq_labels[app->freq_idx];
            snprintf(freq_buf, sizeof(freq_buf), "Preset %d", app->freq_idx);
            return freq_buf;
        case CFG_HOPPING:
            return app->hopping ? "ON" : "OFF";
        case CFG_MODULATION:
            if (subghz_mod_labels)
                return subghz_mod_labels[app->mod_idx];
            return "?";
        case CFG_SOUND:
            return app->sound ? "ON" : "OFF";
        default:
            return "";
    }
}

static void change_value(SubGhzApp *app, uint8_t item, int8_t dir)
{
    switch (item)
    {
        case CFG_FREQUENCY:
            if (dir > 0)
                app->freq_idx = (app->freq_idx + 1) % CFG_FREQ_COUNT;
            else
                app->freq_idx = (app->freq_idx > 0) ?
                    app->freq_idx - 1 : CFG_FREQ_COUNT - 1;
            break;
        case CFG_HOPPING:
            app->hopping = !app->hopping;
            break;
        case CFG_MODULATION:
            if (dir > 0)
                app->mod_idx = (app->mod_idx + 1) % CFG_MOD_COUNT;
            else
                app->mod_idx = (app->mod_idx > 0) ?
                    app->mod_idx - 1 : CFG_MOD_COUNT - 1;
            break;
        case CFG_SOUND:
            app->sound = !app->sound;
            break;
    }
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    (void)app;
    cfg_sel = 0;
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            cfg_sel = (cfg_sel > 0) ? cfg_sel - 1 : CFG_ITEMS - 1;
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            cfg_sel = (cfg_sel + 1) % CFG_ITEMS;
            app->need_redraw = true;
            return true;

        case SubGhzEventRight:
        case SubGhzEventOk:
            change_value(app, cfg_sel, +1);
            app->need_redraw = true;
            return true;

        case SubGhzEventLeft:
            change_value(app, cfg_sel, -1);
            app->need_redraw = true;
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    (void)app;
}

static void draw(SubGhzApp *app)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 124, "Config", TEXT_ALIGN_CENTER);

    /* Config items */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    for (uint8_t i = 0; i < CFG_ITEMS; i++)
    {
        uint8_t y = 12 + i * 9;

        if (i == cfg_sel)
        {
            /* Highlight selected row */
            u8g2_DrawBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH, 9);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }

        /* Label on left */
        u8g2_DrawStr(&m1_u8g2, 4, y + 7, cfg_item_labels[i]);

        /* Value on right with ◀ ▶ arrows for selected item */
        const char *val = get_value_text(app, i);
        if (i == cfg_sel)
        {
            u8g2_DrawStr(&m1_u8g2, 62, y + 7, "<");
            u8g2_DrawStr(&m1_u8g2, 68, y + 7, val);
            uint8_t vw = u8g2_GetStrWidth(&m1_u8g2, val);
            u8g2_DrawStr(&m1_u8g2, 68 + vw + 2, y + 7, ">");
        }
        else
        {
            u8g2_DrawStr(&m1_u8g2, 68, y + 7, val);
        }

        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    /* Bottom bar */
    subghz_button_bar_draw(
        NULL, NULL,
        NULL, "LR:Chng",
        arrowdown_8x8, "Select");

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_config_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
