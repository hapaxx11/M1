/* See COPYING.txt for license details. */

/*
 * m1_esp32_rpc.h
 *
 * M1 <-> ESP32-C6 M1_RPC compatibility layer (host / STM32 side).
 *
 * Background
 * ----------
 * The M1 speaks three mutually-exclusive command protocols to its ESP32-C6
 * coprocessor over the same SPI-HD hardware:
 *
 *   - AT text commands ("AT+...\r\n") — bedge117 / neddy299 / dag builds and
 *     the legacy CD3-AT firmware,
 *   - the 64-byte binary CMD_* protocol (m1_esp32_cmd.c) — SiN360,
 *   - the M1_RPC binary framing (magic 0x4D31) — the native "brain" CD3
 *     (bedge117/m1-esp32-brain).
 *
 * There are TWO distinct CD3 firmwares.  The legacy **CD3-AT** speaks AT text
 * commands and is handled by the existing AT path unchanged; this module does
 * not touch it (esp32_firmware_transport() classifies it as
 * ESP32_TRANSPORT_AT).  Only the newer native **brain CD3** speaks M1_RPC and
 * is what this compatibility layer targets.  Both remain fully supported.
 *
 * Historically only ESP-NOW spoke M1_RPC; every other WiFi / BLE / 802.15.4
 * feature emitted AT text commands, so those features stayed dark against the
 * native brain CD3 firmware (which does not implement the AT command set).  This
 * module is the missing reusable M1_RPC feature layer: a single robust RPC
 * client any feature module can call, plus the canonical opcode map and the
 * transport-mode selector used to choose between the three encoders.
 *
 * The opcode enum and payload structs mirror the canonical shared header
 * `components/m1_rpc/include/m1_rpc.h` in bedge117/m1-esp32-brain.  The frame
 * build / parse primitives and the M1_RPC frame constants (magic, version,
 * header/CRC sizes, REQ/RESP/NAK, SYS_PING, SYS_GET_STATUS) already live in
 * m1_esp32_caps.h and are reused verbatim here — this header does not
 * redefine them.
 *
 * M1 Project
 */

#ifndef M1_ESP32_RPC_H_
#define M1_ESP32_RPC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "m1_esp32_caps.h"     /* frame constants + build/parse inline helpers */
#include "esp32_feature_map.h" /* esp32_transport_t + esp32_firmware_transport */

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Message IDs (16-bit, range-grouped) — canonical m1_rpc_id_t.
 *
 * SYS_PING (0x0001) and SYS_GET_STATUS (0x0002) already exist as the macros
 * M1_ESP32_RPC_SYS_PING / M1_ESP32_RPC_SYS_GET_STATUS in m1_esp32_caps.h
 * (used by the CD3 detection probe) and are intentionally NOT duplicated here.
 * All identifiers use the M1_ESP32_RPC_ prefix to stay consistent with the
 * existing constants and avoid clashing with the desktop RPC (RPC_CMD_*) in
 * m1_rpc.h.
 * =========================================================================*/
