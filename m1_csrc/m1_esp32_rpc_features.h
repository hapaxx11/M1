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

#include "FreeRTOS.h"           /* TickType_t, TaskHandle_t, SemaphoreHandle_t */
#include "m1_esp32_rpc.h"       /* client + opcode enum + payload structs */
#include "esp32_feature_map.h"  /* esp32_feature_id_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Default per-call response timeout (seconds) shared by the wrappers below.
 * Matches the ESP-NOW / CD3 detection probe timeout — generous for the prompt
 * single-transaction control commands these features issue. */
#define M1_ESP32_RPC_FEATURE_TIMEOUT_S  2

/* WIFI_SCAN-specific response timeout (seconds) — issue #719 Phase 5.
 *
 * Unlike every other scan-style feature (STA_SCAN, BLE_SCAN), which use a
 * START trigger followed by a separate, quick RESULTS poll of an
 * already-buffered list, the brain's handle_wifi_scan() runs the *entire*
 * channel sweep synchronously inside the single WIFI_SCAN request/response
 * transaction (see m1_esp32_rpc_wifi_scan()'s "one logical response" comment
 * below) and only replies once the scan completes. A real active scan across
 * all 2.4 GHz channels can legitimately take several seconds — far longer
 * than M1_ESP32_RPC_FEATURE_TIMEOUT_S's 2 s budget for prompt commands.
 *
 * Field read-back on the Settings > Dashboard > page 5/5 "Last feature RPC"
 * line confirmed the failure mode this predicted: "op0103 no-reply st253 r0
 * p0" (M1_ESP32_RPC_ERR_TRANSPORT) — the M1 Link transport's poll budget,
 * scaled from the caller's timeout_sec (see spi_m1link_send_recv_bin()),
 * expired before the brain queued its reply. Widening the budget passed for
 * this call (rather than the shared constant, which stays 2 s for every
 * other prompt command) lets a real scan finish before the transport gives
 * up.
 *
 * Update (issue #719 Phase 7): a later field read-back of the same line
 * showed "op0103 no-reply st253 r0 p0 t0s" — the transport was giving up in
 * well under a second despite this 10 s budget, because
 * spi_m1link_send_recv_bin() converted timeout_sec into a fixed poll COUNT
 * that assumed a fixed per-poll cost, not real elapsed time (see the
 * "Poll-budget wall-clock FIX" comment in m1_esp32_rpc.h). With that
 * transport bug fixed, this 10 s value now takes effect as originally
 * intended; a further field read-back after the fix confirmed a real scan
 * completing well within the budget: "op0103 ok st0 r456 p446". */
#define M1_ESP32_RPC_WIFI_SCAN_TIMEOUT_S  10

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

/** One decoded WIFI_SCAN result entry returned to host callers. */
typedef struct {
    uint8_t bssid[6];
    int8_t  rssi;
    uint8_t channel;
    uint8_t authmode;
    char    ssid[33];
} m1_esp32_rpc_wifi_scan_result_t;

/** WIFI_SCAN: fill up to @p max decoded entries; sets @p *out_count. */
m1_esp32_rpc_status_t m1_esp32_rpc_wifi_scan(m1_esp32_rpc_wifi_scan_result_t *out,
                                             uint8_t max, uint8_t *out_count);

/* -------------------------------------------------------------------------
 * Async WIFI_SCAN: non-blocking variant for use on the FreeRTOS target.
 *
 * The synchronous m1_esp32_rpc_wifi_scan() blocks the caller for up to
 * M1_ESP32_RPC_WIFI_SCAN_TIMEOUT_S (10 s) while the brain firmware sweeps
 * all channels.  The async API offloads that work to a short-lived worker
 * task so the calling task can remain responsive (check buttons, refresh the
 * UI) while the scan is in flight.
 *
 * Usage:
 *   m1_esp32_rpc_wifi_scan_async_t ctx;
 *   m1_esp32_rpc_wifi_scan_async_start(&ctx, buf, max);
 *   while (!m1_esp32_rpc_wifi_scan_async_poll(&ctx))
 *       vTaskDelay(pdMS_TO_TICKS(50));          // yield; check buttons here
 *   st = ctx.status; count = ctx.count;
 *   m1_esp32_rpc_wifi_scan_async_cleanup(&ctx);
 * -------------------------------------------------------------------------*/

