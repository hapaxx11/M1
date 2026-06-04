/* See COPYING.txt for license details. */

/**
 * @file   esp32_feature_map.h
 * @brief  ESP32-gated feature classifier — pure logic, no HAL/RTOS deps.
 *
 * Phase C of the firmware-wide momentum-parity migration.
 *
 * Centralises the mapping of every ESP32-dependent firmware feature to the
 * M1_ESP32_CAP_* capability bits it requires, together with the UI label
 * shown in the "Feature not supported" screen.
 *
 * Previously the cap-bit and label were duplicated inline at each
 * DELEGATE_CAPPED call site.  This module extracts that table so:
 *   - A single enum (esp32_feature_id_t) names every gated feature.
 *   - esp32_feature_required_caps() / esp32_feature_label() expose the
 *     mapping for host tests and scene-level helpers.
 *   - esp32_feature_supported() runs the bitmap query without side effects.
 *   - esp32_firmware_is_sin360() encodes the "use binary-SPI path" decision
 *     in one place (currently duplicated in m1_badbt.c).
 *
 * Design rules
 * ------------
 *  - Zero HAL / RTOS / display / FatFS dependencies.
 *  - All functions are pure given their inputs; no global state.
 *  - The table is the authoritative source.  Scene files must not
 *    hard-code raw M1_ESP32_CAP_* bits + label strings at call sites.
 *  - When a new gated feature is added, add one enum value here and one
 *    row to the table in esp32_feature_map.c.  Do NOT add new
 *    DELEGATE_CAPPED(... M1_ESP32_CAP_xxx, "label") sites.
 *
 * Mirroring subghz_button_caps.c (Phase 4a) — same pattern, applied to the
 * ESP32 capability landscape.
 *
 * M1 Project
 */

#ifndef ESP32_FEATURE_MAP_H_
#define ESP32_FEATURE_MAP_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Feature identifier enum
 *
 * Each value identifies one ESP32-gated firmware feature.  The ordering
 * mirrors the M1_ESP32_CAP_* bit assignment in m1_esp32_caps.h for
 * readability, but the numeric values are independent and must not be used
 * as array indices in the caller.  Always pass by enum value.
 * =========================================================================*/
typedef enum {
    /* ----- WiFi scan & monitoring ---------------------------------------- */
    /** AP discovery scan (CMD_WIFI_SCAN_START / NEXT / STOP) */
    ESP32_FEATURE_WIFI_SCAN = 0,

    /** Promiscuous station scan (CMD_STA_SCAN_*) */
    ESP32_FEATURE_STA_SCAN,

    /** Packet sniffer / monitor mode */
    ESP32_FEATURE_PKTMON,

    /* ----- WiFi attacks --------------------------------------------------- */
    /** Deauthentication attack */
    ESP32_FEATURE_DEAUTH,

    /** Beacon flooding / clone / rickroll */
    ESP32_FEATURE_BEACON,

    /** Probe request flooding */
    ESP32_FEATURE_PROBE_FLOOD,

    /** Karma AP impersonation */
    ESP32_FEATURE_KARMA,

    /** Evil captive portal (Karma+Portal combined) */
    ESP32_FEATURE_PORTAL,

    /* ----- WiFi connect — AT-layer stubs (bedge117/neddy299) ------------- */
    /** Station join / disconnect / status.  Absent from SiN360. */
    ESP32_FEATURE_WIFI_JOIN,

    /** WiFi MAC address spoofing */
    ESP32_FEATURE_WIFI_SET_MAC,

    /** WiFi channel override */
    ESP32_FEATURE_WIFI_SET_CHAN,

    /* ----- Network scanning ----------------------------------------------- */
    /** Ping / ARP / SSH / Telnet / port scan */
    ESP32_FEATURE_NETSCAN,

    /* ----- IEEE 802.15.4 -------------------------------------------------- */
    /** Zigbee / Thread passive scan */
    ESP32_FEATURE_802154,

    /* ----- BLE ------------------------------------------------------------- */
    /** BLE passive scan */
    ESP32_FEATURE_BLE_SCAN,

    /** BLE advertisement (spam / spoof) */
    ESP32_FEATURE_BLE_ADV,

    /** BLE HID keyboard injection (Bad-BT) */
    ESP32_FEATURE_BLE_HID,

    /** BLE GATT client discovery */
    ESP32_FEATURE_BLE_GATT,

    /* ----- Bluetooth management ------------------------------------------- */
    /** Saved BT devices list / BT info */
    ESP32_FEATURE_BT_MANAGE,

    /* ----- Sentinel (must be last) ---------------------------------------- */
    ESP32_FEATURE_COUNT,
} esp32_feature_id_t;

/* =========================================================================
 * Pure-logic query API
 *
 * All functions accept a raw capability bitmap (uint64_t) and/or a
 * feature ID.  There are no side effects, no HAL calls, no global state.
 * Safe to call from host tests without stubs.
 * =========================================================================*/

/**
 * @brief  Return true if @p cap_bitmap satisfies all capability bits
 *         required by @p fid.
 *
 * Returns false if @p fid is out of range.  Does not call any HAL
 * function — pass the bitmap from m1_esp32_has_cap() / the test fixture.
 */
bool esp32_feature_supported(uint64_t cap_bitmap, esp32_feature_id_t fid);

/**
 * @brief  Return the OR'd M1_ESP32_CAP_* bits required by @p fid.
 *
 * Returns 0 if @p fid is out of range.  The returned value is suitable
 * for passing directly to m1_esp32_require_cap().
 */
uint64_t esp32_feature_required_caps(esp32_feature_id_t fid);

/**
 * @brief  Return the human-readable UI label for @p fid.
 *
 * The label is the short feature name displayed in the "Feature not
 * supported" screen (e.g. "Bad-BT", "Saved Devices").  Never returns
 * NULL — unknown IDs return the empty string "".
 */
const char *esp32_feature_label(esp32_feature_id_t fid);

/* =========================================================================
 * Firmware-class classifier
 *
 * Derives the firmware transport class (SiN360 binary-SPI vs. AT text)
 * from the capability bitmap.  Pure logic — no global state, no HAL calls.
 * =========================================================================*/

/**
 * @brief  Return true when the bitmap indicates a SiN360 binary-SPI firmware.
 *
 * Discriminator: BLE_HID present (SiN360-only bit) AND WIFI_JOIN absent
 * (AT-firmware-only bit).  This is the same rule used in m1_badbt.c to
 * select the CMD_BLE_HID_START path vs. the AT+BLEHIDINIT path.
 *
 * Returns false for all-zero bitmaps (fallback / unknown firmware) — safe
 * to call before m1_esp32_caps_init() completes.
 */
bool esp32_firmware_is_sin360(uint64_t cap_bitmap);

#ifdef __cplusplus
}
#endif

#endif /* ESP32_FEATURE_MAP_H_ */