typedef enum {
    /* ---- System 0x0000-0x00FF ---- */
    M1_ESP32_RPC_SYS_GET_FW_VERSION = 0x0003,
    M1_ESP32_RPC_SYS_GET_HEAP       = 0x0004,
    M1_ESP32_RPC_SYS_RESET          = 0x0005,
    M1_ESP32_RPC_SYS_OTA_BEGIN      = 0x0006,
    M1_ESP32_RPC_SYS_OTA_DATA       = 0x0007,
    M1_ESP32_RPC_SYS_OTA_END        = 0x0008,
    M1_ESP32_RPC_SYS_SNTP_SYNC      = 0x0009,

    /* ---- WiFi station 0x0100-0x01FF ---- */
    M1_ESP32_RPC_WIFI_GET_MODE      = 0x0100,
    M1_ESP32_RPC_WIFI_SET_MODE      = 0x0101,
    M1_ESP32_RPC_WIFI_GET_MAC       = 0x0102,
    M1_ESP32_RPC_WIFI_SCAN          = 0x0103,
    M1_ESP32_RPC_WIFI_CONNECT       = 0x0104,
    M1_ESP32_RPC_WIFI_DISCONNECT    = 0x0105,
    M1_ESP32_RPC_WIFI_GET_STATUS    = 0x0106,

    /* ---- WiFi AP 0x0200-0x02FF ---- */
    M1_ESP32_RPC_SOFTAP_START       = 0x0200,
    M1_ESP32_RPC_SOFTAP_STOP        = 0x0201,
    M1_ESP32_RPC_SOFTAP_STA_LIST    = 0x0202,

    /* ---- Offensive WiFi 0x0300-0x03FF ---- */
    M1_ESP32_RPC_OFF_MONITOR_START  = 0x0300,
    M1_ESP32_RPC_OFF_MONITOR_STOP   = 0x0301,
    M1_ESP32_RPC_OFF_DEAUTH_START   = 0x0302,
    M1_ESP32_RPC_OFF_DEAUTH_STOP    = 0x0303,
    M1_ESP32_RPC_OFF_BEACON_START   = 0x0304,
    M1_ESP32_RPC_OFF_BEACON_STOP    = 0x0305,
    M1_ESP32_RPC_OFF_PROBE_START    = 0x0306,
    M1_ESP32_RPC_OFF_PROBE_STOP     = 0x0307,
    M1_ESP32_RPC_OFF_PMKID_CAPTURE  = 0x0308,
    M1_ESP32_RPC_OFF_KARMA_START    = 0x0309,
    M1_ESP32_RPC_OFF_KARMA_STOP     = 0x030A,
    M1_ESP32_RPC_OFF_HANDSHAKE_CAP  = 0x030B,
    M1_ESP32_RPC_OFF_RAW_TX         = 0x030C,
    M1_ESP32_RPC_OFF_DEAUTH_STATUS  = 0x030D,
    M1_ESP32_RPC_OFF_STA_SCAN_START = 0x030E,
    M1_ESP32_RPC_OFF_STA_SCAN_RESULTS = 0x030F,
    M1_ESP32_RPC_OFF_HS_START       = 0x0310,
    M1_ESP32_RPC_OFF_HS_STATUS      = 0x0311,
    M1_ESP32_RPC_OFF_HS_GET         = 0x0312,
    M1_ESP32_RPC_OFF_HS_STOP        = 0x0313,
    M1_ESP32_RPC_OFF_MONITOR_STATS  = 0x0314,
    M1_ESP32_RPC_OFF_MONITOR_READ   = 0x0315,
    M1_ESP32_RPC_OFF_CAPTIVE_START  = 0x0316,
    M1_ESP32_RPC_OFF_CAPTIVE_STOP   = 0x0317,
    M1_ESP32_RPC_OFF_CAPTIVE_CREDS  = 0x0318,
    M1_ESP32_RPC_OFF_CAPTIVE_DIAG   = 0x0319,

    /* ---- BLE 0x0400-0x04FF ---- */
    M1_ESP32_RPC_BLE_INIT           = 0x0400,
    M1_ESP32_RPC_BLE_SCAN_START     = 0x0401,
    M1_ESP32_RPC_BLE_SCAN_RESULTS   = 0x0402,
    M1_ESP32_RPC_BLE_ADV_START      = 0x0403,
    M1_ESP32_RPC_BLE_ADV_STOP       = 0x0404,
    M1_ESP32_RPC_BLE_HID_INIT       = 0x0405,
    M1_ESP32_RPC_BLE_HID_KEY        = 0x0406,
    M1_ESP32_RPC_BLE_HID_STRING     = 0x0407,
    M1_ESP32_RPC_BLE_HID_SCRIPT     = 0x0408,
    M1_ESP32_RPC_BLE_CONNECT        = 0x0409,
    M1_ESP32_RPC_BLE_DISCONNECT     = 0x040A,
    M1_ESP32_RPC_BLE_HID_DEINIT     = 0x040B,
    M1_ESP32_RPC_BLE_HID_STATUS     = 0x040C,
    M1_ESP32_RPC_BLE_SPAM_START     = 0x040D,
    M1_ESP32_RPC_BLE_SPAM_STOP      = 0x040E,
    M1_ESP32_RPC_BLE_RPC_ADV        = 0x040F,

    /* ---- Zigbee / 802.15.4 0x0500-0x05FF ---- */
    M1_ESP32_RPC_ZB_INIT            = 0x0500,
    M1_ESP32_RPC_ZB_SCAN            = 0x0501,
    M1_ESP32_RPC_ZB_SNIFF_START     = 0x0502,
    M1_ESP32_RPC_ZB_SNIFF_STOP      = 0x0503,
    M1_ESP32_RPC_ZB_SNIFF_GET       = 0x0504,
    M1_ESP32_RPC_ZB_FLOOD_START     = 0x0505,
    M1_ESP32_RPC_ZB_FLOOD_STOP      = 0x0506,
    M1_ESP32_RPC_ZB_INJECT          = 0x0507,
    M1_ESP32_RPC_THREAD_SCAN        = 0x0510,

    /* ---- ESP-NOW peer-to-peer M1<->M1 link 0x0600-0x06FF ---- */
    M1_ESP32_RPC_NOW_START          = 0x0600,
    M1_ESP32_RPC_NOW_STOP           = 0x0601,
    M1_ESP32_RPC_NOW_ANNOUNCE       = 0x0602,
    M1_ESP32_RPC_NOW_PEERS_GET      = 0x0603,
    M1_ESP32_RPC_NOW_SEND           = 0x0604,
    M1_ESP32_RPC_NOW_RECV_GET       = 0x0605,

    /* ---- Screen / qMonstatek bridge 0x0700-0x07FF ---- */
    M1_ESP32_RPC_SCREEN_PUSH        = 0x0700,
    M1_ESP32_RPC_RPC_STATUS         = 0x0701,
    M1_ESP32_RPC_QMON_POLL          = 0x0702,
    M1_ESP32_RPC_QMON_RESP          = 0x0703,
} m1_esp32_rpc_id_t;

