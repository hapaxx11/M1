/* See COPYING.txt for license details. */

/*
 * m1_esp32_caps.h
 *
 * Runtime capability descriptor for the ESP32-C6 coprocessor.
 *
 * Multiple ESP32 firmware variants are supported (SiN360 binary-SPI,
 * bedge117 AT, neddy299 AT-deauth, and future variants).  Each exposes
 * a different set of features.  This module provides:
 *
 *   1. A capability bitmap queried at runtime via CMD_GET_STATUS.
 *      Each bit corresponds to a named capability the ESP32 supports,
 *      regardless of how the firmware implements it internally.
 *   2. A backward-compatible fallback bitmap when the connected firmware
 *      does not implement CMD_GET_STATUS.
 *   3. A standard "feature not supported" UI helper so call sites never
 *      need to hard-code firmware-specific strings.
 *
 * Design principle: call sites specify exactly which capabilities they need.
 * There are no high-level feature groupings in this header — the system
 * does not define "WiFi attack" as a concept; instead M1_ESP32_CAP_DEAUTH,
 * M1_ESP32_CAP_BEACON, M1_ESP32_CAP_KARMA, etc. are separate bits.
 * Capability names are transport-agnostic: there is no AT vs. binary
 * distinction in the flag names.
 *
 * Rule: new ESP32-dependent features MUST gate on m1_esp32_require_cap()
 * using the M1_ESP32_CAP_* bit(s) for the specific capability they need.
 * Never gate on a compile flag or firmware name string.
 *
 * Wire protocol note:
 *   Two probes are issued in sequence:
 *     1. Binary `CMD_GET_STATUS` (0x02) — SiN360 binary-SPI firmware (and any
 *        AT firmware that implements this extension) self-reports the full
 *        capability bitmap and firmware name.
 *     2. Stock AT `AT+CMD?` — when the binary probe fails, the AT task is
 *        queried for its supported-command list, and a small mapping table
 *        in `m1_esp32_caps.c` translates the presence of specific AT
 *        commands into M1_ESP32_CAP_* bits.  This probe works against any
 *        stock or custom ESP-AT firmware without requiring our own
 *        extensions.
 *   If both probes fail, the capability bitmap is left at zero and feature
 *   gates fail closed.
 *
 * M1 Project
 */

#ifndef M1_ESP32_CAPS_H_
#define M1_ESP32_CAPS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>   /* size_t for parse helper */
#include <string.h>   /* strncpy/strstr/strlen used in inline parse helpers */

/* =========================================================================
 * Capability bits — one bit per named feature
 *
 * The cap_bitmap in CMD_GET_STATUS carries these bits.  Each bit signals
 * that the ESP32 firmware supports the named capability, regardless of
 * whether it is implemented via binary SPI commands or AT text commands.
 * Flag names are transport-agnostic: there is no AT vs. binary distinction.
 *
 * Call sites use these directly.  Example:
 *
 *   // Check a single capability before using it:
 *   if (!m1_esp32_require_cap(M1_ESP32_CAP_DEAUTH, "WiFi Deauth"))
 *       return;
 *
 *   // Require multiple capabilities to all be available:
 *   if (!m1_esp32_require_cap(M1_ESP32_CAP_KARMA | M1_ESP32_CAP_PORTAL,
 *                             "Karma Portal"))
 *       return;
 *
 * Assignment is permanent once published.
 * =========================================================================*/

/** WiFi scan (AP discovery) */
#define M1_ESP32_CAP_WIFI_SCAN      (UINT64_C(1) <<  0)

/** Station scan (promiscuous client discovery) */
#define M1_ESP32_CAP_STA_SCAN       (UINT64_C(1) <<  1)

/** BLE scan */
#define M1_ESP32_CAP_BLE_SCAN       (UINT64_C(1) <<  2)

/** BLE advertisement (custom adv payloads, spam, spoofing) */
#define M1_ESP32_CAP_BLE_ADV        (UINT64_C(1) <<  3)

/** WiFi deauthentication attack */
#define M1_ESP32_CAP_DEAUTH         (UINT64_C(1) <<  4)

/** Beacon flooding / clone / rickroll */
#define M1_ESP32_CAP_BEACON         (UINT64_C(1) <<  5)

/** Probe request flooding */
#define M1_ESP32_CAP_PROBE_FLOOD    (UINT64_C(1) <<  6)

