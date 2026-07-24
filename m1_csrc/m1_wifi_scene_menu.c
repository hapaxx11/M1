/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_menu.c
 * @brief  WiFi top-level menu scene + Recon/802.15.4 sub-menus + core delegates.
 *
 * Scenes covered:
 *   WifiSceneMenu           — top-level 6-item menu
 *   WifiSceneReconMenu      — Recon sub-menu (6 items)
 *   WifiScene802154Menu     — 802.15.4 sub-menu (2 items)
 *   WifiSceneScanConnect    — Networks scan/connect delegate
 *   WifiSceneStationScan    — Station Scan delegate
 *   WifiSceneSurvey24g      — 2.4G Channel Survey delegate
 *   WifiSceneMacTrack       — MAC Track delegate
 *   WifiSceneWardrive       — Wardrive delegate
 *   WifiSceneStationWardrive— Station Wardrive delegate
 *   WifiSceneSignalMonitor  — Signal Monitor delegate
 *   WifiSceneZigbee         — Zigbee Scan delegate
 *   WifiSceneThread         — Thread Scan delegate
 *
 * Submenu model: uses `subghz_submenu_model_t` + `m1_submenu_draw/event` for
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
#include "m1_esp32_caps.h"
#include "esp32_feature_map.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#include "m1_compile_cfg.h"

/*==========================================================================*/
/* Blocking delegate macro                                                  */
/*==========================================================================*/

#define DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/* Capability-gated blocking delegate — shows a "not supported" screen and
 * pops immediately when the required ESP32 capability is absent, instead of
 * calling into a binary-SPI-only command (e.g. CMD_STA_SCAN_START) on
 * AT-firmware builds (dag T-800, stock AT) that don't implement it.  See
 * esp32_feature_map.c for the required-capability table. */
#define DELEGATE_FEATURE(name, fn, fid) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; \
        m1_esp32_ensure_init(); \
        if (m1_esp32_require_cap(esp32_feature_required_caps(fid), \
                                  esp32_feature_label(fid))) { fn(); } \
        m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Core / direct-tool delegates                                             */
/*==========================================================================*/

static void scan_connect_on_enter(M1SceneApp *app) {
    (void)app;
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    bool was_connected = wifi_is_connected();
#endif
    wifi_scan_ap();
    m1_esp32_deinit();
    app->running = true;
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    if (!was_connected && wifi_is_connected()) {
        m1_scene_replace(app, WifiSceneConnectedMenu);
        return;
    }
#endif
    m1_scene_pop(app);
}
DELEGATE_FEATURE(station_scan, wifi_station_scan, ESP32_FEATURE_STA_SCAN)
DELEGATE(survey_24g,        wifi_survey_24g)
DELEGATE_FEATURE(mac_track, wifi_mac_track, ESP32_FEATURE_PKTMON)
DELEGATE(wardrive,          wifi_wardrive)
DELEGATE_FEATURE(station_wardrive, wifi_station_wardrive, ESP32_FEATURE_STA_SCAN)
DELEGATE_FEATURE(signal_monitor, wifi_signal_monitor, ESP32_FEATURE_PKTMON)
DELEGATE_FEATURE(zigbee, zigbee_scan, ESP32_FEATURE_802154)
DELEGATE_FEATURE(thread, thread_scan, ESP32_FEATURE_802154)

const M1SceneHandlers wifi_scene_scan_connect_handlers     = { .on_enter = scan_connect_on_enter     };
const M1SceneHandlers wifi_scene_station_scan_handlers     = { .on_enter = station_scan_on_enter     };
const M1SceneHandlers wifi_scene_survey_24g_handlers       = { .on_enter = survey_24g_on_enter       };
const M1SceneHandlers wifi_scene_mac_track_handlers        = { .on_enter = mac_track_on_enter        };
const M1SceneHandlers wifi_scene_wardrive_handlers         = { .on_enter = wardrive_on_enter         };
const M1SceneHandlers wifi_scene_station_wardrive_handlers = { .on_enter = station_wardrive_on_enter };
const M1SceneHandlers wifi_scene_signal_monitor_handlers   = { .on_enter = signal_monitor_on_enter   };
const M1SceneHandlers wifi_scene_zigbee_handlers           = { .on_enter = zigbee_on_enter           };
const M1SceneHandlers wifi_scene_thread_handlers           = { .on_enter = thread_on_enter           };

