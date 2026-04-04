/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_menu.c
 * @brief  Sub-GHz Menu Scene — entry point with streamlined item list.
 *
 * Replaces the 11-item submenu with a focused 5-item list:
 *   1. Read (protocol decode) — most common, at top
 *   2. Read Raw (raw capture)
 *   3. Saved (file browser)
 *   4. Frequency Analyzer
 *   5. Add Manually
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
/* Menu items                                                                 */
/*============================================================================*/

#define MENU_ITEM_COUNT    5
#define MENU_VISIBLE       5   /* All fit on screen */

static const char *menu_labels[MENU_ITEM_COUNT] = {
    "Read",
    "Read Raw",
    "Saved",
    "Frequency Analyzer",
    "Add Manually"
};

static const SubGhzSceneId menu_targets[MENU_ITEM_COUNT] = {
    SubGhzSceneRead,
    SubGhzSceneReadRaw,
    SubGhzSceneSaved,
    SubGhzSceneFreqAnalyzer,
    SubGhzSceneCount,        /* Add Manually handled separately */
};

static uint8_t menu_sel = 0;

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void on_enter(SubGhzApp *app)
{
    (void)app;
    app->need_redraw = true;
}

static bool on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            menu_sel = (menu_sel > 0) ? menu_sel - 1 : MENU_ITEM_COUNT - 1;
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            menu_sel = (menu_sel + 1) % MENU_ITEM_COUNT;
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

static void on_exit(SubGhzApp *app)
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
    m1_draw_text(&m1_u8g2, 2, 9, 124, "Sub-GHz", TEXT_ALIGN_CENTER);

    /* Draw separator line */
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* Menu items */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    for (uint8_t i = 0; i < MENU_ITEM_COUNT && i < MENU_VISIBLE; i++)
    {
        uint8_t y = 12 + i * 8;

        if (i == menu_sel)
        {
            /* Highlight selected item */
            u8g2_DrawBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH, 8);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }

        u8g2_DrawStr(&m1_u8g2, 4, y + 7, menu_labels[i]);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    /* Bottom bar */
    subghz_button_bar_draw(
        arrowleft_8x8, "Back",
        NULL, NULL,
        NULL, "OK");

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_menu_handlers = {
    .on_enter = on_enter,
    .on_event = on_event,
    .on_exit  = on_exit,
    .draw     = draw,
};
