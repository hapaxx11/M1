/* See COPYING.txt for license details. */

/*
 * m1_esp32_caps.h
 *
 * Runtime capability descriptor for the ESP32-C6 coprocessor.
 *
 * Multiple ESP32 firmware variants are supported (SiN360 binary-SPI,
 * CD3-AT (bedge117 base), neddy299 AT-deauth, and future variants).  Each exposes
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
 *   Three probes are issued in sequence:
 *     0. Binary `CMD_PING` (0x01) — SiN360 binary-SPI firmware detection.
 *        CMD_GET_STATUS was proposed as a capability-reporting command but
 *        never fully implemented in SiN360 (it returns only a 5-byte version
 *        payload, not the 41-byte capability report).  CMD_PING is universally
 *        implemented and returns "PONG" (4 bytes).  If this probe succeeds,
 *        we know we have SiN360 binary firmware and use M1_ESP32_CAP_PROFILE_SIN360.
 *     1. Binary `CMD_GET_STATUS` (0x02) — AT firmware that implements the
 *        full binary extension (e.g. hapaxx11/esp32-at-monstatek-m1) would
 *        self-report the full capability bitmap and firmware name here.
 *        Current SiN360 is already detected via Probe 0, so this is skipped.
 *     2. Stock AT `AT+CMD?` — when the binary probes fail, the AT task is
 *        queried for its supported-command list, and a small mapping table
 *        in `m1_esp32_caps.c` translates the presence of specific AT
 *        commands into M1_ESP32_CAP_* bits.  This probe works against any
 *        stock or custom ESP-AT firmware without requiring our own
 *        extensions.
 *   If all three probes fail, the capability bitmap is left at zero and
 *   feature gates fail closed.
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

/** BLE GATT client (connect to peer, enumerate services/characteristics,
 *  read/write/subscribe).  Exposed by SiN360 firmware via
 *  CMD_BLE_GATT_START/NEXT/STOP/WRITE/SUB/NOTIF (opcodes 0x28..0x2D). */
#define M1_ESP32_CAP_BLE_GATT       (UINT64_C(1) << 17)

/** Dedicated PMKID capture — send a targeted EAPOL-1 and capture the PMKID
 *  without requiring a full four-way handshake.  Distinct from
 *  M1_ESP32_CAP_PKTMON (passive packet monitor).
 *  Reserved for CD3 (bedge117/m1-esp32-brain) via M1_RPC_OFF_PMKID_CAPTURE,
 *  but as of the 2026-07-21 source review that message ID is NOT dispatched
 *  in any shipped CD3 release (falls through to NAK ERR_UNSUPPORTED) — do
 *  not assume CD3 devices support this without confirming their self-reported
 *  cap_bitmap.  Also exposed by dag T-800 AT firmware via AT+M1PMKID (this
 *  path IS functional).  Note: the dag mapping sets CAP_PKTMON for AT+M1PMKID
 *  (legacy overlap); new callers that specifically need PMKID should gate on
 *  M1_ESP32_CAP_PMKID. */
#define M1_ESP32_CAP_PMKID          (UINT64_C(1) << 18)

/** WPA handshake / EAPOL capture — deauth a station and capture the
 *  four-way EAPOL handshake for offline cracking.  Result exported as
 *  .pcap.  Dispatched by CD3 via M1_RPC_OFF_HS_START/STATUS/GET/STOP (this
 *  RPC path IS functional as of the 2026-07-21 source review), but CD3 does
 *  not yet set this bit in its self-reported cap_bitmap (M1_FW_CAPS in
 *  main.c) — m1_esp32_has_cap() will return false against a real device
 *  until a release starts advertising it.  Not self-reported by AT or SiN360
 *  firmware variants; dag T-800 provides AT+M1HSCAP mapped to CAP_PKTMON (legacy overlap). */
#define M1_ESP32_CAP_HANDSHAKE      (UINT64_C(1) << 19)

/** ESP32 firmware OTA self-update — M1 can push a new ESP32 binary to the
 *  coprocessor over the SPI link without re-flashing via UART/SD card.
 *  Reserved for CD3 via M1_RPC_SYS_OTA_BEGIN/DATA/END, but as of the
 *  2026-07-21 source review these message IDs are NOT dispatched in any
 *  shipped CD3 release (fall through to NAK ERR_UNSUPPORTED) — this is a
 *  protocol placeholder only, not a working feature.  Not available in AT
 *  or SiN360 firmware. */