/* Additional message-type values beyond REQ/RESP/NAK (in m1_esp32_caps.h). */
#define M1_ESP32_RPC_IDLE   UINT8_C(0x00)  /**< No-op filler frame */
#define M1_ESP32_RPC_EVENT  UINT8_C(0x03)  /**< Unsolicited async notification */
#define M1_ESP32_RPC_FRAG   UINT8_C(0x04)  /**< Continuation fragment */

/* =========================================================================
 * Status codes returned by m1_esp32_rpc_call().
 *
 * 0x00-0x0B and 0xFF mirror the canonical on-wire m1_rpc_status_t exactly.
 * The two 0xFD/0xFE codes are host-side only (never appear on the wire): they
 * report a transport failure or a malformed / unmatched response frame so the
 * caller can distinguish "the ESP32 said no" from "we never got a valid
 * reply".
 * =========================================================================*/
typedef enum {
    M1_ESP32_RPC_OK             = 0x00,
    M1_ESP32_RPC_ERR_UNKNOWN    = 0x01,
    M1_ESP32_RPC_ERR_INVALID    = 0x02, /**< invalid args */
    M1_ESP32_RPC_ERR_BUSY       = 0x03,
    M1_ESP32_RPC_ERR_TIMEOUT    = 0x04,
    M1_ESP32_RPC_ERR_NO_MEM     = 0x05,
    M1_ESP32_RPC_ERR_NOT_INIT   = 0x06,
    M1_ESP32_RPC_ERR_ALREADY    = 0x07, /**< already running */
    M1_ESP32_RPC_ERR_NOT_RUN    = 0x08, /**< not running */
    M1_ESP32_RPC_ERR_HARDWARE   = 0x09,
    M1_ESP32_RPC_ERR_UNSUPPORTED= 0x0A, /**< command / capability absent */
    M1_ESP32_RPC_ERR_BAD_CRC    = 0x0B,

    /* ---- host-side only (not on the wire) ---- */
    M1_ESP32_RPC_ERR_TRANSPORT  = 0xFD, /**< SPI transport failed / no reply */
    M1_ESP32_RPC_ERR_BAD_FRAME  = 0xFE, /**< malformed / unmatched response */

    M1_ESP32_RPC_PENDING        = 0xFF, /**< accepted; result arrives as EVENT */
} m1_esp32_rpc_status_t;

/* =========================================================================
 * Frame sizing
 *
 * Single-transaction control commands and their requests fit comfortably in
 * this budget (ESP-NOW frames are <= 64 bytes; deauth / connect / HID-string
 * requests are well under 246 payload bytes), so M1_ESP32_RPC_FRAME_MAX still
 * bounds outbound REQ frames (built into the small stack buffer in
 * m1_esp32_rpc_call()).
 * =========================================================================*/
