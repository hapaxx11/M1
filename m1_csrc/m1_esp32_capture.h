/* See COPYING.txt for license details. */

/*
 * m1_esp32_capture.h
 *
 * MtkCore "Native M1 SPI v1" CAPTURE service (0x0004) — host-side codec +
 * client driver for MonstaShark-equivalent WiFi 802.11 frame capture.
 *
 * Background
 * ----------
 * Monstatek's MtkCore ESP32 firmware exposes a monitor-mode packet capture
 * service ("MonstaShark") over the Native M1 SPI v1 transport.  A session is
 * opened with CAPTURE_START (which mints an operation token), individual raw
 * 802.11 frames are drained one at a time with CAPTURE_POLL_READ, and the
 * session is closed with CAPTURE_STOP.  This is the one ESP32-driven capability
 * present in Monstatek's STM32 firmware that our fork previously lacked a
 * transport for; pairing it with wifi_pcapng.c produces Wireshark-openable
 * PCAPNG files identical in spirit to MonstaShark.
 *
 * This module is the pure-logic / injectable-transport half of that feature:
 *   - request body builders (START / POLL_READ / STOP),
 *   - response parsers (START token, POLL_READ frame record),
 *   - a capability gate over the native GET_CAPABILITIES entries, and
 *   - a thin client driver layered on mtk_native_call() (m1_esp32_native.h)
 *     so the whole flow is driven through the injectable exchange primitive and
 *     is exercised by tests/test_esp32_capture.c with a fake transport.
 *
 * Like the rest of m1_esp32_native, the physical 1024-byte SPI primitive and
 * probe-time activation are a deliberate on-hardware follow-up; everything here
 * is host-testable.
 *
 * Authority (Monstatek/MonstaTek-Esp32-Core @ main, commit 3e21a6a)
 * ----------------------------------------------------------------
 *   CAPTURE opcode numbering:
 *     components/mtek_schema/generated/mtek_opcode_registry.c (service 0x0004)
 *   CAPTURE_START req body (tight LE, no padding):
 *     components/mtek_schema/include/mtek_schema_structs.h (mtk_capture_start_req_t,
 *     mtk_channelplan_t, mtk_capturefilter_t)
 *     components/mtek_schema/generated/mtek_schema_field_tables.c (field order)
 *     components/mtek_schema/mtek_schema_codec.c (tight LE encode rule)
 *   CAPTURE_POLL_READ record variant (20-byte header + frame; EMPTY = 0 bytes):
 *     components/mtek_capture_service/mtek_capture_logic.c
 *       (s_poll_record_fields / capture_record_t / frame_cb: link_type=0
 *        IEEE80211, flags bit0 = truncated)
 *
 * M1 Project
 */

#ifndef M1_ESP32_CAPTURE_H_
#define M1_ESP32_CAPTURE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "m1_esp32_native.h"   /* MTK_SVC_CAPTURE, mtk_native_* client + le helpers */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- CAPTURE service (0x0004) opcodes ---------------------------------- */
#define MTK_CAP_OP_START         UINT16_C(0x0001)
#define MTK_CAP_OP_STOP          UINT16_C(0x0002)
#define MTK_CAP_OP_STATUS        UINT16_C(0x0003)
#define MTK_CAP_OP_SESSION_INFO  UINT16_C(0x0004)
#define MTK_CAP_OP_STATS         UINT16_C(0x0005)
#define MTK_CAP_OP_POLL_READ     UINT16_C(0x0006)

/* ---- CAPTURE_START field encodings ------------------------------------- */
#define MTK_CAP_MODE_PUSH        0u  /**< firmware streams frames (credit-gated) */
#define MTK_CAP_MODE_POLL        1u  /**< host drains frames via POLL_READ       */

#define MTK_CAP_CHANPLAN_FIXED   0u  /**< stay on channel_plan.channel           */
#define MTK_CAP_CHANPLAN_HOP     1u  /**< hop the 2.4 GHz channels               */

