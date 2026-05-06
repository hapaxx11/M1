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
 *   CMD_GET_STATUS (0x02) on the binary SPI channel.  The response carries
 *   M1_ESP32_CAP_* bits directly in a single cap_bitmap field — no
 *   derivation step is needed on the STM32 side.  Any firmware variant that
 *   implements CMD_GET_STATUS (SiN360 binary-SPI or AT firmware with the
 *   feat/cmd_get_status extension) can respond; firmware that does not
 *   implement CMD_GET_STATUS will time out, triggering the fallback path.
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
 * Compile-time fallback profiles
 *
 * Used as the CMD_GET_STATUS fallback when the connected firmware does not
 * implement CMD_GET_STATUS.  SiN360 binary-SPI firmware has supported
 * CMD_GET_STATUS since its initial integration in Hapax and always
 * self-reports; it therefore never triggers the fallback path.  AT firmware
 * variants (bedge117, neddy299, dag) that predate the feat/cmd_get_status
 * extension trigger the fallback — M1_ESP32_CAP_PROFILE_AT_BEDGE117 is
 * used as the default.  Firmware that implements CMD_GET_STATUS will always
 * take the self-reporting success path.
 *
 * Profile caps reflect features that currently work with each firmware
 * variant via the M1's implementation:
 *   - M1_ESP32_CAP_BLE_HID: Bad-BT uses esp32_main_init() + AT commands
 *     (m1_badbt.c), which works on all AT variants.
 *   - M1_ESP32_CAP_802154: Zigbee/Thread uses spi_AT_send_recv() AT commands
 *     (m1_802154.c); note these delegates are non-capped so this bit is
 *     informational only.
 *   - WiFi connect / BT manage are excluded: those features use binary SPI
 *     commands in m1_wifi.c / m1_bt.c that AT firmware does not support.
 * =========================================================================*/

/** SiN360 binary-SPI firmware (sincere360/M1_SiN360_ESP32).
 *  Retained for reference and testing; not used as the active fallback. */
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

/** bedge117 / dag AT firmware (base AT feature set).
 *  Active fallback for firmware without CMD_GET_STATUS support. */
#define M1_ESP32_CAP_PROFILE_AT_BEDGE117 \
    (M1_ESP32_CAP_BLE_HID | \
     M1_ESP32_CAP_802154)

/** neddy299 AT firmware (base + WiFi station scan + deauth). */
#define M1_ESP32_CAP_PROFILE_AT_NEDDY299 \
    (M1_ESP32_CAP_PROFILE_AT_BEDGE117 | \
     M1_ESP32_CAP_WIFI_STA_SCAN       | \
     M1_ESP32_CAP_WIFI_ATTACK)

/* =========================================================================
 * CMD_GET_STATUS payload structure (protocol version 1)
 *
 * The ESP32 firmware returns this in the 60-byte resp.payload[] when it
 * receives CMD_GET_STATUS (0x02).  Firmware that does not implement
 * CMD_GET_STATUS will return RESP_ERR or time out, triggering the
 * compile-flag fallback.
 *
 * The single cap_bitmap field carries M1_ESP32_CAP_* bits directly.
 * Any firmware variant — binary-SPI or AT text commands — sets the same
 * bit for the same capability.  There is no derivation step on the STM32
 * side; a feature has exactly one flag regardless of how the ESP32 firmware
 * implements it internally.
 * =========================================================================*/

#define M1_ESP32_CAPS_PROTO_VER  1u

/**
 * Packed layout of the CMD_GET_STATUS response payload.
 * Total: 41 bytes — well within the 60-byte payload limit.
 *
 * Field order matches the ESP32 firmware's m1_protocol.h exactly.
 *   cap_bitmap — uint64_t, LE — M1_ESP32_CAP_* bits.
 *   fw_name    — null-terminated ASCII firmware identifier.
 */
typedef struct __attribute__((packed)) {
    uint8_t  proto_ver;   /**< M1_ESP32_CAPS_PROTO_VER (1) */
    uint64_t cap_bitmap;  /**< M1_ESP32_CAP_* bits, little-endian */
    char     fw_name[32]; /**< Firmware identifier string, null-terminated */
} m1_esp32_status_payload_t;

/* =========================================================================
 * Pure-logic parse helper (static inline — testable on host, no HAL deps)
 * =========================================================================*/

/**
 * Parse a raw CMD_GET_STATUS response payload and extract the capability
 * bitmap and firmware name.
 *
 * @param payload      Raw payload bytes from m1_resp_t.payload[]
 * @param len          Length of valid payload bytes (resp.payload_len)
 * @param caps_out     Receives the M1_ESP32_CAP_* capability bitmap
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

    *caps_out = p->cap_bitmap;
    strncpy(fw_name_out, p->fw_name, 31);
    fw_name_out[31] = '\0';
    return true;
}

/**
 * Parse an AT+GETSTATUSHEX response and extract the raw payload bytes.
 *
 * AT firmware variants that implement the feat/cmd_get_status extension
 * respond to "AT+GETSTATUSHEX\r\n" with a line of the form:
 *
 *   +GETSTATUSHEX:<82 uppercase hex chars>\r\n\r\nOK\r\n
 *
 * The 82 hex characters encode the same 41-byte m1_esp32_status_payload_t
 * that binary-SPI firmware returns for CMD_GET_STATUS (0x02).  The decoded
 * bytes can be passed directly to m1_esp32_caps_parse_payload().
 *
 * @param resp_buf    Buffer containing the full AT response string
 * @param decoded_out Receives the decoded raw bytes
 * @param decoded_len Capacity of decoded_out (must be >= sizeof(m1_esp32_status_payload_t))
 * @return true on success, false if the prefix was not found, the hex string
 *         is too short, or a non-hex character is encountered
 */
static inline bool m1_esp32_caps_parse_at_hex(const char *resp_buf,
                                               uint8_t    *decoded_out,
                                               uint8_t     decoded_len)
{
    /* Locate the response prefix */
    const char *pfx = "+GETSTATUSHEX:";
    const char *hex_start = strstr(resp_buf, pfx);
    if (!hex_start)
        return false;
    hex_start += 14u;  /* strlen("+GETSTATUSHEX:") == 14 */

    if (decoded_len < (uint8_t)sizeof(m1_esp32_status_payload_t))
        return false;

    const uint8_t n = (uint8_t)sizeof(m1_esp32_status_payload_t);  /* 41 */
    for (uint8_t i = 0u; i < n; i++)
    {
        char hi = hex_start[(uint8_t)(i * 2u)];
        char lo = hex_start[(uint8_t)(i * 2u + 1u)];
        if (!hi || !lo)
            return false;

        uint8_t hi_v = (hi >= '0' && hi <= '9') ? (uint8_t)(hi - '0') :
                       (hi >= 'A' && hi <= 'F') ? (uint8_t)(hi - 'A' + 10u) :
                       (hi >= 'a' && hi <= 'f') ? (uint8_t)(hi - 'a' + 10u) : 0xFFu;
        uint8_t lo_v = (lo >= '0' && lo <= '9') ? (uint8_t)(lo - '0') :
                       (lo >= 'A' && lo <= 'F') ? (uint8_t)(lo - 'A' + 10u) :
                       (lo >= 'a' && lo <= 'f') ? (uint8_t)(lo - 'a' + 10u) : 0xFFu;
        if (hi_v == 0xFFu || lo_v == 0xFFu)
            return false;

        decoded_out[i] = (uint8_t)((hi_v << 4u) | lo_v);
    }
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
