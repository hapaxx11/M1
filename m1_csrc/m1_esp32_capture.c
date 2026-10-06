/* See COPYING.txt for license details. */

/*
 * m1_esp32_capture.c
 *
 * MtkCore Native M1 SPI v1 CAPTURE service (0x0004) codec + client driver.
 * See m1_esp32_capture.h for the feature overview and the authority citations
 * for every byte offset below.
 *
 * Everything here is pure logic or driven through the injectable
 * mtk_native_xfer_fn exchange primitive, so it is fully host-tested by
 * tests/test_esp32_capture.c (no HAL / SPI / hardware involved).
 *
 * M1 Project
 */

#include "m1_esp32_capture.h"
#include "wifi_pcapng.h"
#include <string.h>

/* =========================================================================
 * Request body builders
 * =========================================================================*/

size_t mtk_capture_build_start_req(uint8_t *out, size_t cap,
                                   const mtk_capture_cfg_t *cfg)
{
    if (!out || !cfg || cap < MTK_CAP_START_REQ_LEN)
        return 0u;

    /* mtk_capture_start_req_t, tight LE (mtek_schema_codec.c):
     *   u8 mode; u16 snap_len; u32 duration_ms;
     *   channel_plan { u8 mode; u8 channel; u8 band; u16 hop_dwell_ms };
     *   filter { u8 filter_bssid[6] } */
    out[0] = cfg->mode;
    mtk_le_store16(out + 1, cfg->snap_len);
    mtk_le_store32(out + 3, cfg->duration_ms);
    out[7] = cfg->chan_mode;
    out[8] = cfg->channel;
    out[9] = cfg->band;
    mtk_le_store16(out + 10, cfg->hop_dwell_ms);
    memcpy(out + 12, cfg->filter_bssid, 6);
    return MTK_CAP_START_REQ_LEN;
}

size_t mtk_capture_build_poll_read_req(uint8_t *out, size_t cap, uint32_t token)
{
    if (!out || cap < MTK_CAP_POLL_READ_REQ_LEN)
        return 0u;
    mtk_le_store32(out + 0, token);
    return MTK_CAP_POLL_READ_REQ_LEN;
}

size_t mtk_capture_build_stop_req(uint8_t *out, size_t cap,
                                  uint32_t token, uint8_t reason)
{
    if (!out || cap < MTK_CAP_STOP_REQ_LEN)
        return 0u;
    mtk_le_store32(out + 0, token);
    out[4] = reason;
    return MTK_CAP_STOP_REQ_LEN;
}

/* =========================================================================
 * Response parsers
 * =========================================================================*/

bool mtk_capture_parse_start_resp(const uint8_t *payload, size_t len,
                                  uint32_t *token_out)
{
    if (!payload || len < 4u)
        return false;
    if (token_out)
        *token_out = mtk_le_load32(payload);
    return true;
}

mtk_capture_poll_result_t
mtk_capture_parse_poll_record(const uint8_t *payload, size_t len,
                              mtk_capture_record_t *rec_out,
                              const uint8_t **frame_out,
                              uint16_t *frame_len_out)
{
    if (frame_out)     *frame_out = NULL;
    if (frame_len_out) *frame_len_out = 0u;

    /* EMPTY variant: zero body bytes means nothing buffered. */
    if (!payload || len == 0u)
        return MTK_CAP_POLL_EMPTY;

    if (len < MTK_CAP_RECORD_HDR_LEN)
        return MTK_CAP_POLL_MALFORMED;

    /* capture_record_t header (s_poll_record_fields), tight LE:
     *   u32 sequence; u64 timestamp_us; u8 link_type; u8 channel; i8 rssi;
     *   u8 flags; u16 original_len; u16 captured_len; [captured_len frame] */
    uint16_t captured_len = mtk_le_load16(payload + 18);
    if ((size_t)MTK_CAP_RECORD_HDR_LEN + (size_t)captured_len > len)
        return MTK_CAP_POLL_MALFORMED;

    if (rec_out) {
        rec_out->sequence     = mtk_le_load32(payload + 0);
        rec_out->timestamp_us = (uint64_t)mtk_le_load32(payload + 4) |
                                ((uint64_t)mtk_le_load32(payload + 8) << 32);
        rec_out->link_type    = payload[12];
        rec_out->channel      = payload[13];
        rec_out->rssi         = (int8_t)payload[14];
        rec_out->flags        = payload[15];
        rec_out->original_len = mtk_le_load16(payload + 16);
        rec_out->captured_len = captured_len;
    }
    if (frame_out)     *frame_out = payload + MTK_CAP_RECORD_HDR_LEN;
    if (frame_len_out) *frame_len_out = captured_len;
    return MTK_CAP_POLL_RECORD;
}

/* =========================================================================
 * Capability gate
 * =========================================================================*/

bool mtk_capture_cap_entry_is_supported(const mtk_native_cap_entry_t *e)
{
    return e &&
           e->service_id == MTK_SVC_CAPTURE &&
           e->opcode     == MTK_CAP_OP_START &&
           e->state      == MTK_CAP_STATE_SUPPORTED;
}

bool mtk_capture_caps_page_supported(const uint8_t *cap_page_payload,
                                     uint16_t len)
{
    uint32_t count = 0u;
    const uint8_t *entries = NULL;
    if (!mtk_native_decode_capabilities_page(cap_page_payload, len,
                                             &count, &entries, NULL))
        return false;

    for (uint32_t i = 0u; i < count; i++) {
        mtk_native_cap_entry_t e;
        const uint8_t *p = entries + (size_t)i * MTK_NATIVE_CAP_ENTRY_SIZE;
        if (!mtk_native_decode_cap_entry(p, MTK_NATIVE_CAP_ENTRY_SIZE, &e))
            return false;
        if (mtk_capture_cap_entry_is_supported(&e))
            return true;
    }
    return false;
}

