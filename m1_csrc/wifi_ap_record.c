/* See COPYING.txt for license details. */

/**
 * @file   wifi_ap_record.c
 * @brief  Pure-logic WiFi AP record helpers — no HAL, RTOS, or display deps.
 *
 * All functions are hardware-independent and testable on the host.
 *
 * M1 Project
 */

#include "wifi_ap_record.h"

#include <string.h>
#include <stdio.h>   /* sprintf */

/* =========================================================================
 * Binary payload parsing
 * =========================================================================*/

bool wifi_ap_record_parse_one(const uint8_t *payload,
                               uint8_t        payload_len,
                               wifi_ap_t     *out)
{
    if (!payload || !out || payload_len < WIFI_AP_RECORD_PAYLOAD_MIN_LEN)
        return false;

    memset(out, 0, sizeof(*out));

    out->rssi      = (int8_t)payload[0];
    out->channel   = payload[1];
    out->auth_mode = payload[2];
    memcpy(out->bssid, &payload[3], 6);

    uint8_t ssid_len = payload[9];
    if (ssid_len > 32u)
        ssid_len = 32u;

    /* Guard: ensure the payload actually contains ssid_len SSID bytes */
    if ((uint16_t)10u + ssid_len > (uint16_t)payload_len)
        return false;

    memcpy(out->ssid, &payload[10], ssid_len);
    out->ssid[ssid_len] = '\0';

    wifi_bssid_fmt(out->bssid, out->bssid_str);

    out->selected = false;
    return true;
}

/* =========================================================================
 * BSSID helpers
 * =========================================================================*/

void wifi_bssid_fmt(const uint8_t bssid[6], char out[18])
{
    if (!bssid || !out)
        return;

    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
            bssid[0], bssid[1], bssid[2],
            bssid[3], bssid[4], bssid[5]);
}

bool wifi_bssid_parse(const char *s, uint8_t bssid[6])
{
    unsigned int b[6];

    if (!s || !bssid)
        return false;

    if (sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x",   /* NOLINT */
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
        return false;

    for (uint8_t i = 0u; i < 6u; i++)
    {
        if (b[i] > 0xFFu)
            return false;
        bssid[i] = (uint8_t)b[i];
    }
    return true;
}

bool wifi_mac_is_nonzero(const uint8_t mac[6])
{
    if (!mac)
        return false;
    return mac[0] || mac[1] || mac[2] || mac[3] || mac[4] || mac[5];
}

/* =========================================================================
 * Field sanitization / CSV helpers
 * =========================================================================*/

void wifi_sanitize_field(char *dst, const char *src, size_t dst_len)
{
    size_t i;

    if (!dst || dst_len == 0u)
        return;

    for (i = 0u; i + 1u < dst_len && src && src[i]; i++)
    {
        char c = src[i];
        dst[i] = (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
    }
    dst[i] = '\0';
}

void wifi_csv_quote_field(char *dst, const char *src, size_t dst_len)
{
    size_t di = 0u;

    if (!dst || dst_len == 0u)
        return;

    dst[di++] = '"';

    for (size_t si = 0u; src && src[si] && di + 2u < dst_len; si++)
    {
        char c = src[si];

        if (c == '\r' || c == '\n')
            c = ' ';

        if (c == '"')
        {
            if (di + 3u >= dst_len)
                break;
            dst[di++] = '"';
            dst[di++] = '"';
        }
        else
        {
            dst[di++] = c;
        }
    }

    dst[di++] = '"';
    dst[di]   = '\0';
}
