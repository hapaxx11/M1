/* See COPYING.txt for license details. */

/*
 * m1_esp32_rpc_features.h
 *
 * Per-feature M1_RPC action layer for the native "brain" CD3 firmware.
 *
 * Background
 * ----------
 * m1_esp32_rpc.c provides the generic request/response client
 * (m1_esp32_rpc_call()).  This module is the next layer up: one small,
 * host-testable function per ESP32-dependent WiFi / BLE / 802.15.4 feature that
 * builds the canonical little-endian payload, dispatches it via the shared
 * client, and decodes the reply into a neutral out-struct.  Together with
 * esp32_feature_rpc_opcode() (the feature -> opcode map) it is the concrete
 * realisation of "branch on m1_esp32_active_transport() and call
 * m1_esp32_rpc_call() with the matching opcode" for every feature.
 *
 * A feature module wires this in with:
 *
 *   if (m1_esp32_active_transport() == ESP32_TRANSPORT_RPC) {
 *       st = m1_esp32_rpc_wifi_scan(list, MAX, &n);   // brain CD3
 *   } else {
 *       ... existing AT / binary-SPI path ...
 *   }
 *
 * Only payloads whose wire layout is defined by the canonical structs in
 * m1_esp32_rpc.h (mirrored from bedge117/m1-esp32-brain m1_rpc.h) or that are
 * unambiguous scalars (channel byte, MAC) are encoded here.  Opcodes without a
 * settled binary payload are reachable through esp32_feature_rpc_opcode() +
 * m1_esp32_rpc_call() directly.
 *
 * Pure w.r.t. hardware: every function funnels through m1_esp32_rpc_call(),
 * whose transport is injectable (m1_esp32_rpc_set_transport()), so the whole
 * layer is exercised on the host without an ESP32.
 *
 * M1 Project
 */

#ifndef M1_ESP32_RPC_FEATURES_H_
#define M1_ESP32_RPC_FEATURES_H_

#include <stdint.h>
#include <stdbool.h>

#include "m1_esp32_rpc.h"       /* client + opcode enum + payload structs */
#include "esp32_feature_map.h"  /* esp32_feature_id_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Default per-call response timeout (seconds) shared by the wrappers below.
 * Matches the ESP-NOW / CD3 detection probe timeout — generous for the prompt
 * single-transaction control commands these features issue. */
#define M1_ESP32_RPC_FEATURE_TIMEOUT_S  2

/* =========================================================================
 * Feature -> RPC opcode map
 * =========================================================================*/

/**
 * Resolve the primary M1_RPC opcode a feature drives on the native brain CD3.
 *
 * "Primary" is the start / trigger command for the feature (e.g. WIFI_SCAN for
 * ESP32_FEATURE_WIFI_SCAN, OFF_DEAUTH_START for ESP32_FEATURE_DEAUTH).  Some
 * features are composed on the host from several opcodes or have no native
 * brain-CD3 command; those return false and leave @p *out_op untouched.
 *
 * Pure logic — no HAL, no global state.
 *
 * @param fid     Feature identifier.
 * @param out_op  [out] receives the opcode on success (may be NULL to query
 *                only whether a mapping exists).
 * @return true if @p fid has a native M1_RPC opcode, false otherwise.
 */
bool esp32_feature_rpc_opcode(esp32_feature_id_t fid, m1_esp32_rpc_id_t *out_op);

/* =========================================================================
 * WiFi station / AP
 * =========================================================================*/

/** WIFI_SCAN: fill up to @p max scan entries; sets @p *out_count. */
m1_esp32_rpc_status_t m1_esp32_rpc_wifi_scan(m1_esp32_rpc_scan_entry_t *out,
                                             uint8_t max, uint8_t *out_count);

/** WIFI_DISCONNECT: tear down the current station association. */
m1_esp32_rpc_status_t m1_esp32_rpc_wifi_disconnect(void);

/** WIFI_SET_MAC-equivalent: override the station MAC (6 bytes). */
m1_esp32_rpc_status_t m1_esp32_rpc_wifi_set_mac(const uint8_t mac[6]);

/** SOFTAP_STOP: bring the SoftAP hotspot down. */
m1_esp32_rpc_status_t m1_esp32_rpc_softap_stop(void);

/* =========================================================================
 * Offensive WiFi
 * =========================================================================*/

/** OFF_MONITOR_START on @p channel (1-14). */
m1_esp32_rpc_status_t m1_esp32_rpc_monitor_start(uint8_t channel);
/** OFF_MONITOR_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_monitor_stop(void);

/** OFF_DEAUTH_START from a fully-populated request struct. */
m1_esp32_rpc_status_t m1_esp32_rpc_deauth_start(const m1_esp32_rpc_deauth_req_t *req);
/** OFF_DEAUTH_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_deauth_stop(void);

/** OFF_BEACON_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_beacon_stop(void);
/** OFF_PROBE_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_probe_stop(void);
/** OFF_KARMA_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_karma_stop(void);
/** OFF_CAPTIVE_STOP (evil portal). */
m1_esp32_rpc_status_t m1_esp32_rpc_captive_stop(void);

/** OFF_HS_START (handshake/PMKID capture) on @p channel. */
m1_esp32_rpc_status_t m1_esp32_rpc_handshake_start(uint8_t channel);
/** OFF_HS_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_handshake_stop(void);

/* =========================================================================
 * BLE
 * =========================================================================*/

/** BLE_INIT: bring the controller up. */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_init(void);
/** BLE_SCAN_START: begin passive scanning. */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_scan_start(void);
/** BLE_ADV_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_adv_stop(void);
/** BLE_SPAM_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_spam_stop(void);

/** BLE_HID_INIT advertising as @p name (truncated to 31 bytes on the wire). */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_hid_init(const char *name);
/** BLE_HID_KEY: press @p key_count HID usage ids with @p modifier. */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_hid_key(uint8_t modifier,
                                               const uint8_t *keys,
                                               uint8_t key_count);
/** BLE_HID_DEINIT. */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_hid_deinit(void);

/* =========================================================================
 * IEEE 802.15.4 (Zigbee / Thread)
 * =========================================================================*/

/** ZB_INIT: bring the 802.15.4 radio up. */
m1_esp32_rpc_status_t m1_esp32_rpc_zb_init(void);
/** ZB_SNIFF_START on @p channel (11-26). */
m1_esp32_rpc_status_t m1_esp32_rpc_zb_sniff_start(uint8_t channel);
/** ZB_SNIFF_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_zb_sniff_stop(void);
/** ZB_SNIFF_GET: fill up to @p max device records; sets @p *out_count. */
m1_esp32_rpc_status_t m1_esp32_rpc_zb_sniff_get(m1_esp32_rpc_zb_device_t *out,
                                                uint8_t max, uint8_t *out_count);
/** ZB_FLOOD_START on @p channel (11-26). */
m1_esp32_rpc_status_t m1_esp32_rpc_zb_flood_start(uint8_t channel);
/** ZB_FLOOD_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_zb_flood_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESP32_RPC_FEATURES_H_ */