#define M1_ESP32_CAP_OTA            (UINT64_C(1) << 20)

/** ESP-NOW peer-to-peer communication (discovery, unicast, broadcast).
 *  Supported by CD3 (bedge117/m1-esp32-brain) via M1_RPC_NOW_* handlers
 *  (msg_ids 0x0600..0x0605) but not yet self-reported in M1_FW_CAPS.
 *  Until CD3 sets this bit in M1_FW_CAPS, the feature gate will fail closed
 *  on CD3 builds.  No fallback probe is implemented. */
#define M1_ESP32_CAP_ESPNOW         (UINT64_C(1) << 21)

/* Bits 22-63 reserved for future use */

/* =========================================================================
 * Compile-time profile reference
 *
 * SiN360 binary-SPI firmware self-reports its capabilities via CMD_GET_STATUS
 * and never relies on this macro.  It is retained as documentation of the
 * SiN360 feature set and as a reference value for unit tests; runtime
 * detection always takes the firmware-reported path.
 * =========================================================================*/

/** SiN360 firmware profile (sincere360/M1_SiN360_ESP32) — reference only.
 *  Updated to include M1_ESP32_CAP_BLE_HID: SiN360 v0.9.1.0 added
 *  CMD_BLE_HID_START/STOP/STATUS/REPORT for binary-SPI BLE keyboard injection.
 *  Updated to include M1_ESP32_CAP_BLE_GATT: SiN360 exposes a NimBLE GATT
 *  client via CMD_BLE_GATT_START/NEXT/STOP/WRITE/SUB/NOTIF. */
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
     M1_ESP32_CAP_NETSCAN      | \
     M1_ESP32_CAP_BLE_HID      | \
     M1_ESP32_CAP_BLE_GATT)

/* AT firmware is detected at runtime via the stock `AT+CMD?` probe — no
 * curated AT fallback profile macros are defined here.  Adding a new AT
 * command to the runtime probe is a single-line edit to the
 * `s_at_cmd_cap_map[]` table in `m1_esp32_caps.c`. */

/* dag T-800 profile: AT firmware (dagnazty/M1-T-800) with custom WiFi and
 * HID AT commands.  Discriminated from stock CD3-AT (bedge117/neddy299) by the
 * presence of `M1_ESP32_CAP_BEACON` (AT+M1BEACON — only T-800 has it).
 * Does NOT include 802.15.4 despite having AT+ZIGSNIFF in some builds
 * because 802.15.4 is optional and may not be present in all T-800
 * configurations; the probe detects it dynamically. */
#define M1_ESP32_CAP_PROFILE_DAG_T800 \
    (M1_ESP32_CAP_WIFI_JOIN    | \
     M1_ESP32_CAP_WIFI_SCAN    | \
     M1_ESP32_CAP_DEAUTH       | \
     M1_ESP32_CAP_BEACON       | \
     M1_ESP32_CAP_KARMA        | \
     M1_ESP32_CAP_PORTAL       | \
     M1_ESP32_CAP_BLE_ADV      | \
     M1_ESP32_CAP_PKTMON       | \
     M1_ESP32_CAP_PROBE_FLOOD  | \
     M1_ESP32_CAP_BLE_HID)

/** CD3 native binary RPC firmware profile (bedge117/m1-esp32-brain) —
 *  reference/ASPIRATIONAL only, not what any shipped release currently
 *  self-reports.  Represents the full feature set the m1-native firmware is
 *  designed to eventually expose once all components are wired.  CD3
 *  self-reports its actual bitmap via M1_RPC_SYS_GET_STATUS; this macro is
 *  retained for unit tests and as a fallback when the GET_STATUS probe
 *  succeeds but returns an all-zero bitmap.
 *
 *  CD3 uses the M1_RPC binary SPI protocol (magic 0x4D31, "M1") and is
 *  detected by the M1_RPC probe in m1_esp32_caps_init().  Capabilities unique
 *  to CD3 vs SiN360/AT: PMKID, HANDSHAKE, OTA (bits 18-20) — but as of the
 *  2026-07-21 source review, PMKID and OTA are reserved message IDs with no
 *  dispatch in main.c, and HANDSHAKE is dispatched but not yet included in
 *  the firmware's self-reported M1_FW_CAPS.  See the CAP_PMKID/HANDSHAKE/OTA
 *  comments above for details before relying on this macro's bits 18-20.
 *  NETSCAN absent (no ping/ARP scanner component in v1.x).
 *  BT_MANAGE absent (no Bluetooth Classic component). */
