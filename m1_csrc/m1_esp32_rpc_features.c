/* See COPYING.txt for license details. */

/**
 * @file   m1_esp32_rpc_features.c
 * @brief  Per-feature M1_RPC action layer for the native brain CD3 firmware.
 *
 * See m1_esp32_rpc_features.h for the design rationale.  Each function builds
 * the canonical little-endian payload for one feature action, dispatches it via
 * the shared m1_esp32_rpc_call() client, and (where the reply carries data)
 * decodes it into a neutral out-struct.  The client's transport is injectable,
 * so this whole layer runs under host tests without an ESP32.
 *
 * M1 Project
 */

#include <string.h>

#include "m1_esp32_rpc_features.h"

/*==========================================================================*/
/* Feature -> opcode map                                                    */
/*==========================================================================*/

bool esp32_feature_rpc_opcode(esp32_feature_id_t fid, m1_esp32_rpc_id_t *out_op)
{
    m1_esp32_rpc_id_t op;

    switch (fid) {
    case ESP32_FEATURE_WIFI_SCAN:      op = M1_ESP32_RPC_WIFI_SCAN;          break;
    case ESP32_FEATURE_STA_SCAN:       op = M1_ESP32_RPC_OFF_STA_SCAN_START; break;
    case ESP32_FEATURE_PKTMON:         op = M1_ESP32_RPC_OFF_MONITOR_START;  break;
    case ESP32_FEATURE_DEAUTH:         op = M1_ESP32_RPC_OFF_DEAUTH_START;   break;
    case ESP32_FEATURE_BEACON:         op = M1_ESP32_RPC_OFF_BEACON_START;   break;
    case ESP32_FEATURE_PROBE_FLOOD:    op = M1_ESP32_RPC_OFF_PROBE_START;    break;
    case ESP32_FEATURE_KARMA:          op = M1_ESP32_RPC_OFF_KARMA_START;    break;
    case ESP32_FEATURE_PORTAL:         op = M1_ESP32_RPC_OFF_CAPTIVE_START;  break;
    case ESP32_FEATURE_WIFI_JOIN:      op = M1_ESP32_RPC_WIFI_CONNECT;       break;
    case ESP32_FEATURE_WIFI_SET_MAC:   op = M1_ESP32_RPC_WIFI_GET_MAC;       break;
    case ESP32_FEATURE_WIFI_SET_CHAN:  op = M1_ESP32_RPC_WIFI_SET_MODE;      break;
    case ESP32_FEATURE_WIFI_DISCONNECT:op = M1_ESP32_RPC_WIFI_DISCONNECT;    break;
    case ESP32_FEATURE_802154:         op = M1_ESP32_RPC_ZB_SNIFF_START;     break;
    case ESP32_FEATURE_BLE_SCAN:       op = M1_ESP32_RPC_BLE_SCAN_START;     break;
    case ESP32_FEATURE_BLE_ADV:        op = M1_ESP32_RPC_BLE_ADV_START;      break;
    case ESP32_FEATURE_BLE_HID:        op = M1_ESP32_RPC_BLE_HID_INIT;       break;
    case ESP32_FEATURE_PMKID:          op = M1_ESP32_RPC_OFF_PMKID_CAPTURE;  break;
    case ESP32_FEATURE_HANDSHAKE:      op = M1_ESP32_RPC_OFF_HS_START;       break;
    case ESP32_FEATURE_OTA:            op = M1_ESP32_RPC_SYS_OTA_BEGIN;      break;
    case ESP32_FEATURE_ESPNOW:         op = M1_ESP32_RPC_NOW_START;          break;
    case ESP32_FEATURE_WIFI_HOTSPOT:   op = M1_ESP32_RPC_SOFTAP_START;       break;

    /* No settled native brain-CD3 opcode: host-composed or AT/binary-only. */
    case ESP32_FEATURE_NETSCAN:
    case ESP32_FEATURE_BLE_GATT:
    case ESP32_FEATURE_BT_MANAGE:
    default:
        return false;
    }

    if (out_op)
        *out_op = op;
    return true;
}

/*==========================================================================*/
/* Internal dispatch helpers                                                */
/*==========================================================================*/

/* Fire-and-check a request with an optional payload, ignoring any response
 * body.  Returns the raw client status so callers can distinguish OK / NAK /
 * transport error. */
