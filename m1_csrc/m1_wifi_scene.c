/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene.c
 * @brief  WiFi Scene Manager — scene registry and entry point.
 *
 * Scene implementations live in per-group files (Phase D split):
 *   m1_wifi_scene_menu.c    — top menu + core/direct-tool delegates
 *   m1_wifi_scene_sniff.c   — Sniffers sub-menu + sniffer delegates
 *   m1_wifi_scene_attack.c  — Attacks sub-menu + attack delegates
 *   m1_wifi_scene_net.c     — Net Scan sub-menu + network-scanner delegates
 *   m1_wifi_scene_general.c — General sub-menu + general/config delegates
 *   m1_wifi_scene_connect.c — Saved/Status/Disconnect delegates (ifdef-gated)
 */

#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_wifi_scene.h"
#include "m1_scene.h"
#include "m1_wifi.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Scene registry                                                           */
/*==========================================================================*/

static const M1SceneHandlers *const scene_registry[WifiSceneCount] = {
    [WifiSceneMenu]             = &wifi_scene_menu_handlers,
    [WifiSceneScanConnect]      = &wifi_scene_scan_connect_handlers,
    [WifiSceneStationScan]      = &wifi_scene_station_scan_handlers,
    [WifiSceneMacTrack]         = &wifi_scene_mac_track_handlers,
    [WifiSceneWardrive]         = &wifi_scene_wardrive_handlers,
    [WifiSceneStationWardrive]  = &wifi_scene_station_wardrive_handlers,
    [WifiSceneSignalMonitor]    = &wifi_scene_signal_monitor_handlers,

    [WifiSceneSnifferMenu]      = &wifi_scene_sniffer_menu_handlers,
    [WifiSceneSniffAll]         = &wifi_scene_sniff_all_handlers,
    [WifiSceneSniffBeacon]      = &wifi_scene_sniff_beacon_handlers,
    [WifiSceneSniffProbe]       = &wifi_scene_sniff_probe_handlers,
    [WifiSceneSniffDeauth]      = &wifi_scene_sniff_deauth_handlers,
    [WifiSceneSniffEapol]       = &wifi_scene_sniff_eapol_handlers,
    [WifiSceneSniffPwnagotchi]  = &wifi_scene_sniff_pwnagotchi_handlers,
    [WifiSceneSniffSae]         = &wifi_scene_sniff_sae_handlers,

    [WifiSceneAttackMenu]       = &wifi_scene_attack_menu_handlers,
    [WifiSceneAttackDeauth]     = &wifi_scene_attack_deauth_handlers,
    [WifiSceneAttackBeacon]     = &wifi_scene_attack_beacon_handlers,
    [WifiSceneAttackClone]      = &wifi_scene_attack_clone_handlers,
    [WifiSceneAttackRickroll]   = &wifi_scene_attack_rickroll_handlers,
    [WifiSceneAttackEvilPortal] = &wifi_scene_attack_evil_portal_handlers,
    [WifiSceneAttackProbeFlood] = &wifi_scene_attack_probe_flood_handlers,
    [WifiSceneAttackKarma]      = &wifi_scene_attack_karma_handlers,
    [WifiSceneAttackKarmaPortal]= &wifi_scene_attack_karma_portal_handlers,

    [WifiSceneNetMenu]          = &wifi_scene_net_menu_handlers,
    [WifiSceneNetPing]          = &wifi_scene_net_ping_handlers,
    [WifiSceneNetArp]           = &wifi_scene_net_arp_handlers,
    [WifiSceneNetSsh]           = &wifi_scene_net_ssh_handlers,
    [WifiSceneNetTelnet]        = &wifi_scene_net_telnet_handlers,
    [WifiSceneNetPorts]         = &wifi_scene_net_ports_handlers,

    [WifiSceneGeneralMenu]          = &wifi_scene_general_menu_handlers,
    [WifiSceneGeneralViewInfo]      = &wifi_scene_gen_view_info_handlers,
    [WifiSceneGeneralSelectAps]     = &wifi_scene_gen_select_aps_handlers,
    [WifiSceneGeneralSelectStas]    = &wifi_scene_gen_select_stas_handlers,
    [WifiSceneGeneralSaveAps]       = &wifi_scene_gen_save_aps_handlers,
    [WifiSceneGeneralLoadAps]       = &wifi_scene_gen_load_aps_handlers,
    [WifiSceneGeneralClearAps]      = &wifi_scene_gen_clear_aps_handlers,
    [WifiSceneGeneralLoadSsids]     = &wifi_scene_gen_load_ssids_handlers,
    [WifiSceneGeneralClearSsids]    = &wifi_scene_gen_clear_ssids_handlers,
    [WifiSceneGeneralJoin]          = &wifi_scene_gen_join_handlers,
    [WifiSceneGeneralSetMacs]       = &wifi_scene_gen_set_macs_handlers,
    [WifiSceneGeneralSetChan]       = &wifi_scene_gen_set_chan_handlers,
    [WifiSceneGeneralShutdown]      = &wifi_scene_gen_shutdown_handlers,
    [WifiSceneGeneralSetEpSsid]     = &wifi_scene_gen_ep_ssid_handlers,
    [WifiSceneGeneralSelectEpHtml]  = &wifi_scene_gen_ep_html_handlers,

    [WifiSceneZigbee]           = &wifi_scene_zigbee_handlers,
    [WifiSceneThread]           = &wifi_scene_thread_handlers,

#ifdef M1_APP_WIFI_CONNECT_ENABLE
    [WifiSceneSaved]            = &wifi_scene_saved_handlers,
    [WifiSceneStatus]           = &wifi_scene_status_handlers,
    [WifiSceneDisconnect]       = &wifi_scene_disconnect_handlers,
#endif
};

/*==========================================================================*/
/* Entry point                                                              */
/*==========================================================================*/

void wifi_scene_entry(void)
{
    m1_scene_run(scene_registry, WifiSceneCount, menu_wifi_init, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
