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
 * Wire protocol note:
 *   CMD_GET_STATUS (0x02) on the binary SPI channel.  Only SiN360-style
 *   binary firmware responds; stock ESP-AT firmware does not understand
 *   binary opcodes and will time out, triggering the fallback path.
 *
 * M1 Project
 */

#ifndef M1_ESP32_CAPS_H_
#define M1_ESP32_CAPS_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>   /* strncpy used in inline parse helper */

/* =========================================================================
 * High-level capability bits (internal — used by m1_esp32_has_cap() callers)
 *
 * These are stable identifiers for features.  They are DERIVED from the
 * two wire-format bitmaps (at_cmd_bitmap + ext_bitmap) by the parse helper
 * below and are NEVER sent over the wire.
 *
 * Assignment is permanent once published.
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
 * Composite high-level profiles
 *
 * These express the capability set of each known firmware variant in terms
 * of M1_ESP32_CAP_* bits.  Used as the CMD_GET_STATUS fallback when no
 * binary-SPI handshake is possible (e.g. older SiN360 firmware that predates
 * CMD_GET_STATUS support, or a non-binary firmware path).
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
 * Wire-format: standard AT command bits  (CMD_GET_STATUS at_cmd_bitmap)
 *
 * Carried in m1_esp32_status_payload_t.at_cmd_bitmap (uint64_t, LE).
 * Each bit indicates that the corresponding ESP-AT command is available.
 * Mirrored from the ESP32 firmware's m1_protocol.h.
 * =========================================================================*/

#define M1_AT_CMD_AT              (UINT64_C(1) <<  0)
#define M1_AT_CMD_GMR             (UINT64_C(1) <<  1)
#define M1_AT_CMD_CWMODE          (UINT64_C(1) <<  2)
#define M1_AT_CMD_CWLAP           (UINT64_C(1) <<  3)  /* WiFi AP scan */
#define M1_AT_CMD_CWJAP           (UINT64_C(1) <<  4)  /* WiFi connect */
#define M1_AT_CMD_CWQAP           (UINT64_C(1) <<  5)  /* WiFi disconnect */
#define M1_AT_CMD_CIPSTAMAC       (UINT64_C(1) <<  6)
#define M1_AT_CMD_CWSTARTSMART    (UINT64_C(1) <<  7)
#define M1_AT_CMD_CWSTOPSMART     (UINT64_C(1) <<  8)
#define M1_AT_CMD_BLEINIT         (UINT64_C(1) <<  9)
#define M1_AT_CMD_BLESCANPARAM    (UINT64_C(1) << 10)
#define M1_AT_CMD_BLESCAN         (UINT64_C(1) << 11)  /* BLE scan */
#define M1_AT_CMD_BLEGAPADV       (UINT64_C(1) << 12)  /* BLE advertise */
#define M1_AT_CMD_BLEADVSTART     (UINT64_C(1) << 13)
#define M1_AT_CMD_BLEADVSTOP      (UINT64_C(1) << 14)
#define M1_AT_CMD_BLEGATTCPRIMSRV (UINT64_C(1) << 15)
#define M1_AT_CMD_BLEGATTCCHAR    (UINT64_C(1) << 16)
#define M1_AT_CMD_BLEGATTCWR      (UINT64_C(1) << 17)
#define M1_AT_CMD_BLEGATTCNTFY    (UINT64_C(1) << 18)
/* Bits 19-63 reserved */

/* =========================================================================
 * Wire-format: binary-SPI extension bits  (CMD_GET_STATUS ext_bitmap)
 *
 * Carried in m1_esp32_status_payload_t.ext_bitmap (uint32_t, LE).
 * Each bit indicates a binary-SPI-only extension with no stock AT analogue.
 * Mirrored from the ESP32 firmware's m1_protocol.h.
 * =========================================================================*/

#define M1_EXT_CMD_STA_SCAN       (1u <<  0)  /* WiFi station discovery */
#define M1_EXT_CMD_WIFI_SNIFF     (1u <<  1)  /* Packet monitor / sniffer */
#define M1_EXT_CMD_WIFI_ATTACK    (1u <<  2)  /* Deauth, beacon, karma, etc. */
#define M1_EXT_CMD_WIFI_NETSCAN   (1u <<  3)  /* Ping / ARP / SSH / port scan */
#define M1_EXT_CMD_WIFI_PORTAL    (1u <<  4)  /* Evil portal */
#define M1_EXT_CMD_BLE_SPAM       (1u <<  5)  /* BLE beacon spam */
#define M1_EXT_CMD_BLE_SNIFF      (1u <<  6)  /* BLE sniffers */
#define M1_EXT_CMD_BLE_HID        (1u <<  7)  /* BLE HID keyboard */
#define M1_EXT_CMD_BT_MANAGE      (1u <<  8)  /* BT device management */
#define M1_EXT_CMD_802154         (1u <<  9)  /* IEEE 802.15.4 */
/* Bits 10-31 reserved */

