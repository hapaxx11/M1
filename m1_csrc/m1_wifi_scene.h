/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene.h
 * @brief  WiFi Scene Manager — scene IDs and per-file handler exports.
 *
 * Scene implementation is split across six files following the
 * m1_subghz_scene_*.c convention (Phase D):
 *
 *   m1_wifi_scene_menu.c    — top-level menu + core/direct-tool delegates
 *                             (Scan & Connect, Station Scan, MAC Track,
 *                              Wardrive, Station Wardrive, Signal Monitor,
 *                              Zigbee Scan, Thread Scan)
 *   m1_wifi_scene_sniff.c   — Sniffers sub-menu + 7 sniffer delegates
 *   m1_wifi_scene_attack.c  — Attacks sub-menu + 8 attack delegates
 *   m1_wifi_scene_net.c     — Net Scan sub-menu + 5 network-scanner delegates
 *   m1_wifi_scene_general.c — General sub-menu + 14 general/config delegates
 *   m1_wifi_scene_connect.c — Saved Networks, Status, Disconnect delegates
 *                             (compile-gated by M1_APP_WIFI_CONNECT_ENABLE)
 *
 * m1_wifi_scene.c owns only the scene_registry[] table and wifi_scene_entry().
 */

#ifndef M1_WIFI_SCENE_H_
#define M1_WIFI_SCENE_H_

#include "m1_scene.h"

/*==========================================================================*/
/* Scene IDs                                                                */
/*==========================================================================*/

typedef enum {
    WifiSceneMenu = 0,

    /* Direct tools */
    WifiSceneScanConnect,
    WifiSceneStationScan,
    WifiSceneMacTrack,
    WifiSceneWardrive,
    WifiSceneStationWardrive,
    WifiSceneSignalMonitor,

    /* Sniffers sub-menu */
    WifiSceneSnifferMenu,
    WifiSceneSniffAll,
    WifiSceneSniffBeacon,
    WifiSceneSniffProbe,
    WifiSceneSniffDeauth,
    WifiSceneSniffEapol,
    WifiSceneSniffPwnagotchi,
    WifiSceneSniffSae,

    /* Attacks sub-menu */
    WifiSceneAttackMenu,
    WifiSceneAttackDeauth,
    WifiSceneAttackBeacon,
    WifiSceneAttackClone,
    WifiSceneAttackRickroll,
    WifiSceneAttackEvilPortal,
    WifiSceneAttackProbeFlood,
    WifiSceneAttackKarma,
    WifiSceneAttackKarmaPortal,

    /* Network Scanners sub-menu */
    WifiSceneNetMenu,
    WifiSceneNetPing,
    WifiSceneNetArp,
    WifiSceneNetSsh,
    WifiSceneNetTelnet,
    WifiSceneNetPorts,

    /* General / Config sub-menu */
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

    /* Connect features (AT-layer; compile-gated) */
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    WifiSceneSaved,
    WifiSceneStatus,
    WifiSceneDisconnect,
#endif

    WifiSceneCount
} WifiSceneId;

/*==========================================================================*/
/* Per-file handler exports                                                 */
/*==========================================================================*/

/* m1_wifi_scene_menu.c */
extern const M1SceneHandlers wifi_scene_menu_handlers;
extern const M1SceneHandlers wifi_scene_scan_connect_handlers;
extern const M1SceneHandlers wifi_scene_station_scan_handlers;
extern const M1SceneHandlers wifi_scene_mac_track_handlers;
extern const M1SceneHandlers wifi_scene_wardrive_handlers;
extern const M1SceneHandlers wifi_scene_station_wardrive_handlers;
extern const M1SceneHandlers wifi_scene_signal_monitor_handlers;
extern const M1SceneHandlers wifi_scene_zigbee_handlers;
extern const M1SceneHandlers wifi_scene_thread_handlers;

/* m1_wifi_scene_sniff.c */
extern const M1SceneHandlers wifi_scene_sniffer_menu_handlers;
extern const M1SceneHandlers wifi_scene_sniff_all_handlers;
extern const M1SceneHandlers wifi_scene_sniff_beacon_handlers;
extern const M1SceneHandlers wifi_scene_sniff_probe_handlers;
extern const M1SceneHandlers wifi_scene_sniff_deauth_handlers;
extern const M1SceneHandlers wifi_scene_sniff_eapol_handlers;
extern const M1SceneHandlers wifi_scene_sniff_pwnagotchi_handlers;
extern const M1SceneHandlers wifi_scene_sniff_sae_handlers;

/* m1_wifi_scene_attack.c */
extern const M1SceneHandlers wifi_scene_attack_menu_handlers;
extern const M1SceneHandlers wifi_scene_attack_deauth_handlers;
extern const M1SceneHandlers wifi_scene_attack_beacon_handlers;
extern const M1SceneHandlers wifi_scene_attack_clone_handlers;
extern const M1SceneHandlers wifi_scene_attack_rickroll_handlers;
extern const M1SceneHandlers wifi_scene_attack_evil_portal_handlers;
extern const M1SceneHandlers wifi_scene_attack_probe_flood_handlers;
extern const M1SceneHandlers wifi_scene_attack_karma_handlers;
extern const M1SceneHandlers wifi_scene_attack_karma_portal_handlers;

/* m1_wifi_scene_net.c */
extern const M1SceneHandlers wifi_scene_net_menu_handlers;
extern const M1SceneHandlers wifi_scene_net_ping_handlers;
extern const M1SceneHandlers wifi_scene_net_arp_handlers;
extern const M1SceneHandlers wifi_scene_net_ssh_handlers;
extern const M1SceneHandlers wifi_scene_net_telnet_handlers;
extern const M1SceneHandlers wifi_scene_net_ports_handlers;

/* m1_wifi_scene_general.c */
extern const M1SceneHandlers wifi_scene_general_menu_handlers;
extern const M1SceneHandlers wifi_scene_gen_view_info_handlers;
extern const M1SceneHandlers wifi_scene_gen_select_aps_handlers;
extern const M1SceneHandlers wifi_scene_gen_select_stas_handlers;
extern const M1SceneHandlers wifi_scene_gen_save_aps_handlers;
extern const M1SceneHandlers wifi_scene_gen_load_aps_handlers;
extern const M1SceneHandlers wifi_scene_gen_clear_aps_handlers;
extern const M1SceneHandlers wifi_scene_gen_load_ssids_handlers;
extern const M1SceneHandlers wifi_scene_gen_clear_ssids_handlers;
extern const M1SceneHandlers wifi_scene_gen_join_handlers;
extern const M1SceneHandlers wifi_scene_gen_set_macs_handlers;
extern const M1SceneHandlers wifi_scene_gen_set_chan_handlers;
extern const M1SceneHandlers wifi_scene_gen_shutdown_handlers;
extern const M1SceneHandlers wifi_scene_gen_ep_ssid_handlers;
extern const M1SceneHandlers wifi_scene_gen_ep_html_handlers;

/* m1_wifi_scene_connect.c (compile-gated by M1_APP_WIFI_CONNECT_ENABLE) */
#ifdef M1_APP_WIFI_CONNECT_ENABLE
extern const M1SceneHandlers wifi_scene_saved_handlers;
extern const M1SceneHandlers wifi_scene_status_handlers;
extern const M1SceneHandlers wifi_scene_disconnect_handlers;
#endif

/*==========================================================================*/
/* Entry point                                                              */
/*==========================================================================*/

void wifi_scene_entry(void);

#endif /* M1_WIFI_SCENE_H_ */