/** Karma access-point impersonation + optional captive portal */
#define M1_ESP32_CAP_KARMA          (UINT64_C(1) <<  7)

/** Packet sniffer / monitor mode */
#define M1_ESP32_CAP_PKTMON         (UINT64_C(1) <<  8)

/** Evil portal (custom captive HTML + credential capture) */
#define M1_ESP32_CAP_PORTAL         (UINT64_C(1) <<  9)

/** WiFi station join / disconnect */
#define M1_ESP32_CAP_WIFI_JOIN      (UINT64_C(1) << 10)

/** WiFi MAC address spoofing */
#define M1_ESP32_CAP_WIFI_SET_MAC   (UINT64_C(1) << 11)

/** WiFi channel override */
#define M1_ESP32_CAP_WIFI_SET_CHAN  (UINT64_C(1) << 12)

/** Network scanner (ping / ARP / port / SSH / Telnet) */
#define M1_ESP32_CAP_NETSCAN        (UINT64_C(1) << 13)

/** BLE HID keyboard emulation (Bad-BT) */
#define M1_ESP32_CAP_BLE_HID        (UINT64_C(1) << 14)

/** Bluetooth device management (saved devices, BT info) */
#define M1_ESP32_CAP_BT_MANAGE      (UINT64_C(1) << 15)

/** IEEE 802.15.4 / Zigbee / Thread */
#define M1_ESP32_CAP_802154         (UINT64_C(1) << 16)

/* Bits 17-63 reserved for future use */

/* =========================================================================
 * Compile-time profile reference
 *
 * SiN360 binary-SPI firmware self-reports its capabilities via CMD_GET_STATUS
 * and never relies on this macro.  It is retained as documentation of the
 * SiN360 feature set and as a reference value for unit tests; runtime
 * detection always takes the firmware-reported path.
 * =========================================================================*/

/** SiN360 firmware profile (sincere360/M1_SiN360_ESP32) — reference only. */
#define M1_ESP32_CAP_PROFILE_SIN360 \
    (M1_ESP32_CAP_WIFI_SCAN    | \
     M1_ESP32_CAP_STA_SCAN     | \
     M1_ESP32_CAP_BLE_SCAN     | \
     M1_ESP32_CAP_BLE_ADV      | \
     M1_ESP32_CAP_DEAUTH       | \
     M1_ESP32_CAP_BEACON       | \
     M1_ESP32_CAP_PROBE_FLOOD  | \
     M1_ESP32_CAP_KARMA        | \
     M1_ESP32_CAP_PKTMON       | \
     M1_ESP32_CAP_PORTAL       | \
     M1_ESP32_CAP_NETSCAN)

/* AT firmware is detected at runtime via the stock `AT+CMD?` probe — no
 * curated AT fallback profile macros are defined here.  Adding a new AT
 * command to the runtime probe is a single-line edit to the
 * `s_at_cmd_cap_map[]` table in `m1_esp32_caps.c`. */

/* =========================================================================
 * Memory footprint estimates — for developer / diagnostic use only
 *
 * bss_bytes and free_heap_bytes are NOT part of the CMD_GET_STATUS wire
 * protocol.  They are derived from compile-time constants (below) that
 * reflect source-code analysis of the known Hapax-fork ESP32 firmware
 * releases.  m1_esp32_caps_bss_bytes() and m1_esp32_caps_free_heap() are
 * developer-diagnostic accessors — not user-visible.
 *
 * Profile discriminator: M1_ESP32_CAP_WIFI_JOIN present in bitmap →
 * AT/C3 profile (bedge117/neddy299); absent → SiN360 profile.
 *
 * Sources (see documentation/esp32_firmware.md for derivation rationale):
 *   SiN360  BSS : sincere360/M1_SiN360_ESP32 v0.9.0.8, ESP-IDF 5.5.4
 *   SiN360  heap: NimBLE with MSYS_BUF_FROM_HEAP=y; 10×1600 B static WiFi
 *                 RX; ap_records[64] ≈ 14 KB
 *   AT/C3   BSS : bedge117/esp32-at-monstatek-m1 v2.0.2, ESP-AT v4.0.0.0
 *   AT/C3   heap: Full AT infrastructure + SPI ring buffers + BLE HID +
 *                 802.15.4
 * =========================================================================*/
