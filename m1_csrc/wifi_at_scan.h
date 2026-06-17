/* See COPYING.txt for license details. */
/**
 * @file   wifi_at_scan.h
 * @brief  Pure-logic helpers for AT+CWLAP scan and AT+M1PMKID response parsing.
 *
 * Parses responses produced by the dag T-800 ESP32 AT firmware:
 *   - AT+CWLAP  → list of visible access points
 *   - AT+M1PMKID → targeted PMKID capture result
 *
 * Hardware-independent: no FreeRTOS, no HAL, no SPI.  Host-testable.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/** Maximum SSID length including null terminator. */
#define WIFI_AT_CWLAP_SSID_MAX  33

/** Maximum number of APs that wifi_at_cwlap_parse() will return. */
#define WIFI_AT_CWLAP_MAX_APS   32

/**
 * Single AP record parsed from one AT+CWLAP response line.
 */
typedef struct {
    char    ssid[WIFI_AT_CWLAP_SSID_MAX]; /**< SSID (null-terminated; empty = hidden) */
    uint8_t bssid[6];                      /**< AP MAC address (6 bytes)               */
    int8_t  rssi;                          /**< Signal strength in dBm                 */
    uint8_t channel;                       /**< RF channel (1-14)                      */
} wifi_at_ap_t;

/**
 * Parse an AT+CWLAP response buffer into @p out[].
 *
 * Accepts lines of the form:
 *   +CWLAP:(ecn,"ssid",rssi,"mac",channel[,...extra fields...])
 * Extra fields beyond channel are ignored.  Hidden SSIDs (empty string) are
 * included.  Lines that do not match the expected format are skipped.
 *
 * @param resp    NUL-terminated AT response buffer (may contain "OK"/"ERROR").
 * @param out     Output array; must have space for at least @p max_out entries.
 * @param max_out Maximum number of entries to write into @p out.
 * @return        Number of APs successfully parsed (0 if none or @p resp is NULL).
 */
uint8_t wifi_at_cwlap_parse(const char *resp, wifi_at_ap_t *out, uint8_t max_out);

/**
 * Format a 6-byte BSSID into "xx:xx:xx:xx:xx:xx" (lowercase hex with colons).
 * @p dst must point to a buffer of at least 18 bytes.
 */
void wifi_at_format_bssid(const uint8_t bssid[6], char *dst);

/**
 * Parse a successful AT+M1PMKID response.
 *
 * Expected format in @p resp (from dag T-800 firmware):
 *   +M1PMKID:<bssid>,<32_hex_chars>\r\nOK\r\n
 *
 * Error responses (+M1PMKID:ERR:... or ERROR) return false.
 *
 * @param resp      NUL-terminated AT response buffer.
 * @param bssid_out Receives the 6-byte AP BSSID on success.
 * @param pmkid_out Receives the 16-byte PMKID on success.
 * @return          true on success; false on any parse or format error.
 */
bool wifi_at_pmkid_parse(const char *resp,
                          uint8_t bssid_out[6], uint8_t pmkid_out[16]);