#define M1_ESP32_CAP_PROFILE_CD3 \
    (M1_ESP32_CAP_WIFI_SCAN    | \
     M1_ESP32_CAP_WIFI_JOIN    | \
     M1_ESP32_CAP_STA_SCAN     | \
     M1_ESP32_CAP_DEAUTH       | \
     M1_ESP32_CAP_BEACON       | \
     M1_ESP32_CAP_PROBE_FLOOD  | \
     M1_ESP32_CAP_KARMA        | \
     M1_ESP32_CAP_PKTMON       | \
     M1_ESP32_CAP_PORTAL       | \
     M1_ESP32_CAP_PMKID        | \
     M1_ESP32_CAP_HANDSHAKE    | \
     M1_ESP32_CAP_BLE_SCAN     | \
     M1_ESP32_CAP_BLE_ADV      | \
     M1_ESP32_CAP_BLE_HID      | \
     M1_ESP32_CAP_BLE_GATT     | \
     M1_ESP32_CAP_802154       | \
     M1_ESP32_CAP_OTA)

/* =========================================================================
 * Memory footprint estimates — for developer / diagnostic use only
 *
 * bss_bytes and free_heap_bytes are NOT part of the CMD_GET_STATUS wire
 * protocol.  They are derived from compile-time constants (below) that
 * reflect source-code analysis of the known Hapax-fork ESP32 firmware
 * releases.  m1_esp32_caps_bss_bytes() and m1_esp32_caps_free_heap() are
 * developer-diagnostic accessors — not user-visible.
 *
 * Profile discriminator (four-way; evaluated in priority order):
 *   HANDSHAKE + OTA present → CD3 native (bedge117/m1-esp32-brain) — see
 *                             caveat: no shipped release currently sets
 *                             both bits (or either bit)
 *   WIFI_JOIN + BEACON      → dag T-800 (AT firmware with custom cmds)
 *   WIFI_JOIN alone         → CD3-AT (bedge117/neddy299)
 *   neither WIFI_JOIN       → SiN360 binary-SPI
 *
 * Sources (see documentation/esp32_firmware.md for derivation rationale):
 *   SiN360  BSS : sincere360/M1_SiN360_ESP32 v0.9.0.8, ESP-IDF 5.5.4
 *   SiN360  heap: NimBLE with MSYS_BUF_FROM_HEAP=y; 10×1600 B static WiFi
 *                 RX; ap_records[64] ≈ 14 KB
 *   CD3-AT  BSS : bedge117/esp32-at-monstatek-m1 v2.0.2, ESP-AT v4.0.0.0
 *   CD3-AT  heap: Full AT infrastructure + SPI ring buffers + BLE HID +
 *                 802.15.4
 *   CD3     BSS : bedge117/m1-esp32-brain v1.x (estimated; no map file yet)
 *   CD3     heap: Native ESP-IDF WiFi/BLE/802.15.4 without AT overhead
 * =========================================================================*/
#define M1_ESP32_FALLBACK_BSS_SIN360   (200u * 1024u)  /**< ≈200 KB BSS */
#define M1_ESP32_FALLBACK_HEAP_SIN360  (160u * 1024u)  /**< ≈160 KB free heap */
#define M1_ESP32_FALLBACK_BSS_AT       (284u * 1024u)  /**< ≈284 KB BSS */
#define M1_ESP32_FALLBACK_HEAP_AT      (112u * 1024u)  /**< ≈112 KB free heap */

/* dag T-800 AT firmware (dagnazty/M1-T-800): ESP-AT base + 14 custom AT
 * commands.  Estimated from ESP-AT v4.0.0.0 base + dag custom modules
 * (WiFi attacks, HID, Zigbee sniff).  BSS is comparable to stock AT;
 * heap is slightly lower due to additional static buffers for deauth,
 * beacon, and monitor functions.  To be refined once actual T-800
 * firmware measurements are available (see CLAUDE.md update procedure). */
