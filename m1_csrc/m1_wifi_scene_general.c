/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_general.c
 * @brief  WiFi General/Config sub-menu + general delegates.
 *
 * Scenes covered:
 *   WifiSceneGeneralMenu            — General sub-menu (14 items)
 *   WifiSceneGeneralViewInfo        — View AP Info delegate
 *   WifiSceneGeneralSelectAps       — Select APs delegate
 *   WifiSceneGeneralSelectStas      — Select STAs delegate
 *   WifiSceneGeneralSaveAps         — Save APs delegate
 *   WifiSceneGeneralLoadAps         — Load APs delegate
 *   WifiSceneGeneralClearAps        — Clear APs delegate
 *   WifiSceneGeneralLoadSsids       — Load SSIDs delegate
 *   WifiSceneGeneralClearSsids      — Clear SSIDs delegate
 *   WifiSceneGeneralJoin            — Join WiFi delegate
 *   WifiSceneGeneralSetMacs         — Set MACs delegate
 *   WifiSceneGeneralSetChan         — Set Channel delegate
 *   WifiSceneGeneralShutdown        — Shutdown WiFi delegate
 *   WifiSceneGeneralSetEpSsid       — Set EP SSID delegate
 *   WifiSceneGeneralSelectEpHtml    — Select EP HTML delegate
 *
 * Phase E: uses `subghz_submenu_model_t` + `m1_submenu_draw/event` for
 * consistent font-aware layout and automatic visible-count sync.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_wifi_scene.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_wifi.h"
#include "m1_esp32_hal.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Blocking delegate macro                                                  */
/*==========================================================================*/

#define DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* General delegates                                                        */
/*==========================================================================*/

DELEGATE(gen_view_info,   wifi_general_view_ap_info)
DELEGATE(gen_select_aps,  wifi_general_select_aps)
DELEGATE(gen_select_stas, wifi_general_select_stations)
DELEGATE(gen_save_aps,    wifi_general_save_aps)
DELEGATE(gen_load_aps,    wifi_general_load_aps)
DELEGATE(gen_clear_aps,   wifi_general_clear_aps)
DELEGATE(gen_load_ssids,  wifi_general_load_ssids)
DELEGATE(gen_clear_ssids, wifi_general_clear_ssids)
DELEGATE(gen_join,        wifi_general_join_wifi)
DELEGATE(gen_set_macs,    wifi_general_set_macs)
DELEGATE(gen_set_chan,     wifi_general_set_channel)
DELEGATE(gen_shutdown,    wifi_general_shutdown_wifi)
DELEGATE(gen_ep_ssid,     wifi_general_set_ep_ssid)
DELEGATE(gen_ep_html,     wifi_general_select_ep_html)

const M1SceneHandlers wifi_scene_gen_view_info_handlers   = { .on_enter = gen_view_info_on_enter   };
const M1SceneHandlers wifi_scene_gen_select_aps_handlers  = { .on_enter = gen_select_aps_on_enter  };
const M1SceneHandlers wifi_scene_gen_select_stas_handlers = { .on_enter = gen_select_stas_on_enter };
const M1SceneHandlers wifi_scene_gen_save_aps_handlers    = { .on_enter = gen_save_aps_on_enter    };
const M1SceneHandlers wifi_scene_gen_load_aps_handlers    = { .on_enter = gen_load_aps_on_enter    };
const M1SceneHandlers wifi_scene_gen_clear_aps_handlers   = { .on_enter = gen_clear_aps_on_enter   };
const M1SceneHandlers wifi_scene_gen_load_ssids_handlers  = { .on_enter = gen_load_ssids_on_enter  };
const M1SceneHandlers wifi_scene_gen_clear_ssids_handlers = { .on_enter = gen_clear_ssids_on_enter };
const M1SceneHandlers wifi_scene_gen_join_handlers        = { .on_enter = gen_join_on_enter        };
const M1SceneHandlers wifi_scene_gen_set_macs_handlers    = { .on_enter = gen_set_macs_on_enter    };
const M1SceneHandlers wifi_scene_gen_set_chan_handlers     = { .on_enter = gen_set_chan_on_enter     };
const M1SceneHandlers wifi_scene_gen_shutdown_handlers    = { .on_enter = gen_shutdown_on_enter    };
const M1SceneHandlers wifi_scene_gen_ep_ssid_handlers     = { .on_enter = gen_ep_ssid_on_enter     };
const M1SceneHandlers wifi_scene_gen_ep_html_handlers     = { .on_enter = gen_ep_html_on_enter     };

/*==========================================================================*/
/* General sub-menu                                                         */
/*==========================================================================*/

#define GENERAL_ITEM_COUNT  14

static const char *const general_labels[GENERAL_ITEM_COUNT] = {
    "View AP Info", "Select APs", "Select STAs",
    "Save APs", "Load APs", "Clear APs",
    "Load SSIDs", "Clear SSIDs",
    "Join WiFi", "Set MACs", "Set Channel",
    "Shutdown WiFi", "Set EP SSID", "Select EP HTML",
};

static const uint8_t general_targets[GENERAL_ITEM_COUNT] = {
    WifiSceneGeneralViewInfo, WifiSceneGeneralSelectAps, WifiSceneGeneralSelectStas,
    WifiSceneGeneralSaveAps, WifiSceneGeneralLoadAps, WifiSceneGeneralClearAps,
    WifiSceneGeneralLoadSsids, WifiSceneGeneralClearSsids,
    WifiSceneGeneralJoin, WifiSceneGeneralSetMacs, WifiSceneGeneralSetChan,
    WifiSceneGeneralShutdown, WifiSceneGeneralSetEpSsid, WifiSceneGeneralSelectEpHtml,
};

static subghz_submenu_model_t s_general_model;

static void general_menu_enter(M1SceneApp *app)
{
    (void)app;
    subghz_submenu_model_init(&s_general_model, GENERAL_ITEM_COUNT,
                              M1_MENU_VIS(GENERAL_ITEM_COUNT));
    app->need_redraw = true;
}

static bool general_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_general_model, general_targets);
}

static void general_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_general_model, "General", general_labels);
}

const M1SceneHandlers wifi_scene_general_menu_handlers = {
    .on_enter = general_menu_enter,
    .on_event = general_menu_event,
    .on_exit  = NULL,
    .draw     = general_menu_draw,
};
