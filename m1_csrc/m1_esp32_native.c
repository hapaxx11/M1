/* See COPYING.txt for license details. */

/**
 * @file   m1_esp32_native.c
 * @brief  MtkCore "Native M1 SPI v1" transport — runtime client (host side).
 *
 * Drives the native 1024-byte-cell transport over an injectable single-cell
 * exchange primitive (mtk_native_xfer_fn): the HELLO handshake, a single-cell
 * request builder, an IDLE poll loop that correlates the RESPONSE by
 * request_id, and multi-cell response reassembly via the pure-logic helpers in
 * m1_esp32_native.h.
 *
 * Relationship to the other ESP32 transports
 * ------------------------------------------
 * MtkCore ships both a "Legacy SPI Compatibility" adapter (our existing m1_link
 * binary RPC, magic 0x4D31 — detected in m1_esp32_caps.c and driven by
 * m1_esp32_rpc.c) and this canonical native transport (magic "M1S1", 1024-byte
 * cells).  Tier A lit up the compat subset over the legacy adapter; this module
 * is the Tier-B foundation for the full native feature set (ESP-NOW, 802.15.4,
 * and the paginated GET_CAPABILITIES negotiation that the legacy adapter cannot
 * expose).
 *
 * Deliberate scope boundary (why the live SPI primitive is not wired here)
 * -----------------------------------------------------------------------
 * The firmware's own contract package discloses several wire choices as NOT
 * formally specified:
 *   - the HELLO negotiation payload (HELLO_ACK is an empty acknowledgement),
 *   - the physical 512->1024 cell-size negotiation handshake (lives in the
 *     unread main/mtek_spi_runtime.c), and
 *   - the CREDIT/CANCEL payloads beyond a 4-byte LE credit amount.
 * None of these can be validated without real MtkCore hardware, so this module
 * stops at the host-verifiable protocol/codec layer and an injectable transport.
 * The on-target 1024-byte SPI primitive and probe-time activation are a
 * follow-up step, consistent with the repo rule against asserting unverified
 * hardware behaviour.  s_transport therefore defaults to NULL (every call is
 * driven through the caller-supplied @p xfer argument, as the host tests do).
 *
 * M1 Project
 */

#include <stdlib.h>
#include <string.h>

#include "m1_esp32_native.h"

/*==========================================================================*/
/* Request-id sequencing                                                    */
/*==========================================================================*/

/* Monotonic request-id counter.  request_id == 0 is reserved/avoided so a
 * zero-initialised reassembler or a stale cell cannot accidentally correlate. */
static uint32_t s_request_id = 0u;

uint32_t mtk_native_next_request_id(void)
{
    s_request_id++;
    if (s_request_id == 0u)
        s_request_id = 1u;
    return s_request_id;
}

void mtk_native_reset_request_id(uint32_t seed)
{
    s_request_id = seed;
}

/*==========================================================================*/
/* Internal helpers                                                         */
/*==========================================================================*/

/* Build and clock one request/handshake cell, returning the parsed reply cell
 * in @p rx_cell (always MTK_SPI_NATIVE_CELL_SIZE bytes).  Returns 0 on a
 * successful exchange (rx_cell populated), non-zero on transport error. */
static int native_exchange(mtk_native_xfer_fn xfer, void *ctx,
                           const uint8_t *tx_cell, uint8_t *rx_cell)
{
    memset(rx_cell, 0, MTK_SPI_NATIVE_CELL_SIZE);
    return xfer(tx_cell, rx_cell, (uint16_t)MTK_SPI_NATIVE_CELL_SIZE, ctx);
}

/* Build an IDLE filler cell (no payload) into @p cell. */
static void native_build_idle(uint8_t *cell)
{
    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic     = MTK_SPI_NATIVE_MAGIC;
    h.major     = (uint8_t)MTK_SPI_NATIVE_MAJOR;
    h.minor     = (uint8_t)MTK_SPI_NATIVE_MINOR;
    h.msg_class = (uint8_t)MTK_SPI_CLASS_IDLE;
    h.flags     = (uint8_t)(MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST);
    (void)mtk_native_build_cell(cell, MTK_SPI_NATIVE_CELL_SIZE, &h, NULL, 0u);
}

