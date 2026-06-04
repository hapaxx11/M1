/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_menu.c
 * @brief  WiFi top-level menu scene + core/direct-tool delegates.
 *
 * Scenes covered:
 *   WifiSceneMenu           — top-level 12/15-item menu
 *   WifiSceneScanConnect    — Scan & Connect delegate
 *   WifiSceneStationScan    — Station Scan delegate
 *   WifiSceneMacTrack       — MAC Track delegate
 *   WifiSceneWardrive       — Wardrive delegate
 *   WifiSceneStationWardrive— Station Wardrive delegate
 *   WifiSceneSignalMonitor  — Signal Monitor delegate
 *   WifiSceneZigbee         — Zigbee Scan delegate
 *   WifiSceneThread         — Thread Scan delegate
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
#include "m1_802154.h"
#include "m1_esp32_hal.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#include "m1_compile_cfg.h"

/*==========================================================================*/
/* Blocking delegate macro                                                  */
/*==========================================================================*/

#define DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Core / direct-tool delegates                                             */
/*==========================================================================*/

DELEGATE(scan_connect,      wifi_scan_ap)
DELEGATE(station_scan,      wifi_station_scan)
DELEGATE(mac_track,         wifi_mac_track)
DELEGATE(wardrive,          wifi_wardrive)
DELEGATE(station_wardrive,  wifi_station_wardrive)
DELEGATE(signal_monitor,    wifi_signal_monitor)
DELEGATE(zigbee,            zigbee_scan)
DELEGATE(thread,            thread_scan)

const M1SceneHandlers wifi_scene_scan_connect_handlers     = { .on_enter = scan_connect_on_enter     };
const M1SceneHandlers wifi_scene_station_scan_handlers     = { .on_enter = station_scan_on_enter     };
const M1SceneHandlers wifi_scene_mac_track_handlers        = { .on_enter = mac_track_on_enter        };
const M1SceneHandlers wifi_scene_wardrive_handlers         = { .on_enter = wardrive_on_enter         };
const M1SceneHandlers wifi_scene_station_wardrive_handlers = { .on_enter = station_wardrive_on_enter };
const M1SceneHandlers wifi_scene_signal_monitor_handlers   = { .on_enter = signal_monitor_on_enter   };
const M1SceneHandlers wifi_scene_zigbee_handlers           = { .on_enter = zigbee_on_enter           };
const M1SceneHandlers wifi_scene_thread_handlers           = { .on_enter = thread_on_enter           };

/*==========================================================================*/
/* Top-level menu                                                           */
/*==========================================================================*/

#ifdef M1_APP_WIFI_CONNECT_ENABLE
#define MENU_ITEM_COUNT  15
#else
#define MENU_ITEM_COUNT  12
#endif

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Scan & Connect",
    "Station Scan",
    "MAC Track",
    "Wardrive",
    "Station Wardrive",
    "Sniffers",
    "Attacks",
    "Net Scan",
    "General",
    "Zigbee Scan",
    "Thread Scan",
    "Signal Monitor",
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    "Saved Networks",
    "Status",
    "Disconnect",
#endif
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    WifiSceneScanConnect,
    WifiSceneStationScan,
    WifiSceneMacTrack,
    WifiSceneWardrive,
    WifiSceneStationWardrive,
    WifiSceneSnifferMenu,
    WifiSceneAttackMenu,
    WifiSceneNetMenu,
    WifiSceneGeneralMenu,
    WifiSceneZigbee,
    WifiSceneThread,
    WifiSceneSignalMonitor,
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    WifiSceneSaved,
    WifiSceneStatus,
    WifiSceneDisconnect,
#endif
};

static subghz_submenu_model_t s_wifi_menu_model;

static void menu_on_enter(M1SceneApp *app)
{
    (void)app;
    subghz_submenu_model_init(&s_wifi_menu_model, MENU_ITEM_COUNT,
                              M1_MENU_VIS(MENU_ITEM_COUNT));
    app->need_redraw = true;
}

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_submenu_event(app, event, &s_wifi_menu_model, menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_wifi_menu_model, "WiFi", menu_labels);
}

const M1SceneHandlers wifi_scene_menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};
