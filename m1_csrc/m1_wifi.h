/* See COPYING.txt for license details. */

/*
*
* m1_wifi.h
*
* Library for M1 Wifi
*
* M1 Project
*
*/


#ifndef M1_WIFI_H_
#define M1_WIFI_H_

#include <stdbool.h>
#include "m1_compile_cfg.h"

void menu_wifi_init(void);
void menu_wifi_exit(void);

void wifi_scan_ap(void);

/* Selected-network Target context (WiFi cleanup plan §3.2).
 *
 * wifi_scan_ap() flags a pending target when the user presses OK on a
 * non-connected AP in the scan list; the Scan & Connect scene delegate then
 * opens the Target menu.  The wifi_target_* actions operate on that
 * highlighted AP and are only reachable once a network has been selected. */
bool wifi_scan_ap_target_selected(void);
bool wifi_target_valid(void);
const char *wifi_target_ssid(void);
void wifi_target_connect(void);
void wifi_target_deauth(void);
void wifi_target_handshake(void);
void wifi_target_beacon(void);
void wifi_target_pmkid(void);
void wifi_target_cycle(void);

/* Station scan (client discovery) */
void wifi_station_scan(void);
void wifi_survey_24g(void);

/* Sniffer modes */
void wifi_sniff_all(void);
void wifi_sniff_beacon(void);
void wifi_sniff_probe(void);
void wifi_sniff_deauth(void);
void wifi_sniff_eapol(void);
void wifi_sniff_pwnagotchi(void);
void wifi_sniff_sae(void);
void wifi_signal_monitor(void);
void wifi_mac_track(void);
void wifi_wardrive(void);
void wifi_station_wardrive(void);

/* Network scanners */
void wifi_scan_ping(void);
void wifi_scan_arp(void);
void wifi_scan_ssh(void);
void wifi_scan_telnet(void);
void wifi_scan_ports(void);

/* Attack modes */
void wifi_attack_deauth(void);
void wifi_attack_beacon(void);
void wifi_attack_ap_clone(void);
void wifi_attack_rickroll(void);
void wifi_evil_portal(void);
void wifi_probe_flood(void);
void wifi_attack_karma(void);
void wifi_attack_karma_portal(void);
void wifi_pmkid_at(void);

/* WiFi General / Config */
void wifi_general_view_ap_info(void);
void wifi_general_select_aps(void);
void wifi_general_select_stations(void);
void wifi_general_save_aps(void);
void wifi_general_load_aps(void);
void wifi_general_clear_aps(void);
void wifi_general_load_ssids(void);
void wifi_general_clear_ssids(void);
void wifi_general_join_wifi(void);
void wifi_general_set_macs(void);
void wifi_general_set_channel(void);
void wifi_general_shutdown_wifi(void);
void wifi_general_set_ep_ssid(void);
void wifi_general_select_ep_html(void);

/* Legacy AT-layer stubs — gated by compile flag */
#ifdef M1_APP_WIFI_CONNECT_ENABLE
bool wifi_is_connected(void);
const char *wifi_get_connected_ssid(void);
uint8_t wifi_sync_rtc(void);
void wifi_ntp_background_sync(void);
void wifi_saved_networks(void);
void wifi_show_status(void);
void wifi_disconnect(void);

/**
 * @brief  Check if WiFi is connected and prompt user to disconnect.
 *
 * If WiFi is currently connected, shows a confirmation dialog asking the
 * user whether to disconnect.  If the user confirms, performs the disconnect
 * and returns true.  If the user declines, returns false (caller should
 * abort the operation).  If WiFi is not connected, returns true immediately.
 *
 * Intended for use by Sniffer, Attack, and Recon sub-menu on_enter handlers
 * that cannot operate while a WiFi connection is active.
 *
 * @retval true   WiFi is not connected (or was disconnected successfully).
 * @retval false  User declined to disconnect — caller should pop/abort.
 */
bool wifi_prompt_disconnect(void);

/**
 * @brief  Check if WiFi is connected; show error if not.
 *
 * Shows a dismissible "Join WiFi first" message if WiFi is not
 * connected.  Intended for any feature/scene that requires an active connection
 * (e.g. Connected menu, Net Scan tools).
 * @retval true   WiFi is connected — proceed.
 * @retval false  WiFi is not connected — caller should pop/abort.
 */
bool wifi_require_connected(void);
#endif

#endif /* M1_WIFI_H_ */