#define M1_ESP32_FALLBACK_BSS_SIN360   (200u * 1024u)  /**< ≈200 KB BSS */
#define M1_ESP32_FALLBACK_HEAP_SIN360  (160u * 1024u)  /**< ≈160 KB free heap */
#define M1_ESP32_FALLBACK_BSS_AT       (284u * 1024u)  /**< ≈284 KB BSS */
#define M1_ESP32_FALLBACK_HEAP_AT      (112u * 1024u)  /**< ≈112 KB free heap */

/* =========================================================================
 * CMD_GET_STATUS payload structure (protocol version 1)
 *
 * The ESP32 firmware returns this in the 60-byte resp.payload[] when it
 * receives CMD_GET_STATUS (0x02).  Firmware that does not implement
 * CMD_GET_STATUS (e.g. AT-based variants) will return RESP_ERR or time
 * out, triggering the stock `AT+CMD?` secondary probe.  If that also
 * fails, the capability bitmap is left at zero (fail-closed).
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
 *   cap_bitmap — uint64_t, LE — M1_ESP32_CAP_* capability bits.
 *   fw_name    — null-terminated ASCII firmware identifier.
 */
typedef struct __attribute__((packed)) {
    uint8_t  proto_ver;   /**< M1_ESP32_CAPS_PROTO_VER (1) */
    uint64_t cap_bitmap;  /**< M1_ESP32_CAP_* capability bits, little-endian */
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
    if (!payload || len < (uint8_t)sizeof(m1_esp32_status_payload_t))
        return false;

    if (payload[0] != M1_ESP32_CAPS_PROTO_VER)
        return false;

    /* cap_bitmap is at offset 1 — potentially unaligned on Cortex-M.
     * Use memcpy to avoid a fault and to handle the little-endian wire
     * format correctly on both LE targets (STM32/x86) and potential BE
     * test hosts. */
    uint64_t cap = 0u;
    memcpy(&cap, payload + 1, sizeof(uint64_t));
    *caps_out = cap;   /* wire format is LE; STM32 and x86 need no swap */

    /* fw_name is at offset 9 (1 proto_ver + 8 cap_bitmap) */
    strncpy(fw_name_out, (const char *)(payload + 9), 31);
    fw_name_out[31] = '\0';
    return true;
}

/**
 * One entry in the AT-command → capability-bit mapping table consulted by
 * `m1_esp32_caps_parse_at_cmd_list()`.  Each entry maps the full AT command
 * name (including the "AT+" prefix, e.g. "AT+CWJAP") to the M1_ESP32_CAP_*
 * bit that should be set when the connected ESP32 firmware advertises that
 * command in its `AT+CMD?` response.
 */
typedef struct {
    const char *at_cmd_name;  /**< Full AT command name including "AT+" prefix */
    uint64_t    cap_bit;      /**< M1_ESP32_CAP_* bit to set when present */
} m1_esp32_at_cmd_cap_entry_t;

/**
 * Parse the response from a stock ESP-AT `AT+CMD?` probe and assemble a
 * capability bitmap from the commands the firmware advertises.
 *
 * `AT+CMD?` is part of the stock ESP-AT command set (see
 * https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/Basic_AT_Commands.html#at-cmd)
 * and returns one line per supported command of the form:
 *
 *   +CMD:<index>,"<full command name>",<test>,<query>,<set>,<exec>\r\n
 *   ...
 *   OK\r\n
 *
 * The full command name (e.g. `"AT+CWJAP"`) appears in double quotes.  This
 * parser scans for `"<name>"` exact matches against each entry of the
 * caller-supplied table and OR's in the corresponding capability bit when
 * found.  Command names that the firmware does not list contribute nothing.
 *
 * @param resp_buf  Null-terminated buffer containing the full AT+CMD? response
 * @param table     Mapping of AT command name to M1_ESP32_CAP_* bit
 * @param table_n   Number of entries in @p table
 * @return          OR'd M1_ESP32_CAP_* bitmap of all matched commands.
 *                  Returns 0 if @p resp_buf or @p table is NULL.
 */
