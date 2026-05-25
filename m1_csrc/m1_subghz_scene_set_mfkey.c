/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_set_mfkey.c
 * @brief  Sub-GHz Create-from-scratch KeeLoq SetMfKey picker scene (Phase 8c-2).
 *
 * Scrollable list picker over the loaded KeeLoq manufacturer-key table
 * (built-in flash or SD-card fallback — see
 * `Sub_Ghz/subghz_keeloq_mfkeys.{h,c}`).  Lets the user pick the
 * manufacturer whose master key + learn type should be used to encrypt
 * the assembled hop word in the Phase 8c-3 final-assembly step.
 *
 * On OK the picked manufacturer name is stored in
 * @ref SubGhzApp::create_mfkey_name and the scene pops back to whichever
 * scene pushed it.  Phase 8c-3 will read `create_mfkey_name`, look up
 * the entry via @ref keeloq_mfkeys_find, and assemble the final 64-bit
 * key.
 *
 * If the keystore is empty (host build, missing SD card, or vault
 * absent) the picker draws a short "No KeeLoq mfkeys loaded" placeholder
 * and BACK pops out without modifying the app state.
 *
 * Selection persists across pushes/pops via the Phase 2 per-scene state
 * slot keyed by @ref SubGhzSceneSetMfKey so re-entering the picker
 * returns the user to the same row.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "u8g2.h"

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_subghz_scene.h"
#include "subghz_keeloq_mfkeys.h"
#include "subghz_submenu_model.h"

/*============================================================================*/
/* Scene state                                                                */
/*============================================================================*/

/** Maximum number of manufacturer-key entries shown in the picker.  The
 *  builtin vault and typical SD-card keystore both stay well under this
 *  cap; entries beyond it are simply not displayed (BACK and BACK-from-
 *  parent still work).  Sized to keep this scene's static footprint
 *  reasonable — each label slot is a single name pointer. */
#define SETMFKEY_MAX_ENTRIES   64U

static subghz_submenu_model_t s_model;
/** Cached copy of each visible entry's name (NUL-terminated).  We keep
 *  our own copy rather than pointing into the mfkeys table because the
 *  table could in principle be freed/reloaded between scene entries. */
static char        s_names[SETMFKEY_MAX_ENTRIES][sizeof(((KeeLoqMfrEntry*)0)->name)];
static const char *s_labels[SETMFKEY_MAX_ENTRIES];
static uint32_t    s_loaded_count;

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

static void load_entries(void)
{
    uint32_t total = keeloq_mfkeys_count();
    if (total > SETMFKEY_MAX_ENTRIES)
        total = SETMFKEY_MAX_ENTRIES;

    s_loaded_count = 0U;
    for (uint32_t i = 0U; i < total; ++i)
    {
        KeeLoqMfrEntry e;
        if (!keeloq_mfkeys_get_at(i, &e))
            break;
        /* Truncate-safe copy of the name into our scene-local buffer. */
        strncpy(s_names[s_loaded_count], e.name, sizeof(s_names[0]) - 1U);
        s_names[s_loaded_count][sizeof(s_names[0]) - 1U] = '\0';
        s_labels[s_loaded_count] = s_names[s_loaded_count];
        ++s_loaded_count;
    }
}

/** Find the index of @p name in the cached label list, or UINT32_MAX. */
static uint32_t find_index_by_name(const char *name)
{
    if (!name || name[0] == '\0')
        return UINT32_MAX;
    for (uint32_t i = 0U; i < s_loaded_count; ++i)
    {
        if (strncmp(s_names[i], name, sizeof(s_names[0])) == 0)
            return i;
    }
    return UINT32_MAX;
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    load_entries();

    subghz_submenu_model_init(&s_model,
                              (uint8_t)s_loaded_count,
                              M1_MENU_VIS(s_loaded_count));

    /* Prefer the currently-selected mfkey (if it's still in the table) as
     * the initial cursor position; otherwise restore the last cursor
     * position from the per-scene state slot. */
    uint32_t want = find_index_by_name(app->create_mfkey_name);
    if (want == UINT32_MAX)
        want = subghz_scene_get_state(app, SubGhzSceneSetMfKey);

    if (want < s_loaded_count)
    {
        while (s_model.selected < want)
            subghz_submenu_model_down(&s_model);
        while (s_model.selected > want)
            subghz_submenu_model_up(&s_model);
    }

    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    /* Resync visible_count on every event in case the user changed the
     * text-size preference while a child scene was on top. */
    subghz_submenu_model_set_visible_count(&s_model,
                                           M1_MENU_VIS(s_loaded_count));

    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            if (s_loaded_count > 0U)
            {
                subghz_submenu_model_up(&s_model);
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventDown:
            if (s_loaded_count > 0U)
            {
                subghz_submenu_model_down(&s_model);
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventOk:
            if (s_loaded_count > 0U && s_model.selected < s_loaded_count)
            {
                strncpy(app->create_mfkey_name,
                        s_names[s_model.selected],
                        sizeof(app->create_mfkey_name) - 1U);
                app->create_mfkey_name[sizeof(app->create_mfkey_name) - 1U] = '\0';
                subghz_scene_set_state(app, SubGhzSceneSetMfKey,
                                       s_model.selected);
                subghz_scene_pop(app);
            }
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

/*============================================================================*/
/* Draw                                                                       */
/*============================================================================*/

static void draw_empty(void)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "Manufacturer");
    u8g2_DrawHLine(&m1_u8g2, 0, 11, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 0, 30, M1_LCD_DISPLAY_WIDTH,
                 "No KeeLoq mfkeys", TEXT_ALIGN_CENTER);
    m1_draw_text(&m1_u8g2, 0, 40, M1_LCD_DISPLAY_WIDTH,
                 "loaded", TEXT_ALIGN_CENTER);

    m1_u8g2_nextpage();
}

static void draw(SubGhzApp *app)
{
    (void)app;

    if (s_loaded_count == 0U)
    {
        draw_empty();
        return;
    }

    subghz_submenu_model_set_visible_count(&s_model,
                                           M1_MENU_VIS(s_loaded_count));
    m1_submenu_draw(&s_model, "Manufacturer", s_labels);
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_set_mfkey_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
