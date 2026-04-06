/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_menu.c
 * @brief  Sub-GHz Menu Scene — entry point with Flipper-style item list.
 *
 * Displays a focused selection list with a scrollbar position indicator
 * on the right side.  No bottom button bar — OK is implied by the list.
 *
 *   1. Read (protocol decode) — most common, at top
 *   2. Read Raw (raw capture)
 *   3. Saved (file browser)
 *   4. Playlist
 *   5. Frequency Analyzer
 *   6. Spectrum Analyzer
 *   7. RSSI Meter
 *   8. Freq Scanner
 *   9. Weather Station
 *  10. Brute Force
 *  11. Add Manually
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_subghz_scene.h"

/*============================================================================*/
/* Menu items                                                                 */
/*============================================================================*/

#define MENU_ITEM_COUNT   11
#define MENU_VISIBLE       6   /* Items visible at once (no button bar) */

/* Layout constants */
#define MENU_AREA_TOP     12   /* Y below title + separator line      */
#define MENU_ITEM_H        8   /* Pixel height per menu row           */
#define SCROLLBAR_X      125   /* Scrollbar left edge (3px wide)      */
#define SCROLLBAR_W        3   /* Scrollbar track width               */
#define MENU_TEXT_W      124   /* Highlight / text area width          */

static const char *menu_labels[MENU_ITEM_COUNT] = {
    "Read",
    "Read Raw",
    "Saved",
    "Playlist",
    "Frequency Analyzer",
    "Spectrum Analyzer",
    "RSSI Meter",
    "Freq Scanner",
    "Weather Station",
    "Brute Force",
    "Add Manually"
};

static const SubGhzSceneId menu_targets[MENU_ITEM_COUNT] = {
    SubGhzSceneRead,
    SubGhzSceneReadRaw,
    SubGhzSceneSaved,
    SubGhzScenePlaylist,
    SubGhzSceneFreqAnalyzer,
    SubGhzSceneSpectrumAnalyzer,
    SubGhzSceneRssiMeter,
    SubGhzSceneFreqScanner,
    SubGhzSceneWeatherStation,
    SubGhzSceneBruteForce,
    SubGhzSceneCount,        /* Add Manually handled separately */
};

static uint8_t menu_sel = 0;
static uint8_t menu_scroll = 0;

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    (void)app;
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
            menu_sel = (menu_sel > 0) ? menu_sel - 1 : MENU_ITEM_COUNT - 1;
            /* Adjust scroll window */
            if (menu_sel < menu_scroll)
                menu_scroll = menu_sel;
            else if (menu_sel >= menu_scroll + MENU_VISIBLE)
                menu_scroll = menu_sel - MENU_VISIBLE + 1;
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            menu_sel = (menu_sel + 1) % MENU_ITEM_COUNT;
            /* Adjust scroll window */
            if (menu_sel >= menu_scroll + MENU_VISIBLE)
                menu_scroll = menu_sel - MENU_VISIBLE + 1;
            /* Handle wrap-around */
            if (menu_sel == 0)
                menu_scroll = 0;
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
        {
            SubGhzSceneId target = menu_targets[menu_sel];
            if (target < SubGhzSceneCount)
            {
                subghz_scene_push(app, target);
            }
            else
            {
                /* Add Manually — handled externally for now.
                 * TODO: Create a dedicated AddManually scene. */
            }
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

static void draw(SubGhzApp *app)
{
    (void)app;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Sub-GHz", TEXT_ALIGN_CENTER);

    /* Draw separator line */
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* Menu items */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    for (uint8_t i = 0; i < MENU_VISIBLE && (menu_scroll + i) < MENU_ITEM_COUNT; i++)
    {
        uint8_t idx = menu_scroll + i;
        uint8_t y = MENU_AREA_TOP + i * MENU_ITEM_H;

        if (idx == menu_sel)
        {
            /* Highlight selected item (leave room for scrollbar) */
            u8g2_DrawBox(&m1_u8g2, 0, y, MENU_TEXT_W, MENU_ITEM_H);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }

        u8g2_DrawStr(&m1_u8g2, 4, y + 7, menu_labels[idx]);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    /* Scrollbar — position indicator on the right edge */
    {
        uint8_t sb_area_h  = MENU_VISIBLE * MENU_ITEM_H;
        uint8_t sb_handle_h = sb_area_h / MENU_ITEM_COUNT;
        uint8_t sb_handle_y = MENU_AREA_TOP +
            (uint8_t)((uint16_t)sb_area_h * menu_sel / MENU_ITEM_COUNT);

        /* Track outline */
        u8g2_DrawFrame(&m1_u8g2, SCROLLBAR_X, MENU_AREA_TOP,
                       SCROLLBAR_W, sb_area_h);
        /* Handle (filled) */
        u8g2_DrawBox(&m1_u8g2, SCROLLBAR_X, sb_handle_y,
                     SCROLLBAR_W, sb_handle_h);
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_menu_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
