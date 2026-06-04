/* See COPYING.txt for license details. */

/**
 * @file   wifi_sta_record.h
 * @brief  WiFi station (client) record struct — no HAL, RTOS, or display deps.
 *
 * Extracted from m1_wifi.c following the Preferred Modularization Pattern.
 * Promoting wifi_sta_t from a file-local typedef to a shared header enables
 * pure-logic extraction of selection counters (wifi_selection.h) and the
 * deauth command builder (wifi_deauth_cmd.h).
 *
 * M1 Project
 */

#ifndef WIFI_STA_RECORD_H_
#define WIFI_STA_RECORD_H_

#include <stdint.h>
#include <stdbool.h>

/** One station (client) entry discovered during promiscuous STA scan. */
typedef struct {
    uint8_t  mac[6];      /**< Station MAC address (raw bytes) */
    int8_t   rssi;        /**< Signal strength in dBm */
    uint8_t  channel;     /**< IEEE 802.11 channel number observed */
    uint8_t  bssid[6];   /**< BSSID of the AP the station is associated with */
    bool     selected;    /**< UI selection flag (set by user) */
    char     ssid[33];    /**< SSID of the associated AP (null-terminated) */
    char     mac_str[18]; /**< "XX:XX:XX:XX:XX:XX" formatted station MAC */
} wifi_sta_t;

/** Maximum number of STA entries held in memory. */
#define WIFI_STA_MAX 64

#endif /* WIFI_STA_RECORD_H_ */
