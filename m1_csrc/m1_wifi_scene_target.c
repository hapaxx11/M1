/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_target.c
 * @brief  Selected-network Target context (WiFi cleanup plan §3.2).
 *
 * Reached from Scan & Connect: pressing OK on a network in the scan list flags
 * a pending target (wifi_scan_ap_target_selected()) and the Scan & Connect
 * scene delegate pushes WifiSceneTargetMenu for the highlighted AP.
 *
 * The menu separates the two precondition groups called out in the plan:
 *   - Connect group (needs auth)     → "Connect" → Connected menu on success.
 *   - Target group  (needs an SSID)  → per-AP Deauth, Handshake/EAPOL capture,
 *                                      Beacon clone, PMKID, plus "Cycle AP" to
 *                                      iterate the known BSSIDs of the SSID.
 *
 * Every action operates on the AP currently highlighted in the scan list, so
 * the Target actions are unreachable except via a selected network.
 *
 * Scenes covered:
 *   WifiSceneTargetMenu       — selected-network menu
 *   WifiSceneTargetConnect    — Connect delegate (→ Connected menu)
 *   WifiSceneTargetDeauth     — per-AP Deauth delegate
 *   WifiSceneTargetHandshake  — Handshake / EAPOL capture delegate
 *   WifiSceneTargetBeacon     — Beacon (clone SSID) delegate
 *   WifiSceneTargetPmkid      — PMKID capture delegate
 *   WifiSceneTargetCycle      — Cycle AP delegate (advance BSSID, redraw)
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
#include "m1_esp32_hal.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#include "m1_compile_cfg.h"

/*==========================================================================*/
/* Target-action delegates                                                  */
/*==========================================================================*/

/* Connect (auth group): on success replace the Target menu with the existing
 * Connected menu so the connected-context tools take over (plan §3.2/§3.3). */
static void target_connect_on_enter(M1SceneApp *app)
{
    (void)app;
    wifi_target_connect();
    app->running = true;
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    if (wifi_is_connected()) {
        m1_esp32_deinit();
        m1_scene_replace(app, WifiSceneConnectedMenu);
        return;
    }
#endif
    m1_scene_pop(app);
}

/* Blocking Target-action delegate: run the action, then return to the Target
 * menu (which redraws its title from the currently highlighted AP). */
#define TARGET_DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); app->running = true; m1_scene_pop(app); }

TARGET_DELEGATE(target_deauth,    wifi_target_deauth)
TARGET_DELEGATE(target_handshake, wifi_target_handshake)
TARGET_DELEGATE(target_beacon,    wifi_target_beacon)
TARGET_DELEGATE(target_pmkid,     wifi_target_pmkid)
TARGET_DELEGATE(target_cycle,     wifi_target_cycle)

const M1SceneHandlers wifi_scene_target_connect_handlers   = { .on_enter = target_connect_on_enter   };
const M1SceneHandlers wifi_scene_target_deauth_handlers    = { .on_enter = target_deauth_on_enter    };
const M1SceneHandlers wifi_scene_target_handshake_handlers = { .on_enter = target_handshake_on_enter };
const M1SceneHandlers wifi_scene_target_beacon_handlers    = { .on_enter = target_beacon_on_enter    };
const M1SceneHandlers wifi_scene_target_pmkid_handlers     = { .on_enter = target_pmkid_on_enter     };
const M1SceneHandlers wifi_scene_target_cycle_handlers     = { .on_enter = target_cycle_on_enter     };

/*==========================================================================*/
/* Target sub-menu (Connect group + Target group)                           */
/*==========================================================================*/

#define TARGET_ITEM_COUNT  6

static const char *const target_labels[TARGET_ITEM_COUNT] = {
    "Connect",     /* Connect group — needs auth */
    "Deauth",      /* Target group — needs a specific AP */
    "Handshake",
    "Beacon",
    "PMKID",
    "Cycle AP",
};

static const uint8_t target_targets[TARGET_ITEM_COUNT] = {
    WifiSceneTargetConnect,
    WifiSceneTargetDeauth,
    WifiSceneTargetHandshake,
    WifiSceneTargetBeacon,
    WifiSceneTargetPmkid,
    WifiSceneTargetCycle,
};

static subghz_submenu_model_t s_target_model;

static void target_menu_enter(M1SceneApp *app)
{
    /* Guard: only reachable with a valid highlighted AP.  If the scan list is
     * gone (e.g. re-entry after a rescan produced nothing), fall back. */
    if (!wifi_target_valid()) {
        m1_scene_pop(app);
        return;
    }
    if (s_target_model.item_count == 0)
        subghz_submenu_model_init(&s_target_model, TARGET_ITEM_COUNT,
                                  M1_MENU_VIS(TARGET_ITEM_COUNT));
    app->need_redraw = true;
}

static bool target_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_target_model, target_targets);
}

static void target_menu_draw(M1SceneApp *app)
{
    (void)app;
    const char *ssid = wifi_target_ssid();
    m1_submenu_draw(&s_target_model,
                    (ssid && ssid[0]) ? ssid : "*hidden*",
                    target_labels);
}

const M1SceneHandlers wifi_scene_target_menu_handlers = {
    .on_enter = target_menu_enter,
    .on_event = target_menu_event,
    .on_exit  = NULL,
    .draw     = target_menu_draw,
};
