/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene.c
 * @brief  WiFi Scene Manager — scene-based menu exposing SiN360 WiFi tools.
 *
 * Top-level menu leads to five submenus plus direct functions:
 *   Scan / Sniffers / Attacks / Network Scan / General / (saved/status/disconnect)
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_wifi.h"
#include "m1_802154.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_caps.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#include "m1_compile_cfg.h"

/* ---- Scene IDs ---------------------------------------------------------- */
enum {
    WifiSceneMenu = 0,

    /* Direct tools */
    WifiSceneScanConnect,
    WifiSceneStationScan,
    WifiSceneMacTrack,
    WifiSceneWardrive,
    WifiSceneStationWardrive,
    WifiSceneSignalMonitor,

    /* Sniffers submenu */
    WifiSceneSnifferMenu,
    WifiSceneSniffAll,
    WifiSceneSniffBeacon,
    WifiSceneSniffProbe,
    WifiSceneSniffDeauth,
    WifiSceneSniffEapol,
    WifiSceneSniffPwnagotchi,
    WifiSceneSniffSae,

    /* Attacks submenu */
    WifiSceneAttackMenu,
    WifiSceneAttackDeauth,
    WifiSceneAttackBeacon,
    WifiSceneAttackClone,
    WifiSceneAttackRickroll,
    WifiSceneAttackEvilPortal,
    WifiSceneAttackProbeFlood,
    WifiSceneAttackKarma,
    WifiSceneAttackKarmaPortal,

    /* Network Scanners submenu */
    WifiSceneNetMenu,
    WifiSceneNetPing,
    WifiSceneNetArp,
    WifiSceneNetSsh,
    WifiSceneNetTelnet,
    WifiSceneNetPorts,

    /* General / Config submenu */
    WifiSceneGeneralMenu,
    WifiSceneGeneralViewInfo,
    WifiSceneGeneralSelectAps,
    WifiSceneGeneralSelectStas,
    WifiSceneGeneralSaveAps,
    WifiSceneGeneralLoadAps,
    WifiSceneGeneralClearAps,
    WifiSceneGeneralLoadSsids,
    WifiSceneGeneralClearSsids,
    WifiSceneGeneralJoin,
    WifiSceneGeneralSetMacs,
    WifiSceneGeneralSetChan,
    WifiSceneGeneralShutdown,
    WifiSceneGeneralSetEpSsid,
    WifiSceneGeneralSelectEpHtml,

    /* 802.15.4 */
    WifiSceneZigbee,
    WifiSceneThread,

    /* Connect features (AT-layer stubs) */
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    WifiSceneSaved,
    WifiSceneStatus,
    WifiSceneDisconnect,
#endif

    WifiSceneCount
};

/* ---- Macro: simple blocking-delegate scene builder ---------------------- */
#define DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/* Capability-gated delegate: shows "not supported" screen and pops immediately
 * when the required ESP32 capability is absent. */
#define DELEGATE_CAPPED(name, fn, cap, label) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; \
        if (m1_esp32_require_cap((cap), (label))) { fn(); } \
        m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

DELEGATE(scan_connect,         wifi_scan_ap)
DELEGATE(station_scan,         wifi_station_scan)
DELEGATE(mac_track,            wifi_mac_track)
DELEGATE(wardrive,             wifi_wardrive)
DELEGATE(station_wardrive,     wifi_station_wardrive)
DELEGATE(signal_monitor,       wifi_signal_monitor)

DELEGATE(sniff_all,            wifi_sniff_all)
DELEGATE(sniff_beacon,         wifi_sniff_beacon)
DELEGATE(sniff_probe,          wifi_sniff_probe)
DELEGATE(sniff_deauth,         wifi_sniff_deauth)
DELEGATE(sniff_eapol,          wifi_sniff_eapol)
DELEGATE(sniff_pwnagotchi,     wifi_sniff_pwnagotchi)
DELEGATE(sniff_sae,            wifi_sniff_sae)

DELEGATE(attack_deauth,        wifi_attack_deauth)
DELEGATE(attack_beacon,        wifi_attack_beacon)
DELEGATE(attack_clone,         wifi_attack_ap_clone)
DELEGATE(attack_rickroll,      wifi_attack_rickroll)
DELEGATE(attack_evil_portal,   wifi_evil_portal)
DELEGATE(attack_probe_flood,   wifi_probe_flood)
DELEGATE(attack_karma,         wifi_attack_karma)
DELEGATE(attack_karma_portal,  wifi_attack_karma_portal)

