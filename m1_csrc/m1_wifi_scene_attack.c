/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_attack.c
 * @brief  WiFi Attacks sub-menu + attack delegates.
 *
 * Scenes covered:
 *   WifiSceneAttackMenu         — Attacks sub-menu (8 items)
 *   WifiSceneAttackDeauth       — Deauth attack delegate
 *   WifiSceneAttackBeacon       — Beacon Spam delegate
 *   WifiSceneAttackClone        — AP Clone delegate
 *   WifiSceneAttackRickroll     — Rickroll delegate
 *   WifiSceneAttackEvilPortal   — Evil Portal delegate
 *   WifiSceneAttackProbeFlood   — Probe Flood delegate
 *   WifiSceneAttackKarma        — Karma delegate
 *   WifiSceneAttackKarmaPortal  — Karma+Portal delegate
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
/* Attack delegates                                                         */
/*==========================================================================*/

DELEGATE(attack_deauth,       wifi_attack_deauth)
DELEGATE(attack_beacon,       wifi_attack_beacon)
DELEGATE(attack_clone,        wifi_attack_ap_clone)
DELEGATE(attack_rickroll,     wifi_attack_rickroll)
DELEGATE(attack_evil_portal,  wifi_evil_portal)
DELEGATE(attack_probe_flood,  wifi_probe_flood)
DELEGATE(attack_karma,        wifi_attack_karma)
DELEGATE(attack_karma_portal, wifi_attack_karma_portal)

const M1SceneHandlers wifi_scene_attack_deauth_handlers      = { .on_enter = attack_deauth_on_enter      };
const M1SceneHandlers wifi_scene_attack_beacon_handlers      = { .on_enter = attack_beacon_on_enter      };
const M1SceneHandlers wifi_scene_attack_clone_handlers       = { .on_enter = attack_clone_on_enter       };
const M1SceneHandlers wifi_scene_attack_rickroll_handlers    = { .on_enter = attack_rickroll_on_enter    };
const M1SceneHandlers wifi_scene_attack_evil_portal_handlers = { .on_enter = attack_evil_portal_on_enter };
const M1SceneHandlers wifi_scene_attack_probe_flood_handlers = { .on_enter = attack_probe_flood_on_enter };
const M1SceneHandlers wifi_scene_attack_karma_handlers       = { .on_enter = attack_karma_on_enter       };
const M1SceneHandlers wifi_scene_attack_karma_portal_handlers= { .on_enter = attack_karma_portal_on_enter};

/*==========================================================================*/
/* Attacks sub-menu                                                         */
/*==========================================================================*/

#define ATTACK_ITEM_COUNT  8

static const char *const attack_labels[ATTACK_ITEM_COUNT] = {
    "Deauth", "Beacon Spam", "AP Clone",
    "Rickroll", "Evil Portal", "Probe Flood",
    "Karma", "Karma+Portal",
};

static const uint8_t attack_targets[ATTACK_ITEM_COUNT] = {
    WifiSceneAttackDeauth, WifiSceneAttackBeacon, WifiSceneAttackClone,
    WifiSceneAttackRickroll, WifiSceneAttackEvilPortal, WifiSceneAttackProbeFlood,
    WifiSceneAttackKarma, WifiSceneAttackKarmaPortal,
};

static subghz_submenu_model_t s_attack_model;

static void attack_menu_enter(M1SceneApp *app)
{
    (void)app;
    subghz_submenu_model_init(&s_attack_model, ATTACK_ITEM_COUNT,
                              M1_MENU_VIS(ATTACK_ITEM_COUNT));
    app->need_redraw = true;
}

static bool attack_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_attack_model, attack_targets);
}

static void attack_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_attack_model, "Attacks", attack_labels);
}

const M1SceneHandlers wifi_scene_attack_menu_handlers = {
    .on_enter = attack_menu_enter,
    .on_event = attack_menu_event,
    .on_exit  = NULL,
    .draw     = attack_menu_draw,
};