static m1_esp32_rpc_status_t rpc_do(uint16_t op, const uint8_t *req,
                                    uint16_t req_len)
{
    uint8_t  resp[16];
    uint16_t rlen = 0u;
    return m1_esp32_rpc_call(op, req, req_len, resp, sizeof(resp), &rlen,
                             M1_ESP32_RPC_FEATURE_TIMEOUT_S);
}

/* No-payload trigger. */
static m1_esp32_rpc_status_t rpc_trigger(uint16_t op)
{
    return rpc_do(op, NULL, 0u);
}

/* Single-byte payload (e.g. channel selector). */
static m1_esp32_rpc_status_t rpc_do_u8(uint16_t op, uint8_t v)
{
    return rpc_do(op, &v, 1u);
}

/*==========================================================================*/
/* WiFi station / AP                                                        */
/*==========================================================================*/

m1_esp32_rpc_status_t m1_esp32_rpc_wifi_scan(m1_esp32_rpc_scan_entry_t *out,
                                             uint8_t max, uint8_t *out_count)
{
    if (out_count) *out_count = 0u;
    if (!out || max == 0u)
        return M1_ESP32_RPC_ERR_INVALID;

    uint8_t  resp[M1_ESP32_RPC_PAYLOAD_MAX];
    uint16_t rlen = 0u;
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_WIFI_SCAN, NULL, 0u, resp, sizeof(resp),
                          &rlen, M1_ESP32_RPC_FEATURE_TIMEOUT_S);
    if (st != M1_ESP32_RPC_OK)
        return st;

    /* RESP: [count:1] then repeated (fixed 10-byte entry + ssid_len ssid). */
    if (rlen < 1u)
        return M1_ESP32_RPC_ERR_BAD_FRAME;

    uint8_t  want   = resp[0];
    uint16_t off    = 1u;
    uint8_t  got    = 0u;
    const uint16_t ENTRY = (uint16_t)sizeof(m1_esp32_rpc_scan_entry_t);

    while (got < want && got < max) {
        if ((uint16_t)(off + ENTRY) > rlen)
            break;  /* truncated frame — stop at last whole entry */
        memcpy(&out[got], &resp[off], ENTRY);
        off = (uint16_t)(off + ENTRY + out[got].ssid_len);
        if (off > rlen) {         /* ssid text overran the frame */
            got++;
            break;
        }
        got++;
    }

    if (out_count) *out_count = got;
    return M1_ESP32_RPC_OK;
}

m1_esp32_rpc_status_t m1_esp32_rpc_wifi_disconnect(void)
{
    return rpc_trigger(M1_ESP32_RPC_WIFI_DISCONNECT);
}

m1_esp32_rpc_status_t m1_esp32_rpc_wifi_set_mac(const uint8_t mac[6])
{
    if (!mac)
        return M1_ESP32_RPC_ERR_INVALID;
    return rpc_do(M1_ESP32_RPC_WIFI_GET_MAC, mac, 6u);
}

m1_esp32_rpc_status_t m1_esp32_rpc_softap_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_SOFTAP_STOP);
}

/*==========================================================================*/
/* Offensive WiFi                                                           */
/*==========================================================================*/

m1_esp32_rpc_status_t m1_esp32_rpc_monitor_start(uint8_t channel)
{
    return rpc_do_u8(M1_ESP32_RPC_OFF_MONITOR_START, channel);
}

m1_esp32_rpc_status_t m1_esp32_rpc_monitor_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_OFF_MONITOR_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_deauth_start(const m1_esp32_rpc_deauth_req_t *req)
{
    if (!req)
        return M1_ESP32_RPC_ERR_INVALID;
    return rpc_do(M1_ESP32_RPC_OFF_DEAUTH_START, (const uint8_t *)req,
                  (uint16_t)sizeof(*req));
}

m1_esp32_rpc_status_t m1_esp32_rpc_deauth_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_OFF_DEAUTH_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_beacon_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_OFF_BEACON_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_probe_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_OFF_PROBE_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_karma_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_OFF_KARMA_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_captive_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_OFF_CAPTIVE_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_handshake_start(uint8_t channel)
{
    return rpc_do_u8(M1_ESP32_RPC_OFF_HS_START, channel);
}

m1_esp32_rpc_status_t m1_esp32_rpc_handshake_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_OFF_HS_STOP);
}