#define MTK_CAP_BAND_2GHZ        0u  /**< ESP32-C6 is 2.4 GHz only               */

/* ---- CAPTURE_POLL_READ record fields ----------------------------------- */
#define MTK_CAP_LINKTYPE_IEEE80211  0u    /**< record.link_type: raw 802.11      */
#define MTK_CAP_FLAG_TRUNCATED      0x01u /**< record.flags bit0: snap-truncated */

/* ---- Fixed wire sizes -------------------------------------------------- */
#define MTK_CAP_START_REQ_LEN      18u /**< mode1 snap2 dur4 plan5 filter6      */
#define MTK_CAP_POLL_READ_REQ_LEN   4u /**< operation_token                     */
#define MTK_CAP_STOP_REQ_LEN        5u /**< operation_token + reason            */
#define MTK_CAP_RECORD_HDR_LEN     20u /**< seq4 ts8 lt1 ch1 rssi1 fl1 orig2 cap2*/
#define MTK_CAP_SNAPLEN_MAX      1000u /**< firmware rejects snap_len > 1000     */

/* ---- CAPTURE_START configuration --------------------------------------- */
typedef struct {
    uint8_t  mode;            /**< MTK_CAP_MODE_*                               */
    uint16_t snap_len;        /**< 1..1000 bytes captured per frame             */
    uint32_t duration_ms;     /**< 0 = run until CAPTURE_STOP                    */
    uint8_t  chan_mode;       /**< MTK_CAP_CHANPLAN_*                            */
    uint8_t  channel;         /**< fixed channel when chan_mode == FIXED         */
    uint8_t  band;            /**< MTK_CAP_BAND_2GHZ                             */
    uint16_t hop_dwell_ms;    /**< per-channel dwell when chan_mode == HOP       */
    uint8_t  filter_bssid[6]; /**< all-zero = capture everything                 */
} mtk_capture_cfg_t;

/* ---- Parsed CAPTURE_POLL_READ frame record ----------------------------- */
typedef struct {
    uint32_t sequence;
    uint64_t timestamp_us;
    uint8_t  link_type;       /**< MTK_CAP_LINKTYPE_IEEE80211                   */
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  flags;           /**< MTK_CAP_FLAG_TRUNCATED when snap-truncated    */
    uint16_t original_len;    /**< frame length on air                          */
    uint16_t captured_len;    /**< frame bytes present in this record            */
} mtk_capture_record_t;

/** Result of parsing a CAPTURE_POLL_READ response payload. */
typedef enum {
    MTK_CAP_POLL_EMPTY = 0,   /**< zero-byte body: nothing buffered             */
    MTK_CAP_POLL_RECORD,      /**< a well-formed frame record                   */
    MTK_CAP_POLL_MALFORMED,   /**< truncated / inconsistent body                */
} mtk_capture_poll_result_t;

/* =========================================================================
 * Request body builders — write tight little-endian bodies, return the number
 * of bytes written (a fixed size) or 0 on bad args / insufficient capacity.
 * =========================================================================*/
size_t mtk_capture_build_start_req(uint8_t *out, size_t cap,
                                   const mtk_capture_cfg_t *cfg);
size_t mtk_capture_build_poll_read_req(uint8_t *out, size_t cap, uint32_t token);
size_t mtk_capture_build_stop_req(uint8_t *out, size_t cap,
                                  uint32_t token, uint8_t reason);

/* =========================================================================
 * Response parsers (pure logic).
 * =========================================================================*/

/** Parse a CAPTURE_START response body (operation_token u32). */
bool mtk_capture_parse_start_resp(const uint8_t *payload, size_t len,
                                  uint32_t *token_out);