#define M1_ESP32_FALLBACK_BSS_T800     (290u * 1024u)  /**< ≈290 KB BSS */
#define M1_ESP32_FALLBACK_HEAP_T800    (105u * 1024u)  /**< ≈105 KB free heap */

/* CD3 native binary RPC firmware (bedge117/m1-esp32-brain v1.x):
 * Native ESP-IDF WiFi + NimBLE + 802.15.4, no AT overhead.
 * Estimated conservatively; to be refined once actual hardware
 * measurements are available (see documentation/esp32_firmware.md
 * update procedure). */
#define M1_ESP32_FALLBACK_BSS_CD3      (185u * 1024u)  /**< ≈185 KB BSS (estimated) */
#define M1_ESP32_FALLBACK_HEAP_CD3     (175u * 1024u)  /**< ≈175 KB free heap (estimated) */

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
    *caps_out = cap;   /* wire format is LE; on LE hosts (STM32/x86) memcpy
                        * preserves the encoding correctly.  On a BE host a
                        * bswap64 would be needed here. */

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

/**
 * Decide whether the Probe 3 (`AT+CMD?`) probe should proceed, given the AT
 * task's readiness before and after an attempt to start it.
 *
 * Regression fixed by this predicate: `m1_esp32_caps_init()` used to
 * unconditionally skip Probe 3 whenever the AT task was not already
 * running, instead of starting it (the same pattern already used by
 * `wifi_do_scan()` in m1_wifi.c).  Capability-gated scene delegates
 * (`DELEGATE_FEATURE` in m1_wifi_scene_menu.c and friends) only guarantee
 * the SPI transport is up via `m1_esp32_ensure_init()` before checking a
 * capability — none of them start the AT task first, since that normally
 * happens *inside* the feature function itself, after the gate has already
 * been evaluated.  The result: on pure-AT firmware (dag T-800, bedge117)
 * navigated to directly (e.g. WiFi -> 802.15.4 -> Zigbee Scan, or Attacks
 * -> PMKID Grab, as the very first ESP32 interaction), capability
 * detection would permanently fail — firmware name stuck at "Unknown" and
 * every capability bit unset (e.g. "Requires T-800" shown even when T-800
 * firmware is actually flashed) — because the probe was skipped forever.
 *
 * @param at_task_running_before             AT task status before any start
 *                                            attempt (get_esp32_main_init_status()).
 * @param at_task_running_after_start_attempt AT task status after calling
 *                                            esp32_main_init() when it was
 *                                            not already running.
 * @return true if Probe 3 should run now.
 */
static inline bool
m1_esp32_caps_should_run_at_probe(bool at_task_running_before,
                                   bool at_task_running_after_start_attempt)
{
    return at_task_running_before || at_task_running_after_start_attempt;
}

/* =========================================================================
 * M1_RPC binary SPI protocol helpers (CD3 / bedge117/m1-esp32-brain)
 *
 * The CD3 firmware (bedge117/m1-esp32-brain) uses a structured binary RPC
 * protocol over the same SPI-HD transport as the AT firmware, but with a
 * distinct frame format:
 *
 *   [ magic:2 ][ version:1 ][ msg_type:1 ][ msg_id:2 LE ][ payload_len:2 LE ]
 *   [ payload:0..N ][ CRC16:2 ]
 *
 * where magic = 0x4D31 ("M1" in little-endian — wire bytes [0x31, 0x4D])
 * and CRC16 is CRC-16/CCITT (poly 0x1021, init 0xFFFF) over header+payload.
 *
 * These helpers are pure-logic (no HAL/RTOS deps) and are used both by the
 * Probe 3 detection path in m1_esp32_caps_init() and by host-side unit tests.
 * Defined as static inline to remain header-only.
 *
 * The GET_STATUS response payload (m1_esp32_rpc_devstatus_t) has the same
 * 41-byte layout as m1_esp32_status_payload_t: proto_ver (1), cap_bitmap
 * (8 bytes LE), fw_name (32 bytes).
 * =========================================================================*/

/* M1_RPC frame constants */
#define M1_ESP32_RPC_MAGIC        UINT16_C(0x4D31)  /**< "M1" LE — wire: [0x31, 0x4D] */
#define M1_ESP32_RPC_VERSION      UINT8_C(0x01)     /**< Protocol version */
#define M1_ESP32_RPC_HDR_SIZE     8u                /**< Header size in bytes */
#define M1_ESP32_RPC_CRC_SIZE     2u                /**< CRC16 trailer size in bytes */

