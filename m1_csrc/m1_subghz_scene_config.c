/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_config.c
 * @brief  Sub-GHz Config Scene — frequency, modulation, hopping, sound,
 *         TX power, and ISM region settings.
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
#include "m1_scene.h"
#include "m1_sub_ghz.h"
#include "m1_subghz_scene.h"
#include "m1_settings.h"
#include "m1_system.h"

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

/* TX power accessors (static data in m1_sub_ghz.c) */
extern uint8_t     subghz_get_tx_power_idx_ext(void);
extern void        subghz_set_tx_power_idx_ext(uint8_t idx);
extern const char *subghz_get_tx_power_label_ext(uint8_t idx);
extern uint8_t     subghz_get_tx_power_count_ext(void);

/* ISM region data (non-static globals in m1_sub_ghz.c / m1_sub_ghz.h) */
extern const char *subghz_ism_regions_text[];

/*============================================================================*/
/* Config items                                                               */
/*============================================================================*/

#define CFG_ITEMS      6
#define CFG_FREQUENCY  0
#define CFG_HOPPING    1
#define CFG_MODULATION 2
#define CFG_SOUND      3
#define CFG_TX_POWER   4
#define CFG_ISM_REGION 5

static uint8_t cfg_sel = 0;
static uint8_t cfg_scroll = 0;  /* First visible item (scrolling if items > visible) */

/* Layout constants — aligned with menu scene (no button bar) */
#define CFG_AREA_TOP     12   /* Y below title + separator line      */
#define CFG_TEXT_W      124   /* Highlight / text area width           */
#define CFG_SCROLLBAR_X 125   /* Scrollbar left edge (3px wide)       */
#define CFG_SCROLLBAR_W   3   /* Scrollbar track width                */

static const char *cfg_item_labels[CFG_ITEMS] = {
    "Frequency:",
    "Hopping:",
    "Modulation:",
    "Sound:",
    "TX Power:",
    "ISM Region:",
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
        case CFG_TX_POWER:
            return subghz_get_tx_power_label_ext(subghz_get_tx_power_idx_ext());
        case CFG_ISM_REGION:
            return subghz_ism_regions_text[m1_device_stat.config.ism_band_region];
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
        case CFG_TX_POWER:
        {
            uint8_t tx_count = subghz_get_tx_power_count_ext();
            uint8_t tx_idx   = subghz_get_tx_power_idx_ext();
            if (dir > 0)
                tx_idx = (tx_idx + 1) % tx_count;
            else
                tx_idx = (tx_idx > 0) ? tx_idx - 1 : tx_count - 1;
            subghz_set_tx_power_idx_ext(tx_idx);
            break;
        }
        case CFG_ISM_REGION:
            if (dir > 0)
            {
                m1_device_stat.config.ism_band_region++;
                if (m1_device_stat.config.ism_band_region >= SUBGHZ_ISM_BAND_REGIONS_LIST)
                    m1_device_stat.config.ism_band_region = 0;
            }
            else
            {
                if (m1_device_stat.config.ism_band_region > 0)
                    m1_device_stat.config.ism_band_region--;
                else
                    m1_device_stat.config.ism_band_region = SUBGHZ_ISM_BAND_REGIONS_LIST - 1;
            }
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
    cfg_scroll = 0;
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            /* Persist ISM region change to SD */
            settings_save_to_sd();
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            cfg_sel = (cfg_sel > 0) ? cfg_sel - 1 : CFG_ITEMS - 1;
            /* Adjust scroll window */
            if (cfg_sel < cfg_scroll)
                cfg_scroll = cfg_sel;
            if (cfg_sel == CFG_ITEMS - 1)
            {
                uint8_t vis = M1_MENU_VIS(CFG_ITEMS);
                cfg_scroll = (CFG_ITEMS > vis) ? CFG_ITEMS - vis : 0;
            }
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            cfg_sel = (cfg_sel + 1) % CFG_ITEMS;
            /* Adjust scroll window */
            {
                uint8_t vis = M1_MENU_VIS(CFG_ITEMS);
                if (cfg_sel >= cfg_scroll + vis)
                    cfg_scroll = cfg_sel - vis + 1;
            }
            if (cfg_sel == 0)
                cfg_scroll = 0;
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
    const uint8_t item_h   = m1_menu_item_h();
    const uint8_t text_ofs = item_h - 1;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Config", TEXT_ALIGN_CENTER);

    /* Separator line (consistent with menu scene) */
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* Config items (scrollable) */
    u8g2_SetFont(&m1_u8g2, m1_menu_font());

    uint8_t visible = M1_MENU_VIS(CFG_ITEMS);
    for (uint8_t v = 0; v < visible; v++)
    {
        uint8_t i = cfg_scroll + v;
        if (i >= CFG_ITEMS) break;

        uint8_t y = CFG_AREA_TOP + v * item_h;

        if (i == cfg_sel)
        {
            /* Highlight selected row (leave room for scrollbar) */
            u8g2_DrawBox(&m1_u8g2, 0, y, CFG_TEXT_W, item_h);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }

        /* Label on left */
        u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, cfg_item_labels[i]);

        /* Value on right with ◀ ▶ arrows for selected item */
        const char *val = get_value_text(app, i);
        if (i == cfg_sel)
        {
            u8g2_DrawStr(&m1_u8g2, 62, y + text_ofs, "<");
            u8g2_DrawStr(&m1_u8g2, 68, y + text_ofs, val);
            uint8_t vw = u8g2_GetStrWidth(&m1_u8g2, val);
            u8g2_DrawStr(&m1_u8g2, 68 + vw + 2, y + text_ofs, ">");
        }
        else
        {
            u8g2_DrawStr(&m1_u8g2, 68, y + text_ofs, val);
        }

        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    /* Scrollbar — only when items exceed visible area */
    if (CFG_ITEMS > visible)
    {
        uint8_t sb_area_h   = visible * item_h;
        uint8_t sb_handle_h = sb_area_h / CFG_ITEMS;
        if (sb_handle_h < 3) sb_handle_h = 3;
        uint8_t sb_handle_y = CFG_AREA_TOP +
            (uint8_t)((uint16_t)sb_area_h * cfg_sel / CFG_ITEMS);

        u8g2_DrawFrame(&m1_u8g2, CFG_SCROLLBAR_X, CFG_AREA_TOP,
                       CFG_SCROLLBAR_W, sb_area_h);
        u8g2_DrawBox(&m1_u8g2, CFG_SCROLLBAR_X, sb_handle_y,
                     CFG_SCROLLBAR_W, sb_handle_h);
    }

    /* No button bar — the < > arrows on selected items clearly indicate
     * that LEFT/RIGHT changes the value.  UP/DOWN is self-evident. */

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
