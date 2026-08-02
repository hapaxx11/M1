/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_protocol_filter.c
 * @brief  Sub-GHz Protocol Filter scene — ignore whole protocol groups.
 *
 * Momentum parity: instead of toggling protocols one by one, the user ignores
 * broad CATEGORIES (e.g. "Vehicles", "Gates") via a short On / Ignored list.
 * Every protocol that belongs to an ignored group is skipped by all Sub-GHz
 * reading features (Read, Read Raw, Decode Raw, Playlist decode, and the RF
 * Rosetta Signal ID / Smart ID decode stage) via subghz_ignore_is_ignored().
 *
 * Navigation:
 *   UP/DOWN         = select group
 *   OK / LEFT/RIGHT = toggle On ↔ Ignored for the selected group
 *   BACK            = persist the ignore set and return to Config
 *
 * The ignore set is held by the pure-logic subghz_protocol_ignore module;
 * this scene is only the UI.  Group membership is data-driven from the
 * protocol registry, so the list stays in sync when protocols are added.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "m1_settings.h"
#include "subghz_protocol_registry.h"
#include "subghz_protocol_ignore.h"

/*============================================================================*/
/* Scene state                                                                */
/*============================================================================*/

static uint16_t pf_sel = 0;     /* Selected group index                       */
static uint16_t pf_scroll = 0;  /* First visible row                          */

/* Layout constants — aligned with the Config scene (no button bar). */
#define PF_AREA_TOP     12
#define PF_TEXT_W      122
#define PF_SCROLLBAR_X 124
#define PF_SCROLLBAR_W   3

static uint16_t pf_count(void)
{
    return (uint16_t)SubGhzIgnoreGroupCount;
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    /* Restore the previously-selected row (per-scene 32-bit state slot). */
    uint32_t saved = subghz_scene_get_state(app, SubGhzSceneProtocolFilter);
    pf_sel = (saved < pf_count()) ? (uint16_t)saved : 0;

    uint8_t vis = M1_MENU_VIS(pf_count());
    if (pf_sel < vis)
        pf_scroll = 0;
    else
        pf_scroll = (pf_sel + 1 > vis) ? (uint16_t)(pf_sel + 1 - vis) : 0;

    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    const uint16_t count = pf_count();
    if (count == 0)
    {
        if (event == SubGhzEventBack)
        {
            subghz_scene_pop(app);
            return true;
        }
        return false;
    }

    switch (event)
    {
        case SubGhzEventBack:
            /* Persist the ignore list before leaving. */
            subghz_scene_set_state(app, SubGhzSceneProtocolFilter, pf_sel);
            settings_save_to_sd();
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            pf_sel = (pf_sel > 0) ? (uint16_t)(pf_sel - 1) : (uint16_t)(count - 1);
            {
                uint8_t vis = M1_MENU_VIS(count);
                if (pf_sel < pf_scroll)
                    pf_scroll = pf_sel;
                if (pf_sel == count - 1)
                    pf_scroll = (count > vis) ? (uint16_t)(count - vis) : 0;
            }
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            pf_sel = (uint16_t)((pf_sel + 1) % count);
            {
                uint8_t vis = M1_MENU_VIS(count);
                if (pf_sel >= pf_scroll + vis)
                    pf_scroll = (uint16_t)(pf_sel - vis + 1);
                if (pf_sel == 0)
                    pf_scroll = 0;
            }
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
        case SubGhzEventLeft:
        case SubGhzEventRight:
            subghz_ignore_group_toggle((SubGhzIgnoreGroup)pf_sel);
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
    (void)app;
    const uint16_t count    = pf_count();
    const uint8_t  item_h   = m1_menu_item_h();
    const uint8_t  text_ofs = item_h - 1;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title with ignored-group summary. */
    char title[24];
    snprintf(title, sizeof(title), "Ignore (%u off)",
             (unsigned)subghz_ignore_group_ignored_count());
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, title, TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, m1_menu_font());

    uint8_t visible = M1_MENU_VIS(count);
    for (uint8_t v = 0; v < visible; v++)
    {
        uint16_t i = (uint16_t)(pf_scroll + v);
        if (i >= count) break;

        uint8_t y = PF_AREA_TOP + v * item_h;
        bool ignored = subghz_ignore_group_get((SubGhzIgnoreGroup)i);

        if (i == pf_sel)
        {
            u8g2_DrawRBox(&m1_u8g2, 1, y, PF_TEXT_W, item_h, 2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }

        /* Status marker + group name (with protocol count) on the left. */
        const char *name = subghz_ignore_group_name((SubGhzIgnoreGroup)i);
        if (!name) name = "?";
        char row[40];
        snprintf(row, sizeof(row), "%s %s (%u)",
                 ignored ? "[ ]" : "[x]", name,
                 (unsigned)subghz_ignore_group_protocol_count((SubGhzIgnoreGroup)i));
        u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, row);

        /* Status word on the right for the selected row. */
        if (i == pf_sel)
        {
            const char *status = ignored ? "Ign" : "On";
            u8g2_uint_t sw = u8g2_GetStrWidth(&m1_u8g2, status);
            u8g2_DrawStr(&m1_u8g2, PF_TEXT_W - sw - 2, y + text_ofs, status);
        }

        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    /* Scrollbar — only when the list exceeds the visible area. */
    if (count > visible)
    {
        uint8_t sb_area_h   = visible * item_h;
        uint8_t sb_handle_h = (uint8_t)(sb_area_h / count);
        if (sb_handle_h < 6) sb_handle_h = 6;
        uint8_t sb_travel_h = (sb_area_h > sb_handle_h) ? (sb_area_h - sb_handle_h) : 0;
        uint8_t sb_handle_y = PF_AREA_TOP;
        if (count > 1)
            sb_handle_y += (uint8_t)((uint32_t)sb_travel_h * pf_sel / (count - 1));

        u8g2_DrawVLine(&m1_u8g2, PF_SCROLLBAR_X + PF_SCROLLBAR_W / 2,
                       PF_AREA_TOP, sb_area_h);
        u8g2_DrawRBox(&m1_u8g2, PF_SCROLLBAR_X, sb_handle_y,
                      PF_SCROLLBAR_W, sb_handle_h, 1);
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_protocol_filter_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