/* M1_RPC message type values */
#define M1_ESP32_RPC_REQ          UINT8_C(0x01)  /**< Request (STM32 to ESP32) */
#define M1_ESP32_RPC_RESP         UINT8_C(0x02)  /**< Successful response */
#define M1_ESP32_RPC_NAK          UINT8_C(0x05)  /**< Error response */

/* M1_RPC system command IDs */
#define M1_ESP32_RPC_SYS_PING        UINT16_C(0x0001)  /**< PING: cookie echo */
#define M1_ESP32_RPC_SYS_GET_STATUS  UINT16_C(0x0002)  /**< GET_STATUS: devstatus */

/**
 * CD3 M1_RPC GET_STATUS response payload.
 *
 * 41 bytes: proto_ver (1), cap_bitmap[8] (8 bytes LE uint64), fw_name[32].
 * Wire-identical to m1_esp32_status_payload_t on LE targets.
 */
typedef struct __attribute__((packed)) {
    uint8_t  proto_ver;      /**< M1_ESP32_RPC_VERSION (1) */
    uint8_t  cap_bitmap[8];  /**< M1_ESP32_CAP_* bits, LE uint64 as 8 bytes */
    char     fw_name[32];    /**< Null-terminated ASCII firmware identifier */
} m1_esp32_rpc_devstatus_t;

/**
 * Unpack the LE 8-byte cap_bitmap from an m1_esp32_rpc_devstatus_t into a
 * uint64_t capability bitmap.
 */
static inline uint64_t m1_esp32_rpc_caps_get(const uint8_t bitmap[8])
{
    uint64_t cap = 0u;
    for (unsigned i = 0u; i < 8u; i++)
        cap |= ((uint64_t)bitmap[i]) << (i * 8u);
    return cap;
}

/**
 * CRC-16/CCITT (poly 0x1021, init 0xFFFF) over a byte buffer.
 * Used to compute and verify M1_RPC frame CRCs.
 */
static inline uint16_t m1_esp32_rpc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0u; i < len; i++)
    {
        crc ^= ((uint16_t)data[i]) << 8u;
        for (unsigned j = 0u; j < 8u; j++)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1u) ^ 0x1021u)
                                  : (uint16_t)(crc << 1u);
    }
    return crc;
}

/**
 * Build an M1_RPC request frame into @p buf.
 *
 * Writes the 8-byte header, copies @p payload_len bytes of @p payload, then
 * appends the 2-byte CRC16.  The buffer is NOT zero-padded past the frame.
 *
 * @param buf          Output buffer (must be >= 8 + payload_len + 2 bytes)
 * @param buf_size     Capacity of @p buf
 * @param msg_id       M1_ESP32_RPC_SYS_* command ID (stored LE)
 * @param payload      Payload bytes (may be NULL when payload_len == 0)
 * @param payload_len  Number of payload bytes
 * @return Total frame size (8 + payload_len + 2) on success; 0 if buf too small
 */