DELEGATE(net_ping,             wifi_scan_ping)
DELEGATE(net_arp,              wifi_scan_arp)
DELEGATE(net_ssh,              wifi_scan_ssh)
DELEGATE(net_telnet,           wifi_scan_telnet)
DELEGATE(net_ports,            wifi_scan_ports)

DELEGATE(gen_view_info,        wifi_general_view_ap_info)
DELEGATE(gen_select_aps,       wifi_general_select_aps)
DELEGATE(gen_select_stas,      wifi_general_select_stations)
DELEGATE(gen_save_aps,         wifi_general_save_aps)
DELEGATE(gen_load_aps,         wifi_general_load_aps)
DELEGATE(gen_clear_aps,        wifi_general_clear_aps)
DELEGATE(gen_load_ssids,       wifi_general_load_ssids)
DELEGATE(gen_clear_ssids,      wifi_general_clear_ssids)
DELEGATE(gen_join,             wifi_general_join_wifi)
DELEGATE(gen_set_macs,         wifi_general_set_macs)
DELEGATE(gen_set_chan,          wifi_general_set_channel)
DELEGATE(gen_shutdown,         wifi_general_shutdown_wifi)
DELEGATE(gen_ep_ssid,          wifi_general_set_ep_ssid)
DELEGATE(gen_ep_html,          wifi_general_select_ep_html)

DELEGATE(zigbee,               zigbee_scan)
DELEGATE(thread,               thread_scan)

#ifdef M1_APP_WIFI_CONNECT_ENABLE
DELEGATE_CAPPED(saved,      wifi_saved_networks, M1_ESP32_CAP_WIFI_CONNECT, "Saved Networks")
DELEGATE_CAPPED(status,     wifi_show_status,    M1_ESP32_CAP_WIFI_CONNECT, "WiFi Status")
DELEGATE_CAPPED(disconnect, wifi_disconnect,     M1_ESP32_CAP_WIFI_CONNECT, "Disconnect WiFi")
#endif

/* ---- Handler table helpers ---------------------------------------------- */
#define HANDLERS(name)  static const M1SceneHandlers name##_handlers = { .on_enter = name##_on_enter }

HANDLERS(scan_connect);
HANDLERS(station_scan);
HANDLERS(mac_track);
HANDLERS(wardrive);
HANDLERS(station_wardrive);
HANDLERS(signal_monitor);
HANDLERS(sniff_all);
HANDLERS(sniff_beacon);
HANDLERS(sniff_probe);
HANDLERS(sniff_deauth);
HANDLERS(sniff_eapol);
HANDLERS(sniff_pwnagotchi);
HANDLERS(sniff_sae);
HANDLERS(attack_deauth);
HANDLERS(attack_beacon);
HANDLERS(attack_clone);
HANDLERS(attack_rickroll);
HANDLERS(attack_evil_portal);
HANDLERS(attack_probe_flood);
HANDLERS(attack_karma);
HANDLERS(attack_karma_portal);
HANDLERS(net_ping);
HANDLERS(net_arp);
HANDLERS(net_ssh);
HANDLERS(net_telnet);
HANDLERS(net_ports);
HANDLERS(gen_view_info);
HANDLERS(gen_select_aps);
HANDLERS(gen_select_stas);
HANDLERS(gen_save_aps);
HANDLERS(gen_load_aps);
HANDLERS(gen_clear_aps);
HANDLERS(gen_load_ssids);
HANDLERS(gen_clear_ssids);
HANDLERS(gen_join);
HANDLERS(gen_set_macs);
HANDLERS(gen_set_chan);
HANDLERS(gen_shutdown);
HANDLERS(gen_ep_ssid);
HANDLERS(gen_ep_html);
HANDLERS(zigbee);
HANDLERS(thread);
#ifdef M1_APP_WIFI_CONNECT_ENABLE
HANDLERS(saved);
HANDLERS(status);
HANDLERS(disconnect);
#endif