#define M1_ESP32_RPC_FRAME_MAX   256u
#define M1_ESP32_RPC_PAYLOAD_MAX \
    (M1_ESP32_RPC_FRAME_MAX - M1_ESP32_RPC_HDR_SIZE - M1_ESP32_RPC_CRC_SIZE)

/* =========================================================================
 * Response reception budget.
 *
 * The brain firmware (handle_wifi_scan() / handle_sta_scan_results() /
 * handle_ble_scan_results() in bedge117/m1-esp32-brain main.c) does NOT
 * chunk bulk-list responses behind separate *_GET calls -- it returns the
 * whole list (up to M1_SCAN_RESP_MAX == 1800 payload bytes on the firmware
 * side) as a single logical RPC response, relying on the M1 Link transport's
 * own FRAG reassembly (m1_esp32_m1link_send_recv()) to deliver it in one
 * piece. The old 256-byte single-frame ceiling (246 payload bytes) silently
 * discarded that assumption: any WIFI_SCAN/STA_SCAN/BLE_SCAN_RESULTS reply
 * larger than roughly 6-8 APs overflowed the M1 Link reassembly buffer,
 * m1_esp32_m1link_send_recv() returned "reassembly overflow", and the whole
 * RPC call failed with M1_ESP32_RPC_ERR_TRANSPORT -- which is why AP Scan /
 * 2.4G Survey / Station Scan reliably failed with "AP scan failed. Please
 * try again." in any real (non-empty) RF environment. Size the reception
 * buffer to the firmware's actual maximum instead of an arbitrary control-
 * command budget. This buffer is heap-allocated (see m1_esp32_rpc_call()),
 * not stack, to avoid growing every RPC call's stack frame. */
#define M1_ESP32_RPC_RESP_FRAME_MAX   2048u
#define M1_ESP32_RPC_RESP_PAYLOAD_MAX \
    (M1_ESP32_RPC_RESP_FRAME_MAX - M1_ESP32_RPC_HDR_SIZE - M1_ESP32_RPC_CRC_SIZE)

/* =========================================================================
 * Canonical payload structs (mirrored from bedge117/m1-esp32-brain m1_rpc.h).
 * All multi-byte fields are little-endian, matching both the Cortex-M33 host
 * and the RISC-V slave, so these packed structs map straight onto the wire.
 * =========================================================================*/

/* WIFI_SCAN response entry (fixed prefix; ssid_len ssid bytes follow). */
typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    int8_t   rssi;
    uint8_t  channel;
    uint8_t  authmode;
    uint8_t  ssid_len;
} m1_esp32_rpc_scan_entry_t;

/* OFF_DEAUTH_START request. */
typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    uint8_t  channel;
    uint8_t  station[6];   /* ff:ff:ff:ff:ff:ff = broadcast */
    uint16_t count;        /* 0 = infinite */
    uint16_t interval_ms;  /* 0 = fastest */
} m1_esp32_rpc_deauth_req_t;

/* BLE_HID_KEY request (key_count HID usage ids follow, max 6). */
typedef struct __attribute__((packed)) {
    uint8_t modifier;      /* ctrl/shift/alt/gui bitfield */
    uint8_t key_count;
} m1_esp32_rpc_hid_key_t;

/* One discovered 802.15.4 device (ZB_SNIFF_GET response element). */
typedef struct __attribute__((packed)) {
    uint8_t  addr_mode;    /* 2 = short (2B), 3 = extended (8B) */
    uint8_t  addr[8];
    uint16_t panid;
    uint8_t  channel;      /* 11-26 */
    int8_t   rssi;
    uint8_t  lqi;
    uint8_t  flags;        /* bit0=beacon bit1=data bit2=cmd */
    uint16_t frames;
    uint8_t  proto;        /* 'Z'=Zigbee 'T'=Thread 0/'U'=unknown */
} m1_esp32_rpc_zb_device_t;

/* SYS_SNTP_SYNC response: UTC time obtained from NTP. */
typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t  month;   /* 1-12 */
    uint8_t  day;     /* 1-31 */
    uint8_t  hour;    /* 0-23 */
    uint8_t  minute;  /* 0-59 */
    uint8_t  second;  /* 0-59 */
    uint8_t  weekday; /* 0=Sun .. 6=Sat */
} m1_esp32_rpc_time_t;

