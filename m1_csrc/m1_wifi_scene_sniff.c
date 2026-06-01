/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_sniff.c
 * @brief  WiFi Sniffers sub-menu + sniffer delegates.
 *
 * Scenes covered:
 *   WifiSceneSnifferMenu    — Sniffers sub-menu (7 items)
 *   WifiSceneSniffAll       — All Packets sniffer delegate
 *   WifiSceneSniffBeacon    — Beacons sniffer delegate
 *   WifiSceneSniffProbe     — Probe Req sniffer delegate
 *   WifiSceneSniffDeauth    — Deauth sniffer delegate
 *   WifiSceneSniffEapol     — EAPOL sniffer delegate
 *   WifiSceneSniffPwnagotchi— Pwnagotchi sniffer delegate
 *   WifiSceneSniffSae       — SAE/WPA3 sniffer delegate
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
/* Sniffer delegates                                                        */
/*==========================================================================*/

DELEGATE(sniff_all,        wifi_sniff_all)
DELEGATE(sniff_beacon,     wifi_sniff_beacon)
DELEGATE(sniff_probe,      wifi_sniff_probe)
DELEGATE(sniff_deauth,     wifi_sniff_deauth)
DELEGATE(sniff_eapol,      wifi_sniff_eapol)
DELEGATE(sniff_pwnagotchi, wifi_sniff_pwnagotchi)
DELEGATE(sniff_sae,        wifi_sniff_sae)

const M1SceneHandlers wifi_scene_sniff_all_handlers        = { .on_enter = sniff_all_on_enter        };
const M1SceneHandlers wifi_scene_sniff_beacon_handlers     = { .on_enter = sniff_beacon_on_enter     };
const M1SceneHandlers wifi_scene_sniff_probe_handlers      = { .on_enter = sniff_probe_on_enter      };
const M1SceneHandlers wifi_scene_sniff_deauth_handlers     = { .on_enter = sniff_deauth_on_enter     };
const M1SceneHandlers wifi_scene_sniff_eapol_handlers      = { .on_enter = sniff_eapol_on_enter      };
const M1SceneHandlers wifi_scene_sniff_pwnagotchi_handlers = { .on_enter = sniff_pwnagotchi_on_enter };
const M1SceneHandlers wifi_scene_sniff_sae_handlers        = { .on_enter = sniff_sae_on_enter        };

/*==========================================================================*/
/* Sniffers sub-menu                                                        */
/*==========================================================================*/

#define SNIFFER_ITEM_COUNT  7

static const char *const sniffer_labels[SNIFFER_ITEM_COUNT] = {
    "All Packets", "Beacons", "Probe Req",
    "Deauth", "EAPOL", "Pwnagotchi", "SAE/WPA3",
};

static const uint8_t sniffer_targets[SNIFFER_ITEM_COUNT] = {
    WifiSceneSniffAll, WifiSceneSniffBeacon, WifiSceneSniffProbe,
    WifiSceneSniffDeauth, WifiSceneSniffEapol, WifiSceneSniffPwnagotchi,
    WifiSceneSniffSae,
};

static subghz_submenu_model_t s_sniffer_model;

static void sniffer_menu_enter(M1SceneApp *app)
{
    (void)app;
    subghz_submenu_model_init(&s_sniffer_model, SNIFFER_ITEM_COUNT,
                              M1_MENU_VIS(SNIFFER_ITEM_COUNT));
    app->need_redraw = true;
}

static bool sniffer_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_sniffer_model, sniffer_targets);
}

static void sniffer_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_sniffer_model, "Sniffers", sniffer_labels);
}

const M1SceneHandlers wifi_scene_sniffer_menu_handlers = {
    .on_enter = sniffer_menu_enter,
    .on_event = sniffer_menu_event,
    .on_exit  = NULL,
    .draw     = sniffer_menu_draw,
};