static inline uint64_t
m1_esp32_caps_parse_at_cmd_list(const char *resp_buf,
                                 const m1_esp32_at_cmd_cap_entry_t *table,
                                 size_t table_n)
{
    uint64_t caps = 0u;

    if (!resp_buf || !table)
        return 0u;

    for (size_t i = 0u; i < table_n; i++)
    {
        const char *name = table[i].at_cmd_name;
        if (!name || !name[0])
            continue;

        /* Build the quoted needle: e.g. "AT+CWJAP" → \"AT+CWJAP\".
         * The needle buffer holds: 1 leading quote + name + 1 trailing
         * quote + 1 null = nlen + 3 bytes.  40 bytes accommodates names up
         * to 37 chars, comfortably larger than the longest tracked command
         * ("AT+BLEHIDINIT" = 13 chars).  Names that would not fit are
         * silently skipped (caps unaffected) rather than risking a stack
         * overflow or truncated needle that could mis-match. */
        char needle[40];
        size_t nlen = strlen(name);
        if (nlen + 3u > sizeof(needle))
            continue;

        needle[0] = '"';
        for (size_t k = 0u; k < nlen; k++)
            needle[1u + k] = name[k];
        needle[1u + nlen]      = '"';
        needle[1u + nlen + 1u] = '\0';

        if (strstr(resp_buf, needle) != NULL)
            caps |= table[i].cap_bit;
    }

    return caps;
}

/**
 * Quick sanity check that a buffer looks like a real `AT+CMD?` response —
 * i.e. contains at least one `+CMD:` line.  Used to distinguish "probe
 * succeeded but no tracked commands matched" from "probe failed entirely".
 */
static inline bool m1_esp32_caps_at_cmd_response_valid(const char *resp_buf)
{
    return resp_buf && strstr(resp_buf, "+CMD:") != NULL;
}

/* =========================================================================
 * Public API
 * =========================================================================*/

/**
 * Probe the connected ESP32 firmware and cache its capability descriptor.
 * Must be called after m1_esp32_init() + esp32_main_init().
 *
 * Two probes are attempted in order:
 *   1. Binary CMD_GET_STATUS (0x02) — SiN360 / extension-aware firmware.
 *   2. Stock AT command `AT+CMD?` — translated against the
 *      `s_at_cmd_cap_map[]` table in m1_esp32_caps.c.
 *
 * If both probes fail, the capability bitmap is left at zero (feature
 * gates fail closed) and the firmware name is reported as
 * "Unknown (fallback)".  Safe to call multiple times — subsequent calls
 * are no-ops once a probe has succeeded.
 */
void m1_esp32_caps_init(void);

/**
 * Reset the capability cache.
 *
 * The capability cache normally persists for the lifetime of the STM32
 * firmware — capabilities are a property of the ESP32 firmware variant
 * and cannot change across a routine ESP32 deinit/init cycle.  This
 * function exists only for cases where the connected ESP32 firmware may
 * have changed since the last probe (e.g. immediately after an in-field
 * OTA reflash of the coprocessor).  Most callers should NOT invoke this
 * function — the cache is cleared automatically on STM32 reset.
 */
void m1_esp32_caps_reset(void);

/**
 * Return true if the ESP32 firmware supports all requested capabilities.
 * @param cap  One or more M1_ESP32_CAP_* bits OR'd together.
 *             Returns true only when every requested bit is set.
 */
bool m1_esp32_has_cap(uint64_t cap);

/**
 * Return a null-terminated string describing the active ESP32 firmware.
 * Examples: "SiN360-0.9.6", "AT (probed)", "Unknown (fallback)".
 * Never returns NULL.
 */
const char *m1_esp32_caps_fw_name(void);

/**
 * Check that all required capabilities are supported; if any are absent, draw a
 * standard "Feature not supported" screen (2 s) so the user knows why
 * nothing happened.
 *
 * @param cap           One or more M1_ESP32_CAP_* bits OR'd together
 * @param feature_name  Short human-readable feature name (e.g. "Saved Networks")
 * @return true if all requested capabilities are supported (caller may proceed),
 *         false if any are absent (screen has been shown, caller must abort)
 */
bool m1_esp32_require_cap(uint64_t cap, const char *feature_name);

/**
 * Return the estimated BSS footprint of the connected ESP32 firmware in bytes.
 * Derived from M1_ESP32_FALLBACK_BSS_* compile-time constants — never
 * transmitted over the wire.  For developer / OOM diagnostic use only.
 */
uint32_t m1_esp32_caps_bss_bytes(void);

/**
 * Return the estimated free heap of the connected ESP32 firmware in bytes.
 * Derived from M1_ESP32_FALLBACK_HEAP_* compile-time constants — never
 * transmitted over the wire.  For developer / OOM diagnostic use only.
 */
uint32_t m1_esp32_caps_free_heap(void);


#endif /* M1_ESP32_CAPS_H_ */