/*==========================================================================*/
/* HELLO handshake                                                          */
/*==========================================================================*/

mtk_native_result_t mtk_native_hello(mtk_native_xfer_fn xfer, void *ctx,
                                     uint32_t host_boot_epoch, int max_polls)
{
    if (!xfer || max_polls < 1)
        return MTK_NATIVE_ERR_INVALID;

    uint8_t *tx = (uint8_t *)malloc(MTK_SPI_NATIVE_CELL_SIZE);
    uint8_t *rx = (uint8_t *)malloc(MTK_SPI_NATIVE_CELL_SIZE);
    if (!tx || !rx) {
        free(tx); free(rx);
        return MTK_NATIVE_ERR_NO_MEM;
    }

    const uint32_t rid = mtk_native_next_request_id();

    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic      = MTK_SPI_NATIVE_MAGIC;
    h.major      = (uint8_t)MTK_SPI_NATIVE_MAJOR;
    h.minor      = (uint8_t)MTK_SPI_NATIVE_MINOR;
    h.msg_class  = (uint8_t)MTK_SPI_CLASS_HELLO;
    h.flags      = (uint8_t)(MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST);
    h.request_id = rid;
    h.boot_epoch = host_boot_epoch;
    (void)mtk_native_build_cell(tx, MTK_SPI_NATIVE_CELL_SIZE, &h, NULL, 0u);

    mtk_native_result_t result = MTK_NATIVE_ERR_TIMEOUT;

    /* First exchange carries the HELLO; subsequent ones send IDLE fillers. */
    for (int poll = 0; poll <= max_polls; poll++) {
        if (poll == 1)
            native_build_idle(tx);

        if (native_exchange(xfer, ctx, tx, rx) != 0) {
            result = MTK_NATIVE_ERR_TRANSPORT;
            break;
        }

        mtk_spi_native_header_t rh;
        if (mtk_native_parse_cell(rx, MTK_SPI_NATIVE_CELL_SIZE, &rh,
                                  NULL, NULL) != MTK_PARSE_OK)
            continue;  /* padding / garbage — keep polling */
        if (rh.msg_class == MTK_SPI_CLASS_IDLE)
            continue;  /* slave has nothing yet */
        if (rh.request_id != rid)
            continue;  /* EVENT / stale / other correlation */

        if (rh.msg_class == MTK_SPI_CLASS_HELLO_ACK) {
            result = MTK_NATIVE_OK;
            break;
        }
        if (rh.msg_class == MTK_SPI_CLASS_LINK_ERROR) {
            result = MTK_NATIVE_ERR_PROTOCOL;
            break;
        }
    }

    free(tx);
    free(rx);
    return result;
}

/*==========================================================================*/
/* Request / response                                                       */
/*==========================================================================*/

