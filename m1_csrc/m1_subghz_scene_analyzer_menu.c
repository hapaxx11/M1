/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_analyzer_menu.c
 * @brief  Sub-GHz Analyzer sub-menu scene.
 *
 * Consolidates the RF-analysis tools that previously lived as separate
 * top-level Sub-GHz menu entries into a single "Analyzer" group.  Most of
 * these tools are passive — they only observe / measure / identify the RF
 * environment without transmitting.  Proto Pirate additionally offers a
 * protocol-aware receiver for rolling-code capture.
 *
 * Menu items:
 *   1. Frequency Analyzer — pushes SubGhzSceneFreqAnalyzer
 *   2. Spectrum Analyzer  — pushes SubGhzSceneSpectrumAnalyzer
 *   3. RSSI Meter         — pushes SubGhzSceneRssiMeter
 *   4. Freq Scanner       — pushes SubGhzSceneFreqScanner
 *   5. Signal ID          — pushes SubGhzSceneSignalIdentifier (RF Rosetta)
 *   6. Proto Pirate       — pushes SubGhzSceneProtoPirateMenu (rolling-code)
 *
 * All scene transitions are non-blocking; the scene itself only handles
 * navigation events (UP / DOWN / OK / BACK).
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "m1_submenu.h"
#include "subghz_submenu_model.h"

/*============================================================================*/
/* Menu items                                                                 */
/*============================================================================*/

#define AN_MENU_ITEM_COUNT  6

static const char *an_menu_labels[AN_MENU_ITEM_COUNT] = {
    "Frequency Analyzer",
    "Spectrum Analyzer",
    "RSSI Meter",
    "Freq Scanner",
    "Signal ID",
    "Proto Pirate",
};

static const SubGhzSceneId an_menu_targets[AN_MENU_ITEM_COUNT] = {
    SubGhzSceneFreqAnalyzer,
    SubGhzSceneSpectrumAnalyzer,
    SubGhzSceneRssiMeter,
    SubGhzSceneFreqScanner,
    SubGhzSceneSignalIdentifier,
    SubGhzSceneProtoPirateMenu,
};

/*============================================================================*/
/* Selection state                                                            */
/*============================================================================*/

static subghz_submenu_model_t s_model;

static inline uint8_t an_menu_get_saved_sel(const SubGhzApp *app)
{
    uint32_t s = subghz_scene_get_state(app, SubGhzSceneAnalyzerMenu);
    uint8_t sel = (uint8_t)(s & 0xFFu);
    return (sel < AN_MENU_ITEM_COUNT) ? sel : 0;
}

static inline void an_menu_save_sel(SubGhzApp *app, uint8_t sel)
{
    subghz_scene_set_state(app, SubGhzSceneAnalyzerMenu, (uint32_t)sel);
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    subghz_submenu_model_init(&s_model,
                              AN_MENU_ITEM_COUNT,
                              M1_MENU_VIS(AN_MENU_ITEM_COUNT));
    subghz_submenu_model_set_selected(&s_model, an_menu_get_saved_sel(app));
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    subghz_submenu_model_set_visible_count(&s_model,
                                           M1_MENU_VIS(AN_MENU_ITEM_COUNT));
    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            subghz_submenu_model_up(&s_model);
            an_menu_save_sel(app, s_model.selected);
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            subghz_submenu_model_down(&s_model);
            an_menu_save_sel(app, s_model.selected);
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
        {
            SubGhzSceneId target = an_menu_targets[s_model.selected];
            if (target < SubGhzSceneCount)
                subghz_scene_push(app, target);
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
    subghz_submenu_model_set_visible_count(&s_model,
                                           M1_MENU_VIS(AN_MENU_ITEM_COUNT));
    m1_submenu_draw(&s_model, "Analyzer", an_menu_labels);
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_analyzer_menu_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
