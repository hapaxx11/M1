/* See COPYING.txt for license details. */

/**
 * @file   wifi_ap_record.h
 * @brief  Pure-logic WiFi AP record helpers — no HAL, RTOS, or display deps.
 *
 * Extracted from m1_wifi.c following the Preferred Modularization Pattern.
 * All functions are testable on the host via the stub-based extraction pattern.
 *
 * Covers:
 *   - wifi_ap_t          — AP record struct (formerly static in m1_wifi.c)
 *   - wifi_ap_record_parse_one() — binary ESP32 payload → wifi_ap_t
 *   - wifi_bssid_fmt()   — 6-byte BSSID → "XX:XX:XX:XX:XX:XX" string
 *   - wifi_bssid_parse() — "XX:XX:XX:XX:XX:XX" string → 6-byte BSSID
 *   - wifi_mac_is_nonzero()  — check a MAC is not all-zero
 *   - wifi_sanitize_field()  — strip TSV control characters from a field
 *   - wifi_csv_quote_field() — RFC 4180 double-quote–encode a CSV field
 *
 * M1 Project
 */

#ifndef WIFI_AP_RECORD_H_
#define WIFI_AP_RECORD_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * AP record struct
 * =========================================================================*/

/** One Access Point entry parsed from an ESP32 CMD_WIFI_SCAN_NEXT payload. */
typedef struct {
    int8_t   rssi;         /**< Signal strength in dBm */
    uint8_t  channel;      /**< IEEE 802.11 channel number */
    uint8_t  auth_mode;    /**< Authentication mode (0 = Open, 2 = WPA, 3+ = WPA2+) */
    uint8_t  bssid[6];     /**< Raw BSSID bytes */
    bool     selected;     /**< UI selection flag (set by user) */
    char     ssid[33];     /**< Null-terminated SSID (empty string = hidden) */
    char     bssid_str[18]; /**< "XX:XX:XX:XX:XX:XX" formatted BSSID */
} wifi_ap_t;

/* Minimum number of payload bytes required by wifi_ap_record_parse_one() */
#define WIFI_AP_RECORD_PAYLOAD_MIN_LEN 10u

/* =========================================================================
 * Binary payload parsing
 * =========================================================================*/

/**
 * Parse one AP record from a raw CMD_WIFI_SCAN_NEXT binary payload.
 *
 * Wire layout (matches ESP32 firmware m1_protocol.h):
 *   [0]      RSSI (int8_t signed)
 *   [1]      channel
 *   [2]      auth_mode
 *   [3..8]   BSSID (6 bytes, raw)
 *   [9]      SSID length (0–32)
 *   [10..]   SSID bytes (no null terminator on wire)
 *
 * The function also formats bssid_str and null-terminates ssid.
 * The selected flag is initialised to false.
 *
 * @param payload     Raw payload bytes from m1_resp_t.payload[].
 * @param payload_len Number of valid bytes in payload[].
 * @param out         Destination record; fully zeroed on entry by caller or
 *                    by this function — output is always consistent.
 * @return true on success, false if payload is too short. An SSID length
 *         field greater than 32 is clamped to 32 rather than rejected.
 */
bool wifi_ap_record_parse_one(const uint8_t *payload,
                               uint8_t        payload_len,
                               wifi_ap_t     *out);

/* =========================================================================
 * BSSID helpers
 * =========================================================================*/

/**
 * Format a 6-byte BSSID as an upper-case "XX:XX:XX:XX:XX:XX" string.
 *
 * @param bssid  6-byte BSSID array.
 * @param out    Output buffer — must be at least 18 bytes (17 chars + NUL).
 */
void wifi_bssid_fmt(const uint8_t bssid[6], char out[18]);

/**
 * Parse a "XX:XX:XX:XX:XX:XX" BSSID string (case-insensitive) into bytes.
 *
 * @param s      Input string.
 * @param bssid  Output 6-byte array.
 * @return true on success, false if the string does not match the expected
 *         format or any octet value exceeds 0xFF.
 */
bool wifi_bssid_parse(const char *s, uint8_t bssid[6]);

/**
 * Return true if at least one byte of the MAC address is nonzero.
 *
 * @param mac  6-byte MAC/BSSID array.
 */
bool wifi_mac_is_nonzero(const uint8_t mac[6]);

/* =========================================================================
 * Field sanitization / CSV helpers
 * =========================================================================*/

/**
 * Copy src into dst, replacing tab, CR, and LF characters with spaces.
 * Safe to call with dst == src (in-place sanitize).
 * Always null-terminates dst when dst_len > 0.
 *
 * @param dst      Destination buffer.
 * @param src      Source string (may be NULL — treated as empty).
 * @param dst_len  Size of dst in bytes including the null terminator.
 */
void wifi_sanitize_field(char *dst, const char *src, size_t dst_len);

/**
 * Write src as a RFC 4180 double-quoted CSV field into dst.
 * Embedded double-quote characters are escaped by doubling them ("").
 * CR and LF are replaced with space.  Always null-terminates dst.
 *
 * @param dst      Destination buffer.
 * @param src      Source string (may be NULL — written as "").
 * @param dst_len  Size of dst in bytes including the null terminator.
 */
void wifi_csv_quote_field(char *dst, const char *src, size_t dst_len);

#endif /* WIFI_AP_RECORD_H_ */