/** State block for an in-flight async WiFi scan.  Stack- or heap-allocate
 *  one per scan call; do not share between tasks.  All fields are written by
 *  the worker task and read by the caller after completion. */
typedef struct {
    m1_esp32_rpc_wifi_scan_result_t *out;    /**< Result buffer (caller-owned). */
    uint8_t                          max;    /**< Buffer capacity in entries. */
    uint8_t                          count;  /**< Entries written by worker. */
    m1_esp32_rpc_status_t            status; /**< Worker exit status. */
    volatile bool                    cancel; /**< Caller sets true to abort. */
    SemaphoreHandle_t                done;   /**< Binary semaphore: given on exit. */
    TaskHandle_t                     task;   /**< Worker handle (NULL after done). */
} m1_esp32_rpc_wifi_scan_async_t;

/**
 * Start an async WiFi scan.
 *
 * Allocates a binary semaphore, spawns a worker task that calls
 * m1_esp32_rpc_wifi_scan(), and returns immediately.  The caller must later
 * call m1_esp32_rpc_wifi_scan_async_poll() to detect completion and
 * m1_esp32_rpc_wifi_scan_async_cleanup() to release resources.
 *
 * @param ctx  Uninitialised state block; must remain valid until cleanup.
 * @param out  Caller-allocated result buffer; must remain valid until cleanup.
 * @param max  Capacity of @p out in entries.
 * @return true on success (worker launched), false if semaphore or task
 *         creation failed.
 */
bool m1_esp32_rpc_wifi_scan_async_start(m1_esp32_rpc_wifi_scan_async_t *ctx,
                                        m1_esp32_rpc_wifi_scan_result_t *out,
                                        uint8_t max);

/**
 * Non-blocking completion check.
 *
 * @return true when the worker has finished (or been cancelled) and results
 *         are available in @p ctx->out / @p ctx->count / @p ctx->status.
 *         false while the scan is still in flight.
 */
bool m1_esp32_rpc_wifi_scan_async_poll(m1_esp32_rpc_wifi_scan_async_t *ctx);

/**
 * Request cancellation of an in-flight scan.
 *
 * Sets a flag the worker task checks before issuing the RPC call.  Does not
 * block; callers must still poll for completion and call cleanup.
 */
void m1_esp32_rpc_wifi_scan_async_cancel(m1_esp32_rpc_wifi_scan_async_t *ctx);

/**
 * Release resources allocated by m1_esp32_rpc_wifi_scan_async_start().
 *
 * Must be called exactly once after m1_esp32_rpc_wifi_scan_async_poll()
 * returns true (or after cancellation + poll confirms completion).
 */
void m1_esp32_rpc_wifi_scan_async_cleanup(m1_esp32_rpc_wifi_scan_async_t *ctx);

/**
 * WIFI_CONNECT: join an AP with @p ssid and @p password.
 *
 * Wire format: [ssid_len:1][ssid][pwd_len:1][pwd].
 * On success the RESP carries [status:1][ipv4:4 LE].
 * @p out_ip (optional) receives the assigned IPv4 address in host byte order.
 */
m1_esp32_rpc_status_t m1_esp32_rpc_wifi_connect(const char *ssid,
                                                const char *password,
                                                uint32_t *out_ip);

/** WIFI_DISCONNECT: tear down the current station association. */
m1_esp32_rpc_status_t m1_esp32_rpc_wifi_disconnect(void);

/** WIFI_SET_MAC-equivalent: override the station MAC (6 bytes). */
m1_esp32_rpc_status_t m1_esp32_rpc_wifi_set_mac(const uint8_t mac[6]);

/**
 * SOFTAP_START: bring up a WiFi hotspot on @p channel broadcasting @p ssid
 * (protected by @p password, may be empty for open AP).
 *
 * Wire format: [channel:1][ssid\0][pass\0].
 */