/* ---- Sniffers submenu --------------------------------------------------- */
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
static uint8_t sniffer_sel = 0, sniffer_scroll = 0;
static void sniffer_menu_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }
static bool sniffer_menu_event(M1SceneApp *app, M1SceneEvent ev) {
    return m1_scene_menu_event(app, ev, &sniffer_sel, &sniffer_scroll,
                               SNIFFER_ITEM_COUNT, M1_MENU_VIS(SNIFFER_ITEM_COUNT), sniffer_targets);
}
static void sniffer_menu_draw(M1SceneApp *app) {
    (void)app;
    m1_scene_draw_menu("Sniffers", sniffer_labels, SNIFFER_ITEM_COUNT,
                       sniffer_sel, sniffer_scroll, M1_MENU_VIS(SNIFFER_ITEM_COUNT));
}
static const M1SceneHandlers sniffer_menu_handlers = {
    .on_enter = sniffer_menu_enter, .on_event = sniffer_menu_event,
    .on_exit = NULL, .draw = sniffer_menu_draw,
};

/* ---- Attacks submenu ---------------------------------------------------- */
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
static uint8_t attack_sel = 0, attack_scroll = 0;
static void attack_menu_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }
static bool attack_menu_event(M1SceneApp *app, M1SceneEvent ev) {
    return m1_scene_menu_event(app, ev, &attack_sel, &attack_scroll,
                               ATTACK_ITEM_COUNT, M1_MENU_VIS(ATTACK_ITEM_COUNT), attack_targets);
}
static void attack_menu_draw(M1SceneApp *app) {
    (void)app;
    m1_scene_draw_menu("Attacks", attack_labels, ATTACK_ITEM_COUNT,
                       attack_sel, attack_scroll, M1_MENU_VIS(ATTACK_ITEM_COUNT));
}
static const M1SceneHandlers attack_menu_handlers = {
    .on_enter = attack_menu_enter, .on_event = attack_menu_event,
    .on_exit = NULL, .draw = attack_menu_draw,
};

/* ---- Network Scanners submenu ------------------------------------------- */
#define NET_ITEM_COUNT  5
static const char *const net_labels[NET_ITEM_COUNT] = {
    "Ping Scan", "ARP Scan", "SSH Scan", "Telnet Scan", "Port Scan",
};
static const uint8_t net_targets[NET_ITEM_COUNT] = {
    WifiSceneNetPing, WifiSceneNetArp, WifiSceneNetSsh,
    WifiSceneNetTelnet, WifiSceneNetPorts,
};
static uint8_t net_sel = 0, net_scroll = 0;
static void net_menu_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }
static bool net_menu_event(M1SceneApp *app, M1SceneEvent ev) {
    return m1_scene_menu_event(app, ev, &net_sel, &net_scroll,
                               NET_ITEM_COUNT, M1_MENU_VIS(NET_ITEM_COUNT), net_targets);
}
static void net_menu_draw(M1SceneApp *app) {
    (void)app;
    m1_scene_draw_menu("Net Scan", net_labels, NET_ITEM_COUNT,
                       net_sel, net_scroll, M1_MENU_VIS(NET_ITEM_COUNT));
}
static const M1SceneHandlers net_menu_handlers = {
    .on_enter = net_menu_enter, .on_event = net_menu_event,
    .on_exit = NULL, .draw = net_menu_draw,
};

/* ---- General / Config submenu ------------------------------------------- */
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
static uint8_t general_sel = 0, general_scroll = 0;
static void general_menu_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }
static bool general_menu_event(M1SceneApp *app, M1SceneEvent ev) {
    return m1_scene_menu_event(app, ev, &general_sel, &general_scroll,
                               GENERAL_ITEM_COUNT, M1_MENU_VIS(GENERAL_ITEM_COUNT), general_targets);
}
static void general_menu_draw(M1SceneApp *app) {
    (void)app;
    m1_scene_draw_menu("General", general_labels, GENERAL_ITEM_COUNT,
                       general_sel, general_scroll, M1_MENU_VIS(GENERAL_ITEM_COUNT));
}
static const M1SceneHandlers general_menu_handlers = {
    .on_enter = general_menu_enter, .on_event = general_menu_event,
    .on_exit = NULL, .draw = general_menu_draw,
};

/* ---- Top-level menu ----------------------------------------------------- */
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

static uint8_t menu_sel    = 0;
static uint8_t menu_scroll = 0;

