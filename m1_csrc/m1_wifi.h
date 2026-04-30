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

void menu_wifi_init(void);
void menu_wifi_exit(void);

void wifi_scan_ap(void);

/* Station scan (client discovery) */
void wifi_station_scan(void);

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

#endif /* M1_WIFI_H_ */