/* OFF_HS_START request: start EAPOL/handshake capture + optional deauth burst. */
typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];       /* target AP BSSID */
    uint8_t  channel;        /* channel to listen on */
    uint16_t deauth_count;   /* deauth frames to inject (0 = passive) */
} m1_esp32_rpc_hs_start_req_t;

/* OFF_STA_SCAN_START request: find associated stations on a target AP. */
typedef struct __attribute__((packed)) {
    uint8_t bssid[6];   /* target AP BSSID */
    uint8_t channel;    /* channel the AP operates on */
    uint8_t dur_s;      /* scan duration in seconds (0 = firmware default) */
} m1_esp32_rpc_sta_scan_req_t;

/* OFF_STA_SCAN_RESULTS response entry (per station). */
typedef struct __attribute__((packed)) {
    uint8_t mac[6];
    int8_t  rssi;
} m1_esp32_rpc_sta_entry_t;

/* BLE_SCAN_RESULTS response entry (fixed prefix; name_len name bytes follow). */
typedef struct __attribute__((packed)) {
    uint8_t addr[6];
    uint8_t addr_type;   /* 0=public 1=random */
    int8_t  rssi;
    uint8_t name_len;
} m1_esp32_rpc_ble_dev_t;

/* =========================================================================
 * Response decoder (pure logic — host-testable, no HAL / RTOS deps)
 * =========================================================================*/

/**
 * Decode an M1_RPC response frame, distinguishing a successful RESP from an
 * error NAK.
 *
 * Validates magic / version / CRC / msg_id and the frame type:
 *   - msg_type RESP -> returns M1_ESP32_RPC_OK, sets @p *payload_out /
 *     @p *payload_len_out to the response payload.
 *   - msg_type NAK  -> returns the status byte carried in payload[0]
 *     (M1_ESP32_RPC_ERR_UNKNOWN if the NAK payload is empty).
 *   - any framing error (bad magic/version/CRC/length, msg_id mismatch, or an
 *     unexpected msg_type) -> returns M1_ESP32_RPC_ERR_BAD_FRAME.
 *
 * This complements m1_esp32_rpc_parse_resp() in m1_esp32_caps.h, which only
 * accepts RESP frames and silently rejects NAKs — the detection probe does not
 * need the error code, but feature callers do.
 *
 * @param buf              Received frame bytes
 * @param buf_len          Valid bytes in @p buf
 * @param expected_msg_id  msg_id the reply must carry
 * @param payload_out      [out] points into @p buf at the payload (may be NULL)
 * @param payload_len_out  [out] payload byte count (may be NULL)
 * @return status (see above)
 */
static inline m1_esp32_rpc_status_t
m1_esp32_rpc_decode_resp(const uint8_t *buf, uint16_t buf_len,
                         uint16_t expected_msg_id,
                         const uint8_t **payload_out,
                         uint16_t *payload_len_out)
{
    if (payload_out)     *payload_out = NULL;
    if (payload_len_out) *payload_len_out = 0u;

    if (!buf ||
        buf_len < (uint16_t)(M1_ESP32_RPC_HDR_SIZE + M1_ESP32_RPC_CRC_SIZE))
        return M1_ESP32_RPC_ERR_BAD_FRAME;

    uint16_t magic = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8u);
    if (magic != M1_ESP32_RPC_MAGIC)          return M1_ESP32_RPC_ERR_BAD_FRAME;
    if (buf[2] != M1_ESP32_RPC_VERSION)       return M1_ESP32_RPC_ERR_BAD_FRAME;

    uint8_t  msg_type = buf[3];
    uint16_t msg_id   = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8u);
    uint16_t plen     = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8u);
    uint16_t total    = (uint16_t)(M1_ESP32_RPC_HDR_SIZE + plen +
                                   M1_ESP32_RPC_CRC_SIZE);
    if (total > buf_len)                      return M1_ESP32_RPC_ERR_BAD_FRAME;

    uint16_t expected_crc = m1_esp32_rpc_crc16(buf, M1_ESP32_RPC_HDR_SIZE + plen);
    uint16_t wire_crc =
        (uint16_t)buf[M1_ESP32_RPC_HDR_SIZE + plen] |
        ((uint16_t)buf[M1_ESP32_RPC_HDR_SIZE + plen + 1u] << 8u);
    if (wire_crc != expected_crc)             return M1_ESP32_RPC_ERR_BAD_FRAME;

    if (msg_id != expected_msg_id)            return M1_ESP32_RPC_ERR_BAD_FRAME;

    if (msg_type == M1_ESP32_RPC_NAK) {
        /* NAK payload = [status:1]; empty payload -> generic unknown error. */
        if (plen >= 1u)
            return (m1_esp32_rpc_status_t)buf[M1_ESP32_RPC_HDR_SIZE];
        return M1_ESP32_RPC_ERR_UNKNOWN;
    }
    if (msg_type != M1_ESP32_RPC_RESP)        return M1_ESP32_RPC_ERR_BAD_FRAME;

    if (payload_out)     *payload_out     = buf + M1_ESP32_RPC_HDR_SIZE;
    if (payload_len_out) *payload_len_out = plen;
    return M1_ESP32_RPC_OK;
}