/**
 * Parse a CAPTURE_POLL_READ response body.
 *
 * @param payload        response payload (NULL/0 len => EMPTY)
 * @param len            payload length
 * @param rec_out        parsed record header (may be NULL)
 * @param frame_out      set to the raw 802.11 frame bytes inside @p payload
 *                       (may be NULL; valid only on MTK_CAP_POLL_RECORD)
 * @param frame_len_out  frame byte count (== rec.captured_len; may be NULL)
 * @return MTK_CAP_POLL_EMPTY / _RECORD / _MALFORMED.
 */
mtk_capture_poll_result_t
mtk_capture_parse_poll_record(const uint8_t *payload, size_t len,
                              mtk_capture_record_t *rec_out,
                              const uint8_t **frame_out,
                              uint16_t *frame_len_out);

/* =========================================================================
 * Capability gate (pure logic over native GET_CAPABILITIES entries).
 * =========================================================================*/

/** True if @p e is CAPTURE_START (service 0x0004 / op 0x0001) and SUPPORTED. */
bool mtk_capture_cap_entry_is_supported(const mtk_native_cap_entry_t *e);

/**
 * Scan one decoded GET_CAPABILITIES page for a SUPPORTED CAPTURE_START entry.
 * @return true if the page advertises CAPTURE_START as supported.
 */
bool mtk_capture_caps_page_supported(const uint8_t *cap_page_payload,
                                     uint16_t len);

/* =========================================================================
 * PCAPNG glue — map a parsed record + its frame bytes into an Enhanced Packet
 * Block (radiotap-encapsulated).  Defined in m1_esp32_capture.c on top of
 * wifi_pcapng.c so capture-specific field mapping stays out of the generic
 * encoder.
 * =========================================================================*/
size_t mtk_capture_record_to_epb(uint8_t *out, size_t cap,
                                 const mtk_capture_record_t *rec,
                                 const uint8_t *frame);

/* =========================================================================
 * Client driver (m1_esp32_capture.c) — layered on mtk_native_call().
 *
 * Each call performs one native request/response exchange through the supplied
 * single-cell exchange primitive (on-target: the 1024-byte native SPI link;
 * host tests: a fake canned-cell queue), so the whole capture flow is
 * host-testable without hardware.
 * =========================================================================*/

/**
 * Open a capture session (CAPTURE_START).  The firmware answers ACCEPTED with
 * the minted operation token, which is returned in @p token_out.
 *
 * @return MTK_NATIVE_OK on OK/ACCEPTED with a valid token; otherwise an error.
 */
mtk_native_result_t
mtk_capture_start(mtk_native_xfer_fn xfer, void *ctx,
                  const mtk_capture_cfg_t *cfg, uint32_t *token_out,
                  uint16_t *status_out, uint32_t host_boot_epoch, int max_polls);

/**
 * Drain at most one buffered frame (CAPTURE_POLL_READ).
 *
 * On success @p have_frame_out indicates whether a frame was returned; when
 * true the raw 802.11 bytes are copied into @p frame_buf and @p rec_out holds
 * the record metadata.  An empty response (nothing buffered) is success with
 * @p have_frame_out == false.
 *
 * @return MTK_NATIVE_OK on a well-formed (possibly empty) response.
 */
mtk_native_result_t
mtk_capture_poll(mtk_native_xfer_fn xfer, void *ctx, uint32_t token,
                 mtk_capture_record_t *rec_out,
                 uint8_t *frame_buf, size_t frame_cap, uint16_t *frame_len_out,
                 bool *have_frame_out, uint16_t *status_out,
                 uint32_t host_boot_epoch, int max_polls);

/**
 * Close a capture session (CAPTURE_STOP).
 * @return MTK_NATIVE_OK on device status OK; otherwise an error.
 */
mtk_native_result_t
mtk_capture_stop(mtk_native_xfer_fn xfer, void *ctx, uint32_t token,
                 uint8_t reason, uint16_t *status_out,
                 uint32_t host_boot_epoch, int max_polls);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESP32_CAPTURE_H_ */