/* =========================================================================
 * Wire-format profile macros
 *
 * Express the expected at_cmd_bitmap / ext_bitmap values for each known
 * firmware variant.  Useful for testing and for the fallback derive path.
 * =========================================================================*/

/** SiN360 binary-SPI at_cmd_bitmap — standard AT commands it supports */
#define M1_AT_CMD_PROFILE_SIN360 \
    (M1_AT_CMD_AT              | \
     M1_AT_CMD_GMR             | \
     M1_AT_CMD_CWMODE          | \
     M1_AT_CMD_CWLAP           | \
     M1_AT_CMD_CIPSTAMAC       | \
     M1_AT_CMD_BLEINIT         | \
     M1_AT_CMD_BLESCANPARAM    | \
     M1_AT_CMD_BLESCAN         | \
     M1_AT_CMD_BLEGAPADV       | \
     M1_AT_CMD_BLEADVSTART     | \
     M1_AT_CMD_BLEADVSTOP      | \
     M1_AT_CMD_BLEGATTCPRIMSRV | \
     M1_AT_CMD_BLEGATTCCHAR    | \
     M1_AT_CMD_BLEGATTCWR      | \
     M1_AT_CMD_BLEGATTCNTFY)

/** SiN360 binary-SPI ext_bitmap — binary-only extensions */
#define M1_EXT_CMD_PROFILE_SIN360 \
    (M1_EXT_CMD_STA_SCAN    | \
     M1_EXT_CMD_WIFI_SNIFF  | \
     M1_EXT_CMD_WIFI_ATTACK | \
     M1_EXT_CMD_WIFI_NETSCAN| \
     M1_EXT_CMD_WIFI_PORTAL | \
     M1_EXT_CMD_BLE_SPAM    | \
     M1_EXT_CMD_BLE_SNIFF)

/**
 * Default at_cmd_bitmap assumed when CMD_GET_STATUS receives no response.
 * Used internally for the fallback → derive path.  Represents the minimum
 * AT command set of any stock bedge117-compatible firmware.
 */
#define M1_AT_CMD_PROFILE_DEFAULT \
    (M1_AT_CMD_AT           | \
     M1_AT_CMD_GMR          | \
     M1_AT_CMD_CWMODE       | \
     M1_AT_CMD_CWLAP        | \
     M1_AT_CMD_CWJAP        | \
     M1_AT_CMD_CWQAP        | \
     M1_AT_CMD_CIPSTAMAC    | \
     M1_AT_CMD_BLEINIT      | \
     M1_AT_CMD_BLESCANPARAM | \
     M1_AT_CMD_BLESCAN      | \
     M1_AT_CMD_BLEGAPADV    | \
     M1_AT_CMD_BLEADVSTART  | \
     M1_AT_CMD_BLEADVSTOP)

/* =========================================================================
 * CMD_GET_STATUS payload structure (protocol version 1)
 *
 * The ESP32 firmware returns this in the 60-byte resp.payload[] when it
 * receives CMD_GET_STATUS (0x02).  Older firmware that does not implement
 * CMD_GET_STATUS will return RESP_ERR or a timeout, triggering the
 * compile-flag fallback.
 *
 * Only SiN360-style binary-SPI firmware responds to CMD_GET_STATUS on the
 * binary SPI channel.  Stock ESP-AT firmware does not understand binary
 * opcodes; it will time out, and the host uses M1_ESP32_CAP_PROFILE_SIN360
 * as the fallback.
 * =========================================================================*/

#define M1_ESP32_CAPS_PROTO_VER  1u

/**
 * Packed layout of the CMD_GET_STATUS response payload.
 * Total: 45 bytes — well within the 60-byte payload limit.
 *
 * Field order matches the ESP32 firmware's m1_protocol.h exactly.
 *   at_cmd_bitmap — uint64_t, LE — M1_AT_CMD_* bits for standard AT commands.
 *   ext_bitmap    — uint32_t, LE — M1_EXT_CMD_* bits for binary-only extensions.
 *   fw_name       — null-terminated ASCII firmware identifier.
 */
