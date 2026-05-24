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
 *  12. Remote
 *  13. Bind Remote
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_system.h"
#include "m1_subghz_scene.h"

/*============================================================================*/
/* Menu items                                                                 */
/*============================================================================*/

#define MENU_ITEM_COUNT   13

/* Layout constants */
#define MENU_AREA_TOP     12   /* Y below title + separator line      */
#define SCROLLBAR_X      124   /* Scrollbar left edge (3px wide)      */
#define SCROLLBAR_W        3   /* Scrollbar track width               */
#define MENU_TEXT_W      122   /* Highlight / text area width          */

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
    "Add Manually",
    "Remote",
    "Bind Remote",
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
    SubGhzSceneAddManually,
    SubGhzSceneRemote,
    SubGhzSceneBindWizard,
};

/*============================================================================*/
/* Cursor state — packed into the per-scene 32-bit slot                       */
/* (Phase 2 of the Momentum-parity migration: file-scope statics replaced     */
/* with subghz_scene_get_state/set_state so the cursor survives child-scene   */
/* pushes/pops without smelly globals.)                                       */
/*============================================================================*/

/* Layout in the slot:  [bits 0..7 = sel] [bits 8..15 = scroll] */
#define MENU_STATE_PACK(sel, scroll) \
    (((uint32_t)((scroll) & 0xFFu) << 8) | (uint32_t)((sel) & 0xFFu))
#define MENU_STATE_SEL(s)     ((uint8_t)((s) & 0xFFu))
#define MENU_STATE_SCROLL(s)  ((uint8_t)(((s) >> 8) & 0xFFu))

static inline uint8_t menu_get_sel(const SubGhzApp *app)
{
    uint8_t s = MENU_STATE_SEL(subghz_scene_get_state(app, SubGhzSceneMenu));
    return (s < MENU_ITEM_COUNT) ? s : 0;
}

static inline uint8_t menu_get_scroll(const SubGhzApp *app)
{
    uint8_t s = MENU_STATE_SCROLL(subghz_scene_get_state(app, SubGhzSceneMenu));
    return (s < MENU_ITEM_COUNT) ? s : 0;
}

static inline void menu_store(SubGhzApp *app, uint8_t sel, uint8_t scroll)
{
    subghz_scene_set_state(app, SubGhzSceneMenu, MENU_STATE_PACK(sel, scroll));
}

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
    const uint8_t vis = M1_MENU_VIS(MENU_ITEM_COUNT);
    uint8_t sel    = menu_get_sel(app);
    uint8_t scroll = menu_get_scroll(app);

    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            sel = (sel > 0) ? sel - 1 : MENU_ITEM_COUNT - 1;
            /* Adjust scroll window */
            if (sel < scroll)
                scroll = sel;
            else if (sel >= scroll + vis)
                scroll = sel - vis + 1;
            menu_store(app, sel, scroll);
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            sel = (sel + 1) % MENU_ITEM_COUNT;
            /* Adjust scroll window */
            if (sel >= scroll + vis)
                scroll = sel - vis + 1;
            /* Handle wrap-around */
            if (sel == 0)
                scroll = 0;
            menu_store(app, sel, scroll);
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
        {
            SubGhzSceneId target = menu_targets[sel];
            if (target < SubGhzSceneCount)
            {
                subghz_scene_push(app, target);
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
    const uint8_t item_h   = m1_menu_item_h();
    const uint8_t text_ofs = item_h - 1;
    const uint8_t vis      = M1_MENU_VIS(MENU_ITEM_COUNT);
    const uint8_t sel      = menu_get_sel(app);
    const uint8_t scroll   = menu_get_scroll(app);

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Sub-GHz", TEXT_ALIGN_CENTER);

    /* Draw separator line */
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* Menu items */
    u8g2_SetFont(&m1_u8g2, m1_menu_font());
    for (uint8_t i = 0; i < vis && (scroll + i) < MENU_ITEM_COUNT; i++)
    {
        uint8_t idx = scroll + i;
        uint8_t y = MENU_AREA_TOP + i * item_h;

        if (idx == sel)
        {
            /* Highlight selected item — rounded corners, leave room for scrollbar */
            u8g2_DrawRBox(&m1_u8g2, 1, y, MENU_TEXT_W, item_h, 2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }

        u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, menu_labels[idx]);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    /* Scrollbar — position indicator on the right edge */
    {
        uint8_t sb_area_h   = vis * item_h;
        uint8_t sb_handle_h = sb_area_h / MENU_ITEM_COUNT;
        if (sb_handle_h < 6) sb_handle_h = 6;

        uint8_t sb_travel_h = (sb_handle_h < sb_area_h) ? (sb_area_h - sb_handle_h) : 0;
        uint8_t sb_handle_y = MENU_AREA_TOP;
        if (MENU_ITEM_COUNT > 1)
        {
            sb_handle_y +=
                (uint8_t)((uint16_t)sb_travel_h * sel / (MENU_ITEM_COUNT - 1));
        }

        /* Track — single centerline pixel */
        u8g2_DrawVLine(&m1_u8g2, SCROLLBAR_X + SCROLLBAR_W / 2,
                       MENU_AREA_TOP, sb_area_h);
        /* Handle — filled rounded rectangle */
        u8g2_DrawRBox(&m1_u8g2, SCROLLBAR_X, sb_handle_y,
                      SCROLLBAR_W, sb_handle_h, 1);
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