/* =========================================================================
 * PCAPNG glue
 * =========================================================================*/

size_t mtk_capture_record_to_epb(uint8_t *out, size_t cap,
                                 const mtk_capture_record_t *rec,
                                 const uint8_t *frame)
{
    if (!out || !rec)
        return 0u;
    return wifi_pcapng_build_epb_radiotap(out, cap, frame,
                                          rec->captured_len, rec->original_len,
                                          rec->channel, rec->rssi,
                                          rec->timestamp_us);
}

/* =========================================================================
 * Client driver
 * =========================================================================*/

mtk_native_result_t
mtk_capture_start(mtk_native_xfer_fn xfer, void *ctx,
                  const mtk_capture_cfg_t *cfg, uint32_t *token_out,
                  uint16_t *status_out, uint32_t host_boot_epoch, int max_polls)
{
    if (token_out) *token_out = 0u;
    if (!xfer || !cfg)
        return MTK_NATIVE_ERR_INVALID;

    uint8_t req[MTK_CAP_START_REQ_LEN];
    if (mtk_capture_build_start_req(req, sizeof(req), cfg) != MTK_CAP_START_REQ_LEN)
        return MTK_NATIVE_ERR_INVALID;

    uint8_t  resp[8];
    size_t   resp_len = 0u;
    uint16_t status   = 0u;
    mtk_native_result_t r =
        mtk_native_call(xfer, ctx, MTK_SVC_CAPTURE, MTK_CAP_OP_START,
                        req, sizeof(req), resp, sizeof(resp), &resp_len,
                        &status, host_boot_epoch, max_polls);
    if (status_out) *status_out = status;

    /* CAPTURE_START is ACCEPTED_ASYNC: the firmware replies ACCEPTED (not OK)
     * with the operation token, which mtk_native_call surfaces as ERR_STATUS
     * while still delivering the body.  Treat OK and ACCEPTED alike. */
    if (r != MTK_NATIVE_OK &&
        !(r == MTK_NATIVE_ERR_STATUS && status == MTK_STATUS_ACCEPTED))
        return r;

    uint32_t token = 0u;
    if (!mtk_capture_parse_start_resp(resp, resp_len, &token) || token == 0u)
        return MTK_NATIVE_ERR_PROTOCOL;

    if (token_out) *token_out = token;
    return MTK_NATIVE_OK;
}

mtk_native_result_t
mtk_capture_poll(mtk_native_xfer_fn xfer, void *ctx, uint32_t token,
                 mtk_capture_record_t *rec_out,
                 uint8_t *frame_buf, size_t frame_cap, uint16_t *frame_len_out,
                 bool *have_frame_out, uint16_t *status_out,
                 uint32_t host_boot_epoch, int max_polls)
{
    if (frame_len_out)  *frame_len_out = 0u;
    if (have_frame_out) *have_frame_out = false;
    if (!xfer)
        return MTK_NATIVE_ERR_INVALID;

    uint8_t req[MTK_CAP_POLL_READ_REQ_LEN];
    (void)mtk_capture_build_poll_read_req(req, sizeof(req), token);

    uint8_t  resp[MTK_CAP_RECORD_HDR_LEN + MTK_CAP_SNAPLEN_MAX];
    size_t   resp_len = 0u;
    uint16_t status   = 0u;
    mtk_native_result_t r =
        mtk_native_call(xfer, ctx, MTK_SVC_CAPTURE, MTK_CAP_OP_POLL_READ,
                        req, sizeof(req), resp, sizeof(resp), &resp_len,
                        &status, host_boot_epoch, max_polls);
    if (status_out) *status_out = status;
    if (r != MTK_NATIVE_OK)
        return r;

    mtk_capture_record_t rec;
    const uint8_t *frame = NULL;
    uint16_t frame_len = 0u;
    mtk_capture_poll_result_t pr =
        mtk_capture_parse_poll_record(resp, resp_len, &rec, &frame, &frame_len);

    if (pr == MTK_CAP_POLL_EMPTY)
        return MTK_NATIVE_OK;          /* nothing buffered — not an error */
    if (pr != MTK_CAP_POLL_RECORD)
        return MTK_NATIVE_ERR_PROTOCOL;

    if (rec_out) *rec_out = rec;
    if (frame_buf && frame_cap > 0u && frame_len > 0u) {
        uint16_t copy = (frame_len < frame_cap) ? frame_len
                                                : (uint16_t)frame_cap;
        memcpy(frame_buf, frame, copy);
    }
    if (frame_len_out)  *frame_len_out = frame_len;
    if (have_frame_out) *have_frame_out = true;
    return MTK_NATIVE_OK;
}

mtk_native_result_t
mtk_capture_stop(mtk_native_xfer_fn xfer, void *ctx, uint32_t token,
                 uint8_t reason, uint16_t *status_out,
                 uint32_t host_boot_epoch, int max_polls)
{
    if (!xfer)
        return MTK_NATIVE_ERR_INVALID;

    uint8_t req[MTK_CAP_STOP_REQ_LEN];
    (void)mtk_capture_build_stop_req(req, sizeof(req), token, reason);

    return mtk_native_call(xfer, ctx, MTK_SVC_CAPTURE, MTK_CAP_OP_STOP,
                           req, sizeof(req), NULL, 0u, NULL,
                           status_out, host_boot_epoch, max_polls);
}
