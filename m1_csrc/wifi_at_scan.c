/* See COPYING.txt for license details. */
/**
 * @file   wifi_at_scan.c
 * @brief  Pure-logic helpers for AT+CWLAP scan and AT+M1PMKID response parsing.
 *
 * Hardware-independent: no FreeRTOS, no HAL, no SPI.  Host-testable.
 *
 * These functions parse text responses produced by the dag T-800 ESP32 AT
 * firmware.  The calling code in m1_wifi.c is responsible for the SPI
 * transport (spi_AT_send_recv) and display.
 */

#include "wifi_at_scan.h"
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* Internal helpers                                                          */
/*---------------------------------------------------------------------------*/

/** Return true if c is a valid lowercase or uppercase hex digit. */
static bool is_hex(char c)
{
    return ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'));
}

/** Convert a two-character hex string (no null required) to a byte. */
static uint8_t hex2byte(const char *s)
{
    unsigned int v = 0;
    sscanf(s, "%02x", &v);
    return (uint8_t)v;
}

/**
 * Parse one +CWLAP entry (text immediately after "+CWLAP:(").
 * Returns true and fills out on success; false if the entry is malformed.
 *
 * Handles hidden (empty) SSIDs, which sscanf %[^\"] cannot match because
 * the C standard requires at least one character for %[...] to succeed.
 */
static bool cwlap_parse_one(const char *p, wifi_at_ap_t *out)
{
    /* Skip ECN integer */
    while (*p && *p != ',') p++;
    if (*p != ',') return false;
    p++;  /* skip ',' after ECN */

    /* SSID: must start with '"' */
    if (*p != '"') return false;
    p++;  /* skip opening '"' */

    /* Collect SSID until closing '"' (may be empty for hidden networks) */
    const char *ssid_start = p;
    while (*p && *p != '"') p++;
    if (!*p) return false;  /* unterminated */
    size_t ssid_len = (size_t)(p - ssid_start);
    if (ssid_len >= WIFI_AT_CWLAP_SSID_MAX)
        ssid_len = WIFI_AT_CWLAP_SSID_MAX - 1;
    memcpy(out->ssid, ssid_start, ssid_len);
    out->ssid[ssid_len] = '\0';
    p++;  /* skip closing '"' */

    /* Comma between SSID and RSSI */
    if (*p != ',') return false;
    p++;

    /* RSSI, "MAC", channel via sscanf (MAC is never empty so %[^\"] is safe) */
    int rssi = 0, ch = 0;
    char mac[18] = {0};
    if (sscanf(p, "%d,\"%17[^\"]\",%d", &rssi, mac, &ch) != 3) return false;

    /* Validate and decode MAC */
    unsigned int m[6] = {0};
    if (sscanf(mac, "%x:%x:%x:%x:%x:%x",
               &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6)
        return false;

    out->rssi    = (int8_t)rssi;
    out->channel = (uint8_t)ch;
    for (int i = 0; i < 6; i++)
        out->bssid[i] = (uint8_t)m[i];

    return true;
}

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

uint8_t wifi_at_cwlap_parse(const char *resp, wifi_at_ap_t *out, uint8_t max_out)
{
    if (!resp || !out || max_out == 0) return 0;

    uint8_t count = 0;
    const char *p = resp;

    while (count < max_out) {
        /* Locate the next "+CWLAP:(" marker */
        p = strstr(p, "+CWLAP:(");
        if (!p) break;
        p += 8;  /* skip "+CWLAP:(" */

        if (cwlap_parse_one(p, &out[count]))
            count++;

        /* Advance past the closing ')' of this entry to avoid re-matching */
        const char *end = strchr(p, ')');
        if (!end) break;
        p = end + 1;
    }

    return count;
}

void wifi_at_format_bssid(const uint8_t bssid[6], char *dst)
{
    sprintf(dst, "%02x:%02x:%02x:%02x:%02x:%02x",
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}

bool wifi_at_pmkid_parse(const char *resp,
                          uint8_t bssid_out[6], uint8_t pmkid_out[16])
{
    if (!resp || !bssid_out || !pmkid_out) return false;

    /* Reject explicit error responses before searching for success prefix */
    if (strstr(resp, "+M1PMKID:ERR:") || strstr(resp, "ERROR")) return false;

    /* Find "+M1PMKID:" prefix */
    const char *p = strstr(resp, "+M1PMKID:");
    if (!p) return false;
    p += 9;  /* skip "+M1PMKID:" */

    /* Parse "aa:bb:cc:dd:ee:ff,<32hex>" */
    unsigned int m[6] = {0};
    char hex32[35]    = {0};   /* 32 hex chars + possible \r\n + guard */
    if (sscanf(p, "%x:%x:%x:%x:%x:%x,%34s",
               &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], hex32) != 7)
        return false;

    /* Strip any trailing whitespace / line endings from hex32 */
    for (int i = 0; i < 35; i++) {
        if (hex32[i] == '\r' || hex32[i] == '\n' || hex32[i] == '\0') {
            hex32[i] = '\0';
            break;
        }
    }

    /* Validate: must be exactly 32 hex digits */
    if (strlen(hex32) != 32) return false;
    for (int i = 0; i < 32; i++) {
        if (!is_hex(hex32[i])) return false;
    }

    /* Fill BSSID */
    for (int i = 0; i < 6; i++)
        bssid_out[i] = (uint8_t)m[i];

    /* Fill PMKID (16 bytes from 32 hex chars) */
    for (int i = 0; i < 16; i++)
        pmkid_out[i] = hex2byte(&hex32[i * 2]);

    return true;
}