static inline uint16_t
m1_esp32_rpc_build_req(uint8_t *buf, uint16_t buf_size,
                        uint16_t msg_id,
                        const uint8_t *payload, uint16_t payload_len)
{
    uint16_t frame_size = (uint16_t)(M1_ESP32_RPC_HDR_SIZE + payload_len +
                                     M1_ESP32_RPC_CRC_SIZE);
    if (!buf || buf_size < frame_size || (payload_len > 0u && !payload))
        return 0u;

    buf[0] = (uint8_t)(M1_ESP32_RPC_MAGIC        & 0xFFu);
    buf[1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
    buf[2] = M1_ESP32_RPC_VERSION;
    buf[3] = M1_ESP32_RPC_REQ;
    buf[4] = (uint8_t)(msg_id        & 0xFFu);
    buf[5] = (uint8_t)((msg_id >> 8u) & 0xFFu);
    buf[6] = (uint8_t)(payload_len        & 0xFFu);
    buf[7] = (uint8_t)((payload_len >> 8u) & 0xFFu);

    if (payload && payload_len > 0u)
    {
        for (uint16_t i = 0u; i < payload_len; i++)
            buf[M1_ESP32_RPC_HDR_SIZE + i] = payload[i];
    }

    uint16_t crc = m1_esp32_rpc_crc16(buf, M1_ESP32_RPC_HDR_SIZE + payload_len);
    buf[M1_ESP32_RPC_HDR_SIZE + payload_len]      = (uint8_t)(crc        & 0xFFu);
    buf[M1_ESP32_RPC_HDR_SIZE + payload_len + 1u] = (uint8_t)((crc >> 8u) & 0xFFu);

    return frame_size;
}

/**
 * Parse an M1_RPC response frame received from CD3 firmware.
 *
 * Validates magic (0x4D31), version, CRC16, msg_type == RESP, and
 * msg_id == @p expected_msg_id.  On success sets @p *payload_out and
 * @p *payload_len_out.
 *
 * @param buf               Received buffer
 * @param buf_len           Valid bytes in @p buf
 * @param expected_msg_id   M1_ESP32_RPC_SYS_* ID we expect in the response
 * @param payload_out       Points into @p buf at the payload start on success
 * @param payload_len_out   Payload byte count on success
 * @return true on a valid matching response; false otherwise
 */
static inline bool
m1_esp32_rpc_parse_resp(const uint8_t *buf, uint16_t buf_len,
                         uint16_t expected_msg_id,
                         const uint8_t **payload_out,
                         uint16_t *payload_len_out)
{
    if (payload_out)     *payload_out = NULL;
    if (payload_len_out) *payload_len_out = 0u;
    if (!buf || !payload_out || !payload_len_out ||
        buf_len < (uint16_t)(M1_ESP32_RPC_HDR_SIZE + M1_ESP32_RPC_CRC_SIZE))
        return false;
    uint16_t magic = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8u);
    if (magic != M1_ESP32_RPC_MAGIC)
        return false;
    if (buf[2] != M1_ESP32_RPC_VERSION)
        return false;
    if (buf[3] != M1_ESP32_RPC_RESP)
        return false;

    uint16_t msg_id = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8u);
    if (msg_id != expected_msg_id)
        return false;

    uint16_t plen  = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8u);
    uint16_t total = (uint16_t)(M1_ESP32_RPC_HDR_SIZE + plen + M1_ESP32_RPC_CRC_SIZE);
    if (total > buf_len)
        return false;

    uint16_t expected_crc = m1_esp32_rpc_crc16(buf, M1_ESP32_RPC_HDR_SIZE + plen);
    uint16_t wire_crc =
        (uint16_t)buf[M1_ESP32_RPC_HDR_SIZE + plen] |
        ((uint16_t)buf[M1_ESP32_RPC_HDR_SIZE + plen + 1u] << 8u);
    if (wire_crc != expected_crc)
        return false;

    *payload_out     = buf + M1_ESP32_RPC_HDR_SIZE;
    *payload_len_out = plen;
    return true;
}

/* =========================================================================
 * Public API
 * =========================================================================*/

/**
 * Probe the connected ESP32 firmware and cache its capability descriptor.
 * Must be called after m1_esp32_init() + esp32_main_init().
 *
 * Four probes are attempted in order:
 *   - Binary CMD_PING (0x01) — SiN360 binary-SPI firmware detection.
 *   - Binary CMD_GET_STATUS (0x02) — SiN360 / extension-aware firmware.
 *   - M1_RPC PING (magic 0x4D31) — CD3 native binary RPC firmware
 *     (bedge117/m1-esp32-brain), followed by M1_RPC GET_STATUS.
 *   - Stock AT command `AT+CMD?` — translated against the
 *     `s_at_cmd_cap_map[]` table in m1_esp32_caps.c.
 *
 * If all probes fail, the capability bitmap is left at zero (feature
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

/**
 * Return the full cached capability bitmap.
 *
 * Returns 0 when the cache has not yet been populated (i.e. before
 * m1_esp32_caps_init() completes successfully) so callers who need all
 * bits in one shot can pass it to esp32_feature_map helpers without
 * issuing multiple m1_esp32_has_cap() calls.
 *
 * The return value is the same bitmap that m1_esp32_has_cap() queries
 * internally; reading it does not re-probe the firmware.
 */
uint64_t m1_esp32_caps_get_bitmap(void);


#endif /* M1_ESP32_CAPS_H_ */