mtk_native_result_t mtk_native_call(mtk_native_xfer_fn xfer, void *ctx,
                                    uint16_t service, uint16_t opcode,
                                    const uint8_t *req, uint16_t req_len,
                                    uint8_t *resp, size_t resp_cap,
                                    size_t *resp_len, uint16_t *status_out,
                                    uint32_t host_boot_epoch, int max_polls)
{
    if (resp_len)    *resp_len = 0u;
    if (status_out)  *status_out = 0u;

    if (!xfer || max_polls < 1)
        return MTK_NATIVE_ERR_INVALID;
    if (req_len > MTK_SPI_NATIVE_MAX_PAYLOAD || (req_len > 0u && !req))
        return MTK_NATIVE_ERR_INVALID;

    uint8_t *tx    = (uint8_t *)malloc(MTK_SPI_NATIVE_CELL_SIZE);
    uint8_t *rx    = (uint8_t *)malloc(MTK_SPI_NATIVE_CELL_SIZE);
    uint8_t *asm_buf = (uint8_t *)malloc(MTK_SPI_NATIVE_MAX_MESSAGE);
    if (!tx || !rx || !asm_buf) {
        free(tx); free(rx); free(asm_buf);
        return MTK_NATIVE_ERR_NO_MEM;
    }

    const uint32_t rid = mtk_native_next_request_id();

    mtk_spi_native_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic           = MTK_SPI_NATIVE_MAGIC;
    h.major           = (uint8_t)MTK_SPI_NATIVE_MAJOR;
    h.minor           = (uint8_t)MTK_SPI_NATIVE_MINOR;
    h.msg_class       = (uint8_t)MTK_SPI_CLASS_REQUEST;
    h.flags           = (uint8_t)(MTK_SPI_FLAG_FIRST | MTK_SPI_FLAG_LAST);
    h.service         = service;
    h.opcode          = opcode;
    h.status          = 0u;
    h.request_id      = rid;
    h.boot_epoch      = host_boot_epoch;
    h.message_len     = req_len;     /* single-cell request: total == this cell */
    h.fragment_offset = 0u;
    (void)mtk_native_build_cell(tx, MTK_SPI_NATIVE_CELL_SIZE, &h, req, req_len);

    mtk_native_reasm_t reasm;
    mtk_native_reasm_init(&reasm, asm_buf, MTK_SPI_NATIVE_MAX_MESSAGE);

    mtk_native_result_t result = MTK_NATIVE_ERR_TIMEOUT;
    uint16_t resp_status = 0u;
    bool status_latched  = false;

    for (int poll = 0; poll <= max_polls; poll++) {
        if (poll == 1)
            native_build_idle(tx);

        if (native_exchange(xfer, ctx, tx, rx) != 0) {
            result = MTK_NATIVE_ERR_TRANSPORT;
            break;
        }

        mtk_spi_native_header_t rh;
        const uint8_t *rpayload = NULL;
        uint16_t       rplen = 0u;
        if (mtk_native_parse_cell(rx, MTK_SPI_NATIVE_CELL_SIZE, &rh,
                                  &rpayload, &rplen) != MTK_PARSE_OK)
            continue;  /* padding / garbage — keep polling */
        if (rh.msg_class == MTK_SPI_CLASS_IDLE)
            continue;
        if (rh.request_id != rid)
            continue;  /* EVENT / stale / other op */

        if (rh.msg_class == MTK_SPI_CLASS_LINK_ERROR) {
            resp_status = rh.status;
            status_latched = true;
            result = MTK_NATIVE_ERR_PROTOCOL;
            break;
        }
        if (rh.msg_class != MTK_SPI_CLASS_RESPONSE)
            continue;  /* HELLO_ACK/CREDIT/etc. for our id — ignore here */

        if (!status_latched) {
            resp_status = rh.status;   /* status carried on the response header */
            status_latched = true;
        }

        mtk_native_reasm_status_t rs =
            mtk_native_reasm_feed(&reasm, &rh, rpayload);
        if (rs == MTK_REASM_ERROR) {
            result = MTK_NATIVE_ERR_PROTOCOL;
            break;
        }
        if (rs == MTK_REASM_COMPLETE) {
            if (resp_len)
                *resp_len = reasm.received;
            if (resp && resp_cap > 0u && reasm.received > 0u) {
                size_t copy = (reasm.received < resp_cap) ? reasm.received
                                                          : resp_cap;
                memcpy(resp, asm_buf, copy);
            }
            result = (resp_status == MTK_STATUS_OK) ? MTK_NATIVE_OK
                                                    : MTK_NATIVE_ERR_STATUS;
            break;
        }
        /* MTK_REASM_NEED_MORE — keep polling for the next fragment. */
    }

    if (status_out && status_latched)
        *status_out = resp_status;

    free(tx);
    free(rx);
    free(asm_buf);
    return result;
}
