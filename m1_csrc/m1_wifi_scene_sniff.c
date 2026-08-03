/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_sniff.c
 * @brief  WiFi passive sniffer delegates (reached from the Recon sub-menu).
 *
 * Scenes covered:
 *   WifiSceneSniffAll       — Packet Monitor sniffer delegate
 *   WifiSceneSniffBeacon    — Beacon sniffer delegate
 *   WifiSceneSniffProbe     — Probe Req sniffer delegate
 *   WifiSceneSniffDeauth    — Deauth sniffer delegate
 *   WifiSceneSniffEapol     — EAPOL sniffer delegate
 *   WifiSceneSniffPwnagotchi— Pwnagotchi sniffer delegate
 *   WifiSceneSniffSae       — SAE/WPA3 sniffer delegate
 *
 * The former standalone "Sniffers" sub-menu was merged into Recon (Phase 2 of
 * the WiFi cleanup plan); these passive captures are now listed alongside the
 * other passive-observation tools and inherit Recon's disconnect prompt.
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

/* Capability-gated blocking delegate — see m1_wifi_scene_menu.c for the
 * canonical documentation of this pattern. All sniffers here rely on the
 * binary-SPI CMD_PKTMON_START/NEXT/STOP command family (ESP32_FEATURE_PKTMON). */
#define DELEGATE_FEATURE(name, fn, fid) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; \
        m1_esp32_ensure_init(); \
        if (m1_esp32_require_cap(esp32_feature_required_caps(fid), \
                                  esp32_feature_label(fid))) { fn(); } \
        m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Sniffer delegates                                                        */
/*==========================================================================*/

DELEGATE_FEATURE(sniff_all,        wifi_sniff_all,        ESP32_FEATURE_PKTMON)
DELEGATE_FEATURE(sniff_beacon,     wifi_sniff_beacon,     ESP32_FEATURE_PKTMON)
DELEGATE_FEATURE(sniff_probe,      wifi_sniff_probe,      ESP32_FEATURE_PKTMON)
DELEGATE_FEATURE(sniff_deauth,     wifi_sniff_deauth,     ESP32_FEATURE_PKTMON)
DELEGATE_FEATURE(sniff_eapol,      wifi_sniff_eapol,      ESP32_FEATURE_PKTMON)
DELEGATE_FEATURE(sniff_pwnagotchi, wifi_sniff_pwnagotchi, ESP32_FEATURE_PKTMON)
DELEGATE_FEATURE(sniff_sae,        wifi_sniff_sae,        ESP32_FEATURE_PKTMON)

const M1SceneHandlers wifi_scene_sniff_all_handlers        = { .on_enter = sniff_all_on_enter        };
const M1SceneHandlers wifi_scene_sniff_beacon_handlers     = { .on_enter = sniff_beacon_on_enter     };
const M1SceneHandlers wifi_scene_sniff_probe_handlers      = { .on_enter = sniff_probe_on_enter      };
const M1SceneHandlers wifi_scene_sniff_deauth_handlers     = { .on_enter = sniff_deauth_on_enter     };
const M1SceneHandlers wifi_scene_sniff_eapol_handlers      = { .on_enter = sniff_eapol_on_enter      };
const M1SceneHandlers wifi_scene_sniff_pwnagotchi_handlers = { .on_enter = sniff_pwnagotchi_on_enter };
const M1SceneHandlers wifi_scene_sniff_sae_handlers        = { .on_enter = sniff_sae_on_enter        };