/* =========================================================================
 * Transport injection (for host tests)
 *
 * On-target the client sends frames via spi_m1link_send_recv_bin() (the 512-byte
 * full-duplex "M1 Link" path the native brain CD3 requires — see below).  Host
 * tests install a fake transport to exercise the build / decode path without
 * hardware.
 * =========================================================================*/

/**
 * Transport function signature — matches spi_m1link_send_recv_bin() /
 * spi_AT_send_recv_bin().
 * @return 0 (SUCCESS) on success, non-zero on transport error.
 */
typedef uint8_t (*m1_esp32_rpc_transport_fn)(const uint8_t *tx_buf, int tx_len,
                                             uint8_t *rx_buf, int rx_buf_size,
                                             int *out_len, int timeout_sec);

/**
 * Override the transport used by m1_esp32_rpc_call().  Pass NULL to restore the
 * default (spi_m1link_send_recv_bin).  Intended for host tests only.
 */
void m1_esp32_rpc_set_transport(m1_esp32_rpc_transport_fn fn);

/* =========================================================================
 * M1 Link full-duplex transport (native brain CD3)
 *
 * The native brain CD3 firmware (hapaxx11/m1-esp32-brain) is an ESP-IDF
 * `spi_slave` (NOT `spi_slave_hd`) device: every SPI transaction clocks EXACTLY
 * M1_ESP32_M1LINK_MTU bytes in BOTH directions at once (master frame on MOSI,
 * slave frame on MISO), with one m1_rpc frame at the head of each fixed buffer
 * and zero padding after it.  This is fundamentally different from the ESP-AT
 * `spi_slave_hd` command/address/dummy protocol that spi_AT_send_recv_bin()
 * drives — sending AT-style HD command frames to a full-duplex slave yields no
 * reply, which is why every brain feature (and capability detection) failed.
 *
 * The slave pipelines its reply: it consumes the request on transaction N and
 * presents the response on a LATER transaction (it prepares the outgoing buffer
 * from the previous loop iteration).  The host must therefore issue follow-up
 * transactions — sending IDLE filler frames — and scan each received frame for
 * the matching response.
 * =========================================================================*/

/** M1 Link fixed transaction size in bytes (matches brain M1L_MTU). */
#define M1_ESP32_M1LINK_MTU        512u

/** Default/minimum number of follow-up (poll) transactions issued after the
 *  request while waiting for the pipelined response before giving up.
 *
 *  The pure helper uses this as a transaction count only (host tests inject an
 *  instantaneous fake exchange). On-target timing is set by
 *  spi_m1link_send_recv_bin(), which scales max_polls from timeout_sec and
 *  applies this macro as a floor. */
#define M1_ESP32_M1LINK_MAX_POLLS  8

/**
 * Single fixed-size full-duplex exchange primitive.
 *
 * Clocks exactly @p mtu bytes out of @p tx and into @p rx in one SPI
 * transaction (honouring CS + HANDSHAKE on-target).  @p ctx carries
 * transport-specific state (on-target: unused; host tests: a fake frame queue).
 *
 * @return 0 on success, non-zero on transport error / timeout.
 */
typedef int (*m1_esp32_m1link_xfer_fn)(const uint8_t *tx, uint8_t *rx,
                                       uint16_t mtu, void *ctx);