typedef struct __attribute__((packed)) {
    uint8_t  proto_ver;       /**< M1_ESP32_CAPS_PROTO_VER (1) */
    uint64_t at_cmd_bitmap;   /**< Standard AT command bits, little-endian */
    uint32_t ext_bitmap;      /**< Binary-SPI extension bits, little-endian */
    char     fw_name[32];     /**< Firmware identifier string, null-terminated */
} m1_esp32_status_payload_t;

/* =========================================================================
 * Pure-logic derive helper (static inline — testable on host, no HAL deps)
 *
 * Maps the two wire-format bitmaps to the internal M1_ESP32_CAP_* bitmap.
 * =========================================================================*/

/**
 * Derive internal M1_ESP32_CAP_* capability bits from a CMD_GET_STATUS
 * response's two wire-format bitmaps.
 *
 * @param at_cmd  at_cmd_bitmap from the parsed payload
 * @param ext     ext_bitmap from the parsed payload
 * @return        uint64_t bitmask of M1_ESP32_CAP_* bits
 */
static inline uint64_t m1_esp32_caps_derive(uint64_t at_cmd, uint32_t ext)
{
    uint64_t caps = 0u;

    /* WiFi — AT command layer */
    if (at_cmd & M1_AT_CMD_CWLAP)      caps |= M1_ESP32_CAP_WIFI_SCAN;
    if (at_cmd & M1_AT_CMD_CWJAP)      caps |= M1_ESP32_CAP_WIFI_CONNECT;

    /* WiFi — binary extensions */
    if (ext & M1_EXT_CMD_STA_SCAN)     caps |= M1_ESP32_CAP_WIFI_STA_SCAN;
    if (ext & M1_EXT_CMD_WIFI_SNIFF)   caps |= M1_ESP32_CAP_WIFI_SNIFF;
    if (ext & M1_EXT_CMD_WIFI_ATTACK)  caps |= M1_ESP32_CAP_WIFI_ATTACK;
    if (ext & M1_EXT_CMD_WIFI_NETSCAN) caps |= M1_ESP32_CAP_WIFI_NETSCAN;
    if (ext & M1_EXT_CMD_WIFI_PORTAL)  caps |= M1_ESP32_CAP_WIFI_EVIL_PORTAL;

    /* BLE — AT command layer */
    if (at_cmd & M1_AT_CMD_BLESCAN)    caps |= M1_ESP32_CAP_BLE_SCAN;
    if (at_cmd & M1_AT_CMD_BLEGAPADV)  caps |= M1_ESP32_CAP_BLE_ADV;

    /* BLE / BT — binary extensions */
    if (ext & M1_EXT_CMD_BLE_SPAM)     caps |= M1_ESP32_CAP_BLE_SPAM;
    if (ext & M1_EXT_CMD_BLE_SNIFF)    caps |= M1_ESP32_CAP_BLE_SNIFF;
    if (ext & M1_EXT_CMD_BLE_HID)      caps |= M1_ESP32_CAP_BLE_HID;
    if (ext & M1_EXT_CMD_BT_MANAGE)    caps |= M1_ESP32_CAP_BT_MANAGE;

    /* IEEE 802.15.4 */
    if (ext & M1_EXT_CMD_802154)        caps |= M1_ESP32_CAP_802154;

    return caps;
}

/* =========================================================================
 * Pure-logic parse helper (static inline — testable on host, no HAL deps)
 * =========================================================================*/

/**
 * Parse a raw CMD_GET_STATUS response payload and derive the internal
 * capability bitmap.
 *
 * @param payload      Raw payload bytes from m1_resp_t.payload[]
 * @param len          Length of valid payload bytes (resp.payload_len)
 * @param caps_out     Receives the derived M1_ESP32_CAP_* capability bitmap
 * @param fw_name_out  32-byte buffer receives null-terminated firmware name
 * @return true on success, false if payload is too short or protocol
 *         version is unrecognised
 */
static inline bool m1_esp32_caps_parse_payload(const uint8_t *payload,
                                                uint8_t        len,
                                                uint64_t      *caps_out,
                                                char           fw_name_out[32])
{
    if (len < (uint8_t)sizeof(m1_esp32_status_payload_t))
        return false;

    const m1_esp32_status_payload_t *p =
        (const m1_esp32_status_payload_t *)(const void *)payload;

    if (p->proto_ver != M1_ESP32_CAPS_PROTO_VER)
        return false;

    *caps_out = m1_esp32_caps_derive(p->at_cmd_bitmap, p->ext_bitmap);
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


#endif /* M1_ESP32_CAPS_H_ */
