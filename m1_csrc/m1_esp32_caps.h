/* See COPYING.txt for license details. */

/*
 * m1_esp32_caps.h
 *
 * Runtime capability descriptor for the ESP32-C6 coprocessor.
 *
 * Multiple ESP32 firmware variants are supported (SiN360 binary-SPI,
 * bedge117 AT, neddy299 AT-deauth, and future variants).  Each exposes
 * a different feature set.  This module provides:
 *
 *   1. A capability bitmap queried at runtime via CMD_GET_STATUS.
 *   2. A backward-compatible fallback derived from compile-time flags
 *      when the connected firmware does not implement CMD_GET_STATUS.
 *   3. A standard "feature not supported" UI helper so call sites never
 *      need to hard-code firmware-specific strings.
 *
 * Rule: new ESP32-dependent features MUST gate on a capability bit
 * (m1_esp32_require_cap / m1_esp32_has_cap), NOT on a compile flag or
 * firmware name string.
 *
 * M1 Project
 */

#ifndef M1_ESP32_CAPS_H_
#define M1_ESP32_CAPS_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>   /* strncpy used in inline parse helper */

/* =========================================================================
 * Capability Bits (uint64_t bitmap — bits 0-63)
 *
 * Assignment is permanent: once a bit is published it NEVER changes meaning.
 * Unknown bits from a future firmware response are silently ignored (= 0).
 * =========================================================================*/

/* WiFi capabilities */
#define M1_ESP32_CAP_WIFI_SCAN        (UINT64_C(1) <<  0)  /* Basic AP scan */
#define M1_ESP32_CAP_WIFI_STA_SCAN    (UINT64_C(1) <<  1)  /* Client/station discovery */
#define M1_ESP32_CAP_WIFI_SNIFF       (UINT64_C(1) <<  2)  /* Packet monitor / sniffer */
#define M1_ESP32_CAP_WIFI_ATTACK      (UINT64_C(1) <<  3)  /* Deauth, beacon, karma, etc. */
#define M1_ESP32_CAP_WIFI_NETSCAN     (UINT64_C(1) <<  4)  /* Ping / ARP / SSH / port scan */
#define M1_ESP32_CAP_WIFI_EVIL_PORTAL (UINT64_C(1) <<  5)  /* Evil portal (subset of attack) */
#define M1_ESP32_CAP_WIFI_CONNECT     (UINT64_C(1) <<  6)  /* Connect, saved networks, NTP sync */

/* BLE / Bluetooth capabilities */
#define M1_ESP32_CAP_BLE_SCAN         (UINT64_C(1) <<  7)  /* BLE device scan */
#define M1_ESP32_CAP_BLE_ADV          (UINT64_C(1) <<  8)  /* BLE advertise */
#define M1_ESP32_CAP_BLE_SPAM         (UINT64_C(1) <<  9)  /* BLE beacon spam variants */
#define M1_ESP32_CAP_BLE_SNIFF        (UINT64_C(1) << 10)  /* BLE packet sniffers */
#define M1_ESP32_CAP_BLE_HID          (UINT64_C(1) << 11)  /* BLE HID keyboard (Bad-BT) */
#define M1_ESP32_CAP_BT_MANAGE        (UINT64_C(1) << 12)  /* BT device management (AT-layer) */

/* Other capabilities */
#define M1_ESP32_CAP_802154           (UINT64_C(1) << 13)  /* IEEE 802.15.4 / Zigbee / Thread */

/* Bits 14-63 reserved for future use */

/* =========================================================================
 * Composite profiles
 *
 * These capture the capability set of each known firmware variant.
 * Used as the CMD_GET_STATUS fallback when no handshake is possible.
 * =========================================================================*/

/** SiN360 binary-SPI firmware (sincere360/M1_SiN360_ESP32) */
#define M1_ESP32_CAP_PROFILE_SIN360 \
    (M1_ESP32_CAP_WIFI_SCAN        | \
     M1_ESP32_CAP_WIFI_STA_SCAN    | \
     M1_ESP32_CAP_WIFI_SNIFF       | \
     M1_ESP32_CAP_WIFI_ATTACK      | \
     M1_ESP32_CAP_WIFI_NETSCAN     | \
     M1_ESP32_CAP_WIFI_EVIL_PORTAL | \
     M1_ESP32_CAP_BLE_SCAN         | \
     M1_ESP32_CAP_BLE_ADV          | \
     M1_ESP32_CAP_BLE_SPAM         | \
     M1_ESP32_CAP_BLE_SNIFF)

/** bedge117 / C3.12 AT firmware (WiFi-connect + BLE-HID + 802.15.4) */
#define M1_ESP32_CAP_PROFILE_AT_C3 \
    (M1_ESP32_CAP_WIFI_SCAN    | \
     M1_ESP32_CAP_WIFI_CONNECT | \
     M1_ESP32_CAP_BLE_HID      | \
     M1_ESP32_CAP_BT_MANAGE    | \
     M1_ESP32_CAP_802154)

/** neddy299 deauth fork (C3 + deauth + station scan) */
#define M1_ESP32_CAP_PROFILE_AT_NEDDY299 \
    (M1_ESP32_CAP_PROFILE_AT_C3 | \
     M1_ESP32_CAP_WIFI_STA_SCAN | \
     M1_ESP32_CAP_WIFI_ATTACK)

/* =========================================================================
 * CMD_GET_STATUS payload structure (protocol version 1)
 *
 * The ESP32 firmware returns this in the 60-byte resp.payload[] when it
 * receives CMD_GET_STATUS (0x02).  Older firmware that does not implement
 * CMD_GET_STATUS will return RESP_ERR or a timeout, triggering the
 * compile-flag fallback.
 * =========================================================================*/