/**
 * Pure-logic M1 Link request/response over a caller-supplied exchange primitive.
 *
 * Sends the already-built request frame @p tx_buf (length @p tx_len, <= @p mtu)
 * on the first exchange, then issues up to @p max_polls follow-up IDLE
 * transactions, scanning every received frame for a RESP/NAK whose msg_id
 * matches the request.  Frames that are IDLE, EVENT, or carry a different
 * msg_id are skipped; FRAG frames sharing the request msg_id are reassembled.
 *
 * On success the complete response frame (header + payload + CRC) is written to
 * @p rx_buf and @p *out_len is set to its length; the returned frame is
 * suitable input for m1_esp32_rpc_decode_resp().
 *
 * This is host-testable: inject a fake @p xfer that returns canned slave frames.
 *
 * @param xfer         Single-transaction full-duplex exchange primitive
 * @param ctx          Opaque context passed through to @p xfer
 * @param scratch_tx   Caller-provided @p mtu-byte TX working buffer
 * @param scratch_rx   Caller-provided @p mtu-byte RX working buffer
 * @param mtu          Transaction size (M1_ESP32_M1LINK_MTU on-target)
 * @param max_polls    Follow-up transactions to issue (>= 1; budgets the wait)
 * @param tx_buf       Request frame bytes (built by m1_esp32_rpc_build_req)
 * @param tx_len       Request frame length (> 0, <= @p mtu)
 * @param rx_buf       Output buffer for the matched response frame
 * @param rx_buf_size  Capacity of @p rx_buf
 * @param out_len      [out] response frame bytes written (0 on failure)
 * @return 0 on success; non-zero on invalid args, transport error, or no match
 */
uint8_t m1_esp32_m1link_send_recv(m1_esp32_m1link_xfer_fn xfer, void *ctx,
                                  uint8_t *scratch_tx, uint8_t *scratch_rx,
                                  uint16_t mtu, int max_polls,
                                  const uint8_t *tx_buf, int tx_len,
                                  uint8_t *rx_buf, int rx_buf_size,
                                  int *out_len);

/* =========================================================================
 * Feature-call diagnostics (Phase 2, issue #719)
 *
 * Phase 0/1 instrumented the CD3 detection probe (m1_esp32_caps.h's
 * m1_esp32_caps_diag_t) and fixed the SPI3 contention that was zeroing the
 * M1_RPC PING.  With that fixed, the dashboard now reports a real brain
 * (e.g. "m1-native 1.5.0") with a non-zero, WIFI_SCAN-capable bitmap — yet
 * WiFi Scan itself still fails with "AP scan failed. Please try again."
 * That failure is downstream of a DIFFERENT call: every feature (WIFI_SCAN
 * included) goes through m1_esp32_rpc_call(), not the tiny single-frame PING/
 * GET_STATUS probe, so the probe succeeding tells us nothing about whether a
 * bulk-list feature call (which alone exercises the M1 Link FRAG reassembly
 * path in m1_esp32_m1link_send_recv()) is transporting, framing, and
 * decoding correctly.
 *
 * This is the same "diagnose before fixing blind" approach as Phase 0: record
 * a snapshot of the last m1_esp32_rpc_call() outcome (which opcode, whether
 * the transport ever replied, how many raw frame bytes came back, the final
 * status, and how many payload bytes were decoded) and surface it on the
 * dashboard, so the next report can say e.g. "op0103 no-reply st253 r0 p0" or
 * "op0103 ok st0 r512 p24" instead of just "still fails" — pinpointing
 * whether the fault is transport (no reply / reassembly overflow), framing
 * (bad CRC/msg_id), an explicit ESP32 NAK, or the firmware genuinely
 * returning an empty list.
 *
 * Pure logic, no HAL deps: the struct is populated by m1_esp32_rpc_call()
 * (already host-testable via the injectable transport) and the formatter is
 * a static inline so it is covered by host-side unit tests without linking
 * the .c file.
 * ========================================================================= */

/** Snapshot of the last m1_esp32_rpc_call() invocation. */
typedef struct
{
    uint16_t msg_id;    /**< M1_ESP32_RPC_* opcode of the last call             */
    uint8_t  attempted; /**< a call has been made since boot                    */
    uint8_t  status;    /**< m1_esp32_rpc_status_t result returned to the caller*/
    int16_t  rx_len;    /**< raw transport frame bytes received (<=0 == none)   */
    uint16_t resp_len;  /**< decoded response payload bytes copied to the caller*/
} m1_esp32_rpc_call_diag_t;