/*==========================================================================*/
/* BLE                                                                      */
/*==========================================================================*/

m1_esp32_rpc_status_t m1_esp32_rpc_ble_init(void)
{
    return rpc_trigger(M1_ESP32_RPC_BLE_INIT);
}

m1_esp32_rpc_status_t m1_esp32_rpc_ble_scan_start(void)
{
    return rpc_trigger(M1_ESP32_RPC_BLE_SCAN_START);
}

m1_esp32_rpc_status_t m1_esp32_rpc_ble_adv_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_BLE_ADV_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_ble_spam_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_BLE_SPAM_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_ble_hid_init(const char *name)
{
    uint8_t payload[31];
    uint16_t nlen = 0u;
    if (name) {
        size_t n = strlen(name);
        if (n > sizeof(payload)) n = sizeof(payload);
        memcpy(payload, name, n);
        nlen = (uint16_t)n;
    }
    return rpc_do(M1_ESP32_RPC_BLE_HID_INIT, payload, nlen);
}

m1_esp32_rpc_status_t m1_esp32_rpc_ble_hid_key(uint8_t modifier,
                                               const uint8_t *keys,
                                               uint8_t key_count)
{
    /* Wire layout: [modifier:1][key_count:1][keys...], max 6 usage ids. */
    if (key_count > 6u || (key_count > 0u && !keys))
        return M1_ESP32_RPC_ERR_INVALID;

    uint8_t payload[2u + 6u];
    payload[0] = modifier;
    payload[1] = key_count;
    if (key_count)
        memcpy(&payload[2], keys, key_count);
    return rpc_do(M1_ESP32_RPC_BLE_HID_KEY, payload, (uint16_t)(2u + key_count));
}

m1_esp32_rpc_status_t m1_esp32_rpc_ble_hid_deinit(void)
{
    return rpc_trigger(M1_ESP32_RPC_BLE_HID_DEINIT);
}

/*==========================================================================*/
/* IEEE 802.15.4 (Zigbee / Thread)                                          */
/*==========================================================================*/

m1_esp32_rpc_status_t m1_esp32_rpc_zb_init(void)
{
    return rpc_trigger(M1_ESP32_RPC_ZB_INIT);
}

m1_esp32_rpc_status_t m1_esp32_rpc_zb_sniff_start(uint8_t channel)
{
    return rpc_do_u8(M1_ESP32_RPC_ZB_SNIFF_START, channel);
}

m1_esp32_rpc_status_t m1_esp32_rpc_zb_sniff_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_ZB_SNIFF_STOP);
}

m1_esp32_rpc_status_t m1_esp32_rpc_zb_sniff_get(m1_esp32_rpc_zb_device_t *out,
                                                uint8_t max, uint8_t *out_count)
{
    if (out_count) *out_count = 0u;
    if (!out || max == 0u)
        return M1_ESP32_RPC_ERR_INVALID;

    uint8_t  resp[M1_ESP32_RPC_PAYLOAD_MAX];
    uint16_t rlen = 0u;
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(M1_ESP32_RPC_ZB_SNIFF_GET, NULL, 0u, resp,
                          sizeof(resp), &rlen, M1_ESP32_RPC_FEATURE_TIMEOUT_S);
    if (st != M1_ESP32_RPC_OK)
        return st;

    /* RESP: [count:1][fixed-size device record × count]. */
    if (rlen < 1u)
        return M1_ESP32_RPC_ERR_BAD_FRAME;

    uint8_t  want = resp[0];
    uint16_t off  = 1u;
    uint8_t  got  = 0u;
    const uint16_t REC = (uint16_t)sizeof(m1_esp32_rpc_zb_device_t);

    while (got < want && got < max && (uint16_t)(off + REC) <= rlen) {
        memcpy(&out[got], &resp[off], REC);
        off = (uint16_t)(off + REC);
        got++;
    }

    if (out_count) *out_count = got;
    return M1_ESP32_RPC_OK;
}

m1_esp32_rpc_status_t m1_esp32_rpc_zb_flood_start(uint8_t channel)
{
    return rpc_do_u8(M1_ESP32_RPC_ZB_FLOOD_START, channel);
}

m1_esp32_rpc_status_t m1_esp32_rpc_zb_flood_stop(void)
{
    return rpc_trigger(M1_ESP32_RPC_ZB_FLOOD_STOP);
}
