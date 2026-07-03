/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_net.c
 * @brief  WiFi Net Scan sub-menu + network-scanner delegates.
 *
 * Scenes covered:
 *   WifiSceneNetMenu    — Net Scan sub-menu (5 items)
 *   WifiSceneNetPing    — Ping Scan delegate
 *   WifiSceneNetArp     — ARP Scan delegate
 *   WifiSceneNetSsh     — SSH Scan delegate
 *   WifiSceneNetTelnet  — Telnet Scan delegate
 *   WifiSceneNetPorts   — Port Scan delegate
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
 * canonical documentation of this pattern.  Network scanners rely on the
 * binary-SPI CMD_NETSCAN_START/NEXT command family (ESP32_FEATURE_NETSCAN),
 * which dag T-800 AT firmware does not implement. */
#define DELEGATE_FEATURE(name, fn, fid) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; \
        m1_esp32_ensure_init(); \
        if (m1_esp32_require_cap(esp32_feature_required_caps(fid), \
                                  esp32_feature_label(fid))) { fn(); } \
        m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Network-scanner delegates                                                */
/*==========================================================================*/

DELEGATE_FEATURE(net_ping,   wifi_scan_ping,   ESP32_FEATURE_NETSCAN)
DELEGATE_FEATURE(net_arp,    wifi_scan_arp,    ESP32_FEATURE_NETSCAN)
DELEGATE_FEATURE(net_ssh,    wifi_scan_ssh,    ESP32_FEATURE_NETSCAN)
DELEGATE_FEATURE(net_telnet, wifi_scan_telnet, ESP32_FEATURE_NETSCAN)
DELEGATE_FEATURE(net_ports,  wifi_scan_ports,  ESP32_FEATURE_NETSCAN)

const M1SceneHandlers wifi_scene_net_ping_handlers   = { .on_enter = net_ping_on_enter   };
const M1SceneHandlers wifi_scene_net_arp_handlers    = { .on_enter = net_arp_on_enter    };
const M1SceneHandlers wifi_scene_net_ssh_handlers    = { .on_enter = net_ssh_on_enter    };
const M1SceneHandlers wifi_scene_net_telnet_handlers = { .on_enter = net_telnet_on_enter };
const M1SceneHandlers wifi_scene_net_ports_handlers  = { .on_enter = net_ports_on_enter  };

/*==========================================================================*/
/* Net Scan sub-menu                                                        */
/*==========================================================================*/

#define NET_ITEM_COUNT  5

static const char *const net_labels[NET_ITEM_COUNT] = {
    "Ping Scan", "ARP Scan", "SSH Scan", "Telnet Scan", "Port Scan",
};

static const uint8_t net_targets[NET_ITEM_COUNT] = {
    WifiSceneNetPing, WifiSceneNetArp, WifiSceneNetSsh,
    WifiSceneNetTelnet, WifiSceneNetPorts,
};

static subghz_submenu_model_t s_net_model;

static void net_menu_enter(M1SceneApp *app)
{
    (void)app;
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    if (!wifi_require_connected()) {
        m1_scene_pop(app);
        return;
    }
#endif
    if (s_net_model.item_count == 0)
        subghz_submenu_model_init(&s_net_model, NET_ITEM_COUNT,
                                  M1_MENU_VIS(NET_ITEM_COUNT));
    app->need_redraw = true;
}

static bool net_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_net_model, net_targets);
}

static void net_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_net_model, "Net Scan", net_labels);
}

const M1SceneHandlers wifi_scene_net_menu_handlers = {
    .on_enter = net_menu_enter,
    .on_event = net_menu_event,
    .on_exit  = NULL,
    .draw     = net_menu_draw,
};