static void menu_on_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &menu_sel, &menu_scroll,
                               MENU_ITEM_COUNT, M1_MENU_VIS(MENU_ITEM_COUNT), menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("WiFi", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, M1_MENU_VIS(MENU_ITEM_COUNT));
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/* ---- Scene registry ----------------------------------------------------- */
static const M1SceneHandlers *const scene_registry[WifiSceneCount] = {
    [WifiSceneMenu]             = &menu_handlers,
    [WifiSceneScanConnect]      = &scan_connect_handlers,
    [WifiSceneStationScan]      = &station_scan_handlers,
    [WifiSceneMacTrack]         = &mac_track_handlers,
    [WifiSceneWardrive]         = &wardrive_handlers,
    [WifiSceneStationWardrive]  = &station_wardrive_handlers,
    [WifiSceneSignalMonitor]    = &signal_monitor_handlers,

    [WifiSceneSnifferMenu]      = &sniffer_menu_handlers,
    [WifiSceneSniffAll]         = &sniff_all_handlers,
    [WifiSceneSniffBeacon]      = &sniff_beacon_handlers,
    [WifiSceneSniffProbe]       = &sniff_probe_handlers,
    [WifiSceneSniffDeauth]      = &sniff_deauth_handlers,
    [WifiSceneSniffEapol]       = &sniff_eapol_handlers,
    [WifiSceneSniffPwnagotchi]  = &sniff_pwnagotchi_handlers,
    [WifiSceneSniffSae]         = &sniff_sae_handlers,

    [WifiSceneAttackMenu]       = &attack_menu_handlers,
    [WifiSceneAttackDeauth]     = &attack_deauth_handlers,
    [WifiSceneAttackBeacon]     = &attack_beacon_handlers,
    [WifiSceneAttackClone]      = &attack_clone_handlers,
    [WifiSceneAttackRickroll]   = &attack_rickroll_handlers,
    [WifiSceneAttackEvilPortal] = &attack_evil_portal_handlers,
    [WifiSceneAttackProbeFlood] = &attack_probe_flood_handlers,
    [WifiSceneAttackKarma]      = &attack_karma_handlers,
    [WifiSceneAttackKarmaPortal]= &attack_karma_portal_handlers,

    [WifiSceneNetMenu]          = &net_menu_handlers,
    [WifiSceneNetPing]          = &net_ping_handlers,
    [WifiSceneNetArp]           = &net_arp_handlers,
    [WifiSceneNetSsh]           = &net_ssh_handlers,
    [WifiSceneNetTelnet]        = &net_telnet_handlers,
    [WifiSceneNetPorts]         = &net_ports_handlers,

    [WifiSceneGeneralMenu]          = &general_menu_handlers,
    [WifiSceneGeneralViewInfo]      = &gen_view_info_handlers,
    [WifiSceneGeneralSelectAps]     = &gen_select_aps_handlers,
    [WifiSceneGeneralSelectStas]    = &gen_select_stas_handlers,
    [WifiSceneGeneralSaveAps]       = &gen_save_aps_handlers,
    [WifiSceneGeneralLoadAps]       = &gen_load_aps_handlers,
    [WifiSceneGeneralClearAps]      = &gen_clear_aps_handlers,
    [WifiSceneGeneralLoadSsids]     = &gen_load_ssids_handlers,
    [WifiSceneGeneralClearSsids]    = &gen_clear_ssids_handlers,
    [WifiSceneGeneralJoin]          = &gen_join_handlers,
    [WifiSceneGeneralSetMacs]       = &gen_set_macs_handlers,
    [WifiSceneGeneralSetChan]       = &gen_set_chan_handlers,
    [WifiSceneGeneralShutdown]      = &gen_shutdown_handlers,
    [WifiSceneGeneralSetEpSsid]     = &gen_ep_ssid_handlers,
    [WifiSceneGeneralSelectEpHtml]  = &gen_ep_html_handlers,

    [WifiSceneZigbee]           = &zigbee_handlers,
    [WifiSceneThread]           = &thread_handlers,

#ifdef M1_APP_WIFI_CONNECT_ENABLE
    [WifiSceneSaved]            = &saved_handlers,
    [WifiSceneStatus]           = &status_handlers,
    [WifiSceneDisconnect]       = &disconnect_handlers,
#endif
};

/* ---- Entry point -------------------------------------------------------- */
void wifi_scene_entry(void)
{
    m1_scene_run(scene_registry, WifiSceneCount, menu_wifi_init, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