/**
 * Render a compact, human-readable one-line summary of a call snapshot,
 * suitable for a 128px display line.  Pure logic — no HAL deps.
 *
 * Examples:
 *   "no call yet"                    (no m1_esp32_rpc_call() issued yet)
 *   "op0103 ok st0 r512 p24"         (WIFI_SCAN succeeded, 24 payload bytes)
 *   "op0103 no-reply st253 r0 p0"    (transport never got a matching reply)
 *   "op0103 bad-frame st254 r18 p0"  (a reply arrived but failed to decode)
 *   "op0103 nak st10 r18 p0"         (ESP32 explicitly rejected, e.g. err 10)
 *
 * @param d        Snapshot to render (may be NULL — yields "no call yet")
 * @param buf      Destination buffer
 * @param buf_size Capacity of @p buf in bytes (>= 1)
 */
static inline void
m1_esp32_rpc_call_diag_format(const m1_esp32_rpc_call_diag_t *d,
                              char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0u)
        return;
    buf[0] = '\0';
    if (!d || !d->attempted)
    {
        snprintf(buf, buf_size, "no call yet");
        return;
    }

    const char *tag;
    switch ((m1_esp32_rpc_status_t)d->status)
    {
        case M1_ESP32_RPC_OK:            tag = "ok";        break;
        case M1_ESP32_RPC_ERR_TRANSPORT: tag = "no-reply";  break;
        case M1_ESP32_RPC_ERR_BAD_FRAME: tag = "bad-frame"; break;
        default:                         tag = "nak";       break;
    }

    snprintf(buf, buf_size, "op%04X %s st%u r%d p%u",
             (unsigned)d->msg_id, tag, (unsigned)d->status,
             (int)d->rx_len, (unsigned)d->resp_len);
}

/**
 * Copy out a snapshot of the last m1_esp32_rpc_call() invocation without
 * re-issuing any request.  @p out->attempted is 0 if no call has been made
 * since boot (or since the last m1_esp32_rpc_set_transport(), which resets
 * the snapshot along with the transport in host tests).
 */
void m1_esp32_rpc_get_call_diag(m1_esp32_rpc_call_diag_t *out);

/* =========================================================================
 * Public client API
 * =========================================================================*/

/**
 * Return the wire transport the currently-detected ESP32 firmware speaks.
 *
 * Thin runtime wrapper over esp32_firmware_transport() applied to the cached
 * capability bitmap (m1_esp32_caps_get_bitmap()).  Returns ESP32_TRANSPORT_NONE
 * before m1_esp32_caps_init() has populated the bitmap.
 */
esp32_transport_t m1_esp32_active_transport(void);

/**
 * Issue a single M1_RPC request and wait for the matching response.
 *
 * Builds a REQ frame for @p msg_id carrying @p req_len bytes of @p req, sends
 * it over the active transport, and decodes the reply.  On a successful RESP
 * up to @p resp_cap payload bytes are copied into @p resp and @p *resp_len is
 * set to the number copied; on a NAK the ESP32's status byte is returned and
 * no payload is copied.
 *
 * The request payload must not exceed M1_ESP32_RPC_PAYLOAD_MAX; the response
 * payload is truncated to @p resp_cap.  For larger transfers use the chunked
 * *_GET opcodes.
 *
 * @param msg_id       M1_ESP32_RPC_* opcode
 * @param req          Request payload (may be NULL when @p req_len == 0)
 * @param req_len      Request payload length in bytes
 * @param resp         Buffer for the response payload (may be NULL)
 * @param resp_cap     Capacity of @p resp in bytes
 * @param resp_len     [out] response payload bytes copied (may be NULL)
 * @param timeout_sec  Response timeout in seconds (0 = transport default)
 * @return M1_ESP32_RPC_OK on success, an ESP32 NAK status, or a host-side
 *         M1_ESP32_RPC_ERR_TRANSPORT / _BAD_FRAME / _INVALID code.
 */
m1_esp32_rpc_status_t m1_esp32_rpc_call(uint16_t msg_id,
                                        const uint8_t *req, uint16_t req_len,
                                        uint8_t *resp, uint16_t resp_cap,
                                        uint16_t *resp_len, int timeout_sec);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESP32_RPC_H_ */