#define M1_ESP32_CAPS_PROTO_VER  1u

/**
 * Packed layout of the CMD_GET_STATUS response payload.
 * Total: 49 bytes — well within the 60-byte payload limit.
 *
 * Field notes:
 *   bss_bytes      — Size of the ESP32 firmware's BSS segment in bytes.
 *                    This is a compile-time constant (computed from linker
 *                    symbols, e.g. (&__bss_end - &__bss_start) * sizeof(int)).
 *                    Useful for comparing memory footprints across firmware
 *                    variants (larger BSS → less heap available for features).
 *                    Report 0 if the linker symbols are not accessible.
 *   free_heap_bytes — Runtime free heap on the ESP32 at the moment the
 *                    CMD_GET_STATUS response is assembled.  On ESP-IDF call
 *                    esp_get_free_heap_size(); on other RTOSes use the
 *                    equivalent.  Useful for diagnosing OOM-induced feature
 *                    failures.  Report 0 if unavailable.
 */
typedef struct __attribute__((packed)) {
    uint8_t  proto_ver;       /**< M1_ESP32_CAPS_PROTO_VER (1) */
    uint64_t cap_bitmap;      /**< Capability bits, little-endian */
    char     fw_name[32];     /**< Firmware identifier string, null-terminated */
    uint32_t bss_bytes;       /**< ESP32 BSS segment size in bytes (little-endian) */
    uint32_t free_heap_bytes; /**< ESP32 runtime free heap in bytes (little-endian) */
} m1_esp32_status_payload_t;

/* =========================================================================
 * Pure-logic parse helper (static inline — testable on host, no HAL deps)
 * =========================================================================*/

/**
 * Parse a raw CMD_GET_STATUS response payload into a bitmap and name string.
 *
 * @param payload          Raw payload bytes from m1_resp_t.payload[]
 * @param len              Length of valid payload bytes (resp.payload_len)
 * @param bitmap_out       Receives the parsed capability bitmap
 * @param fw_name_out      32-byte buffer receives null-terminated firmware name
 * @param bss_bytes_out    Receives the ESP32 BSS segment size in bytes
 * @param free_heap_out    Receives the ESP32 runtime free heap in bytes
 * @return true on success, false if payload is too short or protocol
 *         version is unrecognised
 */
static inline bool m1_esp32_caps_parse_payload(const uint8_t *payload,
                                                uint8_t        len,
                                                uint64_t      *bitmap_out,
                                                char           fw_name_out[32],
                                                uint32_t      *bss_bytes_out,
                                                uint32_t      *free_heap_out)
{
    if (len < (uint8_t)sizeof(m1_esp32_status_payload_t))
        return false;

    const m1_esp32_status_payload_t *p =
        (const m1_esp32_status_payload_t *)(const void *)payload;

    if (p->proto_ver != M1_ESP32_CAPS_PROTO_VER)
        return false;

    *bitmap_out    = p->cap_bitmap;
    *bss_bytes_out = p->bss_bytes;
    *free_heap_out = p->free_heap_bytes;
    strncpy(fw_name_out, p->fw_name, 31);
    fw_name_out[31] = '\0';
    return true;
}

/* =========================================================================
 * Public API
 * =========================================================================*/

/**
 * Query CMD_GET_STATUS and cache the capability descriptor.
 * Must be called after m1_esp32_init() + esp32_main_init().
 * Falls back to a compile-flag-derived bitmap if the ESP32 firmware does
 * not implement CMD_GET_STATUS.
 * Safe to call multiple times — subsequent calls are no-ops if already
 * queried successfully.
 */
void m1_esp32_caps_init(void);

/**
 * Reset the capability cache.
 * Called from m1_esp32_deinit() so the next init re-queries the firmware.
 */
void m1_esp32_caps_reset(void);

/**
 * Return true if the ESP32 firmware supports the requested capability.
 * @param cap  One of the M1_ESP32_CAP_* bit constants
 */
bool m1_esp32_has_cap(uint64_t cap);

/**
 * Return a null-terminated string describing the active ESP32 firmware.
 * Examples: "SiN360-0.9.6", "AT-bedge117-2.0.2", "Unknown (fallback)".
 * Never returns NULL.
 */
const char *m1_esp32_caps_fw_name(void);

/**
 * Check a capability and, if absent, draw a standard "Feature not
 * supported" screen (2 s) so the user knows why nothing happened.
 *
 * @param cap           Capability bit to check
 * @param feature_name  Short human-readable feature name (e.g. "Saved Networks")
 * @return true if capability is present (caller may proceed),
 *         false if capability is absent (screen has been shown, caller must abort)
 */
bool m1_esp32_require_cap(uint64_t cap, const char *feature_name);

/**
 * Return the ESP32 firmware's BSS segment size in bytes, as reported by
 * CMD_GET_STATUS.  Returns 0 if the firmware did not report a value (fallback
 * path) or if m1_esp32_caps_init() has not been called yet.
 */
uint32_t m1_esp32_caps_bss_bytes(void);

/**
 * Return the ESP32 runtime free heap in bytes at the time CMD_GET_STATUS
 * was last answered.  Returns 0 if the firmware did not report a value
 * (fallback path) or if m1_esp32_caps_init() has not been called yet.
 */
uint32_t m1_esp32_caps_free_heap(void);

#endif /* M1_ESP32_CAPS_H_ */