/*==========================================================================*/
/* Top-level menu (6 items)                                                 */
/*==========================================================================*/

#define MENU_ITEM_COUNT  6

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Networks",
    "Recon",
    "Sniffers",
    "Attacks",
    "802.15.4",
    "General",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    WifiSceneScanConnect,
    WifiSceneReconMenu,
    WifiSceneSnifferMenu,
    WifiSceneAttackMenu,
    WifiScene802154Menu,
    WifiSceneGeneralMenu,
};

static subghz_submenu_model_t s_wifi_menu_model;

static void menu_on_enter(M1SceneApp *app)
{
    (void)app;
    if (s_wifi_menu_model.item_count == 0)
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

/*==========================================================================*/
/* Recon sub-menu (6 items)                                                 */
/*==========================================================================*/

#define RECON_ITEM_COUNT  6

static const char *const recon_labels[RECON_ITEM_COUNT] = {
    "Station Scan",
    "2.4G Survey",
    "MAC Track",
    "Wardrive",
    "Station Wardrive",
    "Signal Monitor",
};

static const uint8_t recon_targets[RECON_ITEM_COUNT] = {
    WifiSceneStationScan,
    WifiSceneSurvey24g,
    WifiSceneMacTrack,
    WifiSceneWardrive,
    WifiSceneStationWardrive,
    WifiSceneSignalMonitor,
};

static subghz_submenu_model_t s_recon_model;

static void recon_menu_enter(M1SceneApp *app)
{
    (void)app;
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    if (!wifi_prompt_disconnect()) {
        m1_scene_pop(app);
        return;
    }
#endif
    if (s_recon_model.item_count == 0)
        subghz_submenu_model_init(&s_recon_model, RECON_ITEM_COUNT,
                                  M1_MENU_VIS(RECON_ITEM_COUNT));
    app->need_redraw = true;
}

static bool recon_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_recon_model, recon_targets);
}

static void recon_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_recon_model, "Recon", recon_labels);
}

const M1SceneHandlers wifi_scene_recon_menu_handlers = {
    .on_enter = recon_menu_enter,
    .on_event = recon_menu_event,
    .on_exit  = NULL,
    .draw     = recon_menu_draw,
};

/*==========================================================================*/
/* 802.15.4 sub-menu (2 items)                                              */
/*==========================================================================*/

#define IEEE802154_ITEM_COUNT  2

static const char *const ieee802154_labels[IEEE802154_ITEM_COUNT] = {
    "Zigbee Scan",
    "Thread Scan",
};

static const uint8_t ieee802154_targets[IEEE802154_ITEM_COUNT] = {
    WifiSceneZigbee,
    WifiSceneThread,
};

static subghz_submenu_model_t s_802154_model;

static void ieee802154_menu_enter(M1SceneApp *app)
{
    (void)app;
    if (s_802154_model.item_count == 0)
        subghz_submenu_model_init(&s_802154_model, IEEE802154_ITEM_COUNT,
                                  M1_MENU_VIS(IEEE802154_ITEM_COUNT));
    app->need_redraw = true;
}

static bool ieee802154_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_802154_model, ieee802154_targets);
}

static void ieee802154_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_802154_model, "802.15.4", ieee802154_labels);
}

const M1SceneHandlers wifi_scene_802154_menu_handlers = {
    .on_enter = ieee802154_menu_enter,
    .on_event = ieee802154_menu_event,
    .on_exit  = NULL,
    .draw     = ieee802154_menu_draw,
};