m1_esp32_rpc_status_t m1_esp32_rpc_softap_start(const char *ssid,
                                                const char *password,
                                                uint8_t channel);

/** SOFTAP_STOP: bring the SoftAP hotspot down. */
m1_esp32_rpc_status_t m1_esp32_rpc_softap_stop(void);

/* =========================================================================
 * Offensive WiFi
 * =========================================================================*/

/** OFF_MONITOR_START on @p channel (1-14; 0 = channel-hop 1-13). */
m1_esp32_rpc_status_t m1_esp32_rpc_monitor_start(uint8_t channel);
/** OFF_MONITOR_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_monitor_stop(void);

/**
 * OFF_MONITOR_READ: poll one buffered monitor frame from the ESP32.
 *
 * Wire format (when a frame is available):
 *   [channel:1][rssi:i8][frame_len:2 LE][frame bytes]
 * If no frame is buffered the response has payload_len == 0.
 *
 * @param out_frame    [out] receives the raw 802.11 frame bytes (may be NULL).
 * @param frame_max    Capacity of @p out_frame in bytes.
 * @param out_len      [out] number of frame bytes copied (may be NULL).
 * @param out_channel  [out] channel the frame was captured on (may be NULL).
 * @param out_rssi     [out] RSSI in dBm (may be NULL).
 * @return M1_ESP32_RPC_OK on success, including a zero-length response (no
 *         packet buffered).  Other codes report transport / framing errors.
 */
m1_esp32_rpc_status_t m1_esp32_rpc_monitor_read(uint8_t *out_frame,
                                                uint16_t frame_max,
                                                uint16_t *out_len,
                                                uint8_t *out_channel,
                                                int8_t  *out_rssi);

/** OFF_DEAUTH_START from a fully-populated request struct. */
m1_esp32_rpc_status_t m1_esp32_rpc_deauth_start(const m1_esp32_rpc_deauth_req_t *req);
/** OFF_DEAUTH_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_deauth_stop(void);

/**
 * OFF_BEACON_START: broadcast beacon frames for each SSID in @p ssids.
 *
 * Wire format: [count:1] then per SSID [len:1][ssid].
 * @p count must be 1..32; @p ssids must not be NULL.
 */
m1_esp32_rpc_status_t m1_esp32_rpc_beacon_start(const char (*ssids)[33],
                                                uint8_t count);
/** OFF_BEACON_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_beacon_stop(void);

/**
 * OFF_PROBE_START: flood probe-request frames on @p channel.
 *
 * Wire format: [channel:1][count:1] then per SSID [len:1][ssid].
 * @p count must be 1..32; @p ssids must not be NULL.
 */
m1_esp32_rpc_status_t m1_esp32_rpc_probe_start(uint8_t channel,
                                               const char (*ssids)[33],
                                               uint8_t count);
/** OFF_PROBE_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_probe_stop(void);

/**
 * OFF_KARMA_START: auto-respond to every probe request on @p channel.
 *
 * Wire format: [channel:1].
 */
m1_esp32_rpc_status_t m1_esp32_rpc_karma_start(uint8_t channel);
/** OFF_KARMA_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_karma_stop(void);

/**
 * OFF_CAPTIVE_START: launch a rogue-AP captive portal on @p channel.
 *
 * Wire format: [channel:1][ssid_len:1][ssid][title...].
 * @p title is optional; pass NULL or "" to use the firmware default.
 */
m1_esp32_rpc_status_t m1_esp32_rpc_captive_start(uint8_t channel,
                                                 const char *ssid,
                                                 const char *title);
/** OFF_CAPTIVE_STOP (evil portal). */
m1_esp32_rpc_status_t m1_esp32_rpc_captive_stop(void);

/**
 * OFF_STA_SCAN_START: scan for associated stations on a target AP.
 *
 * Wire format: m1_esp32_rpc_sta_scan_req_t ([bssid:6][channel:1][dur_s:1]).
 */
m1_esp32_rpc_status_t m1_esp32_rpc_sta_scan_start(const uint8_t bssid[6],
                                                  uint8_t channel,
                                                  uint8_t dur_s);

