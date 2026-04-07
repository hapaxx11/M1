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

void wifi_scan_ap(void);

#ifdef M1_APP_WIFI_CONNECT_ENABLE
void wifi_saved_networks(void);
void wifi_show_status(void);
void wifi_disconnect(void);
uint8_t wifi_sync_rtc(void);
bool wifi_is_connected(void);
const char *wifi_get_connected_ssid(void);
#endif

#endif /* M1_WIFI_H_ */