/**
 * OFF_STA_SCAN_RESULTS: retrieve discovered station list.
 *
 * Fills up to @p max entries and sets @p *out_count.
 * RESP wire format: [count:2 LE] then per sta [mac:6][rssi:i8].
 */
m1_esp32_rpc_status_t m1_esp32_rpc_sta_scan_results(m1_esp32_rpc_sta_entry_t *out,
                                                    uint8_t max,
                                                    uint8_t *out_count);

/**
 * OFF_HS_START: start EAPOL handshake capture on @p channel.
 *
 * Wire format: m1_esp32_rpc_hs_start_req_t ([bssid:6][channel:1][deauth_count:2 LE]).
 * @p deauth_count injected deauth frames to force client reconnection (0 = passive).
 */
m1_esp32_rpc_status_t m1_esp32_rpc_hs_start(const uint8_t bssid[6],
                                            uint8_t channel,
                                            uint16_t deauth_count);
/** OFF_HS_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_handshake_stop(void);

/* =========================================================================
 * BLE
 * =========================================================================*/

/** BLE_INIT: bring the controller up. */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_init(void);

/**
 * BLE_SCAN_START: begin passive scanning for @p dur_s seconds.
 *
 * Wire format: [dur_lo:1][dur_hi:1] (u16 LE seconds; 0 = firmware default 3 s).
 */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_scan_start(uint16_t dur_s);

/**
 * BLE_SCAN_RESULTS: retrieve discovered BLE device list.
 *
 * Fills up to @p max entries and sets @p *out_count.
 * RESP wire format: [count:2 LE] then per dev [addr:6][type:1][rssi:i8][name_len:1][name].
 */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_scan_results(m1_esp32_rpc_ble_dev_t *out,
                                                    uint8_t max,
                                                    uint8_t *out_count);

/**
 * BLE_ADV_START: start generic BLE advertising as @p name.
 *
 * Wire format: raw device name bytes (no length prefix).
 */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_adv_start(const char *name);
/** BLE_ADV_STOP. */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_adv_stop(void);

/**
 * BLE_SPAM_START: start proximity-pair popup ad-spam.
 *
 * Wire format: [mode:1].  Mode values (mirrors BLE_SPAM_MODE_* on firmware side):
 *   0x01 = Apple  0x02 = Samsung  0x03 = Windows  0xFF = all (round-robin).
 */
m1_esp32_rpc_status_t m1_esp32_rpc_ble_spam_start(uint8_t mode);
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
/** BLE_HID_STATUS: returns true if a host is connected (bit1 of status byte). */
bool m1_esp32_rpc_ble_hid_is_connected(void);

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

/* =========================================================================
 * System
 * =========================================================================*/

/**
 * Decoded UTC time from a SYS_SNTP_SYNC response.
 * Field layout mirrors m1_time_t and clock_time_t so the caller can copy
 * directly to either without field-by-field assignment.
 */
typedef struct {
    uint16_t year;
    uint8_t  month;   /**< 1–12 */
    uint8_t  day;     /**< 1–31 */
    uint8_t  hour;    /**< 0–23 */
    uint8_t  minute;  /**< 0–59 */
    uint8_t  second;  /**< 0–59 */
    uint8_t  weekday; /**< 0=Sun … 6=Sat */
} m1_esp32_rpc_utctime_t;

/**
 * SYS_SNTP_SYNC: request the brain firmware to fetch the current time from
 * the NTP pool and return it.
 *
 * The brain issues a pool.ntp.org query and replies with the UTC time once
 * synchronized (or returns an error if no IP connectivity is available).
 * The call uses M1_ESP32_RPC_FEATURE_TIMEOUT_S (2 s) because the firmware
 * is already associated when this is called (after a successful WIFI_CONNECT).
 *
 * @param out  [out] receives the decoded UTC time on success; may be NULL to
 *             merely trigger synchronization without reading back the time.
 * @return M1_ESP32_RPC_OK on success, or an error code on failure.
 */
m1_esp32_rpc_status_t m1_esp32_rpc_sntp_sync(m1_esp32_rpc_utctime_t *out);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESP32_RPC_FEATURES_H_ */
