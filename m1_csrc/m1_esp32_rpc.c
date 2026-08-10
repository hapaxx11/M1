/* See COPYING.txt for license details. */

/**
 * @file   m1_esp32_rpc.c
 * @brief  M1_RPC compatibility layer — reusable CD3 RPC client (host side).
 *
 * Generalises the ESP-NOW-only M1_RPC transport (m1_espnow_hal.c) into a
 * single request/response client that every ESP32-dependent feature can call
 * when the attached firmware is the CD3 native build (hapaxx11/m1-esp32-brain).
 *
 * The native brain CD3 firmware is an ESP-IDF `spi_slave` (full-duplex) device:
 * every SPI transaction clocks EXACTLY M1_ESP32_M1LINK_MTU (512) bytes in both
 * directions, with one m1_rpc frame at the head of the buffer.  The slave
 * pipelines its reply onto a LATER transaction, so the host must issue follow-up
 * IDLE transactions and scan for the matching response — see
 * m1_esp32_m1link_send_recv() below.  This is NOT the ESP-AT half-duplex
 * `spi_slave_hd` command protocol driven by spi_AT_send_recv_bin(), and it is
 * NOT the fixed 64-byte full-duplex transfer used by the SiN360 binary protocol.
 *
 * M1 Project
 */

#include <stdlib.h>
#include <string.h>

#include "m1_esp32_rpc.h"

/* Binary-safe half-duplex SPI-HD send/receive (Esp_spi_at master driver).
 * Declared extern exactly as in m1_esp32_caps.c / m1_espnow_hal.c.
 * Retained for the AT presence/AT+CMD? probes; NOT used for M1_RPC. */
extern uint8_t spi_AT_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                    uint8_t *rx_buf, int rx_buf_size,
                                    int *out_len, int timeout_sec);

/* Full-duplex "M1 Link" master transport for the native brain CD3 firmware
 * (Esp_spi_at master driver).  Clocks fixed 512-byte transactions on hspi_esp
 * with manual CS/HANDSHAKE handling and reassembles the pipelined reply.  This
 * is the transport the M1_RPC client must use to reach the brain. */
extern uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                        uint8_t *rx_buf, int rx_buf_size,
                                        int *out_len, int timeout_sec);

/*==========================================================================*/
/* Transport selection                                                      */
/*==========================================================================*/

/* Active transport function pointer.  Defaults to the full-duplex M1 Link path
 * (the brain CD3 is a full-duplex spi_slave, not an ESP-AT spi_slave_hd device);
 * host tests swap in a fake via m1_esp32_rpc_set_transport(). */
static m1_esp32_rpc_transport_fn s_transport = spi_m1link_send_recv_bin;

void m1_esp32_rpc_set_transport(m1_esp32_rpc_transport_fn fn)
{
    s_transport = fn ? fn : spi_m1link_send_recv_bin;
}

esp32_transport_t m1_esp32_active_transport(void)
{
    return esp32_firmware_transport(m1_esp32_caps_get_bitmap());
}

/*==========================================================================*/
/* RPC client                                                               */
/*==========================================================================*/

m1_esp32_rpc_status_t m1_esp32_rpc_call(uint16_t msg_id,
                                        const uint8_t *req, uint16_t req_len,
                                        uint8_t *resp, uint16_t resp_cap,
                                        uint16_t *resp_len, int timeout_sec)
{
    uint8_t  tx[M1_ESP32_RPC_FRAME_MAX];
    uint8_t *rx;
    int      rx_len = 0;

    if (resp_len) *resp_len = 0u;

    if (req_len > M1_ESP32_RPC_PAYLOAD_MAX || (req_len > 0u && !req))
        return M1_ESP32_RPC_ERR_INVALID;

    uint16_t frame_sz = m1_esp32_rpc_build_req(tx, (uint16_t)sizeof(tx),
                                               msg_id, req, req_len);
    if (frame_sz == 0u)
        return M1_ESP32_RPC_ERR_INVALID;

    /* Heap-allocated: the reception budget (M1_ESP32_RPC_RESP_FRAME_MAX) must
     * cover the firmware's largest bulk-list response (WIFI_SCAN/STA_SCAN/
     * BLE_SCAN_RESULTS), which is far bigger than a stack-friendly control
     * frame -- see the comment on M1_ESP32_RPC_RESP_FRAME_MAX. */
    rx = (uint8_t *)malloc(M1_ESP32_RPC_RESP_FRAME_MAX);
    if (!rx)
        return M1_ESP32_RPC_ERR_NO_MEM;

    memset(rx, 0, M1_ESP32_RPC_RESP_FRAME_MAX);
    if (s_transport(tx, (int)frame_sz, rx, (int)M1_ESP32_RPC_RESP_FRAME_MAX,
                    &rx_len, timeout_sec) != 0 || rx_len <= 0) {
        free(rx);
        return M1_ESP32_RPC_ERR_TRANSPORT;
    }

    const uint8_t *payload = NULL;
    uint16_t       payload_len = 0u;
    m1_esp32_rpc_status_t st = m1_esp32_rpc_decode_resp(rx, (uint16_t)rx_len,
                                                        msg_id, &payload,
                                                        &payload_len);
    if (st != M1_ESP32_RPC_OK) {
        free(rx);
        return st;
    }

    if (resp && resp_cap > 0u && payload_len > 0u) {
        uint16_t copy_len = (payload_len < resp_cap) ? payload_len : resp_cap;
        memcpy(resp, payload, copy_len);
        if (resp_len) *resp_len = copy_len;
    }
    free(rx);
    return M1_ESP32_RPC_OK;
}

/*==========================================================================*/
/* M1 Link full-duplex framing/pipelining (pure logic, host-testable)       */
/*==========================================================================*/

/* Validate a complete frame whose header starts at @p f, with @p avail bytes
 * available from @p f to the end of the enclosing buffer.  On success returns 0
 * and fills the out params; returns non-zero if the frame is malformed (bad
 * magic/version, length overruns @p avail, or CRC mismatch). */
static int m1link_parse_frame_at(const uint8_t *f, size_t avail,
                                 uint8_t *msg_type, uint16_t *msg_id,
                                 uint16_t *plen, const uint8_t **payload)
{
    if (avail < (size_t)(M1_ESP32_RPC_HDR_SIZE + M1_ESP32_RPC_CRC_SIZE))
        return -1;

    uint16_t magic = (uint16_t)f[0] | ((uint16_t)f[1] << 8u);
    if (magic != M1_ESP32_RPC_MAGIC)  return -1;
    if (f[2] != M1_ESP32_RPC_VERSION) return -1;

    uint16_t p = (uint16_t)f[6] | ((uint16_t)f[7] << 8u);
    size_t crc_off = (size_t)M1_ESP32_RPC_HDR_SIZE + (size_t)p;
    size_t total = crc_off + (size_t)M1_ESP32_RPC_CRC_SIZE;
    if (total > avail) return -1;

    uint16_t expected_crc = m1_esp32_rpc_crc16(f, (uint16_t)crc_off);
    uint16_t wire_crc =
        (uint16_t)f[crc_off] |
        ((uint16_t)f[crc_off + 1u] << 8u);
    if (wire_crc != expected_crc) return -1;

    if (msg_type) *msg_type = f[3];
    if (msg_id)   *msg_id   = (uint16_t)f[4] | ((uint16_t)f[5] << 8u);
    if (plen)     *plen     = p;
    if (payload)  *payload  = f + M1_ESP32_RPC_HDR_SIZE;
    return 0;
}

/* Locate and validate a frame anywhere within @p buf.  The full-duplex brain
 * reply normally starts at offset 0, but a byte of residue left in the SPI FIFO
 * by a prior half-duplex (AT / SiN360) transfer shifts the whole frame a few
 * bytes into the received buffer.  Because every transaction over-clocks the
 * MTU with zero padding, a shifted frame is still delivered intact — just at a
 * non-zero offset — so we scan for the RPC magic across the buffer instead of
 * trusting offset 0.  On success returns 0 and sets the out params (payload
 * points at the located frame's payload); returns non-zero if no valid frame is
 * found.  Candidate offsets that fail version/length/CRC checks are skipped so a
 * stray 0x31 0x4D pair inside padding or a payload cannot cause a false match. */
static int m1link_parse_frame(const uint8_t *buf, uint16_t buf_len,
                              uint8_t *msg_type, uint16_t *msg_id,
                              uint16_t *plen, const uint8_t **payload)
{
    if (!buf ||
        buf_len < (uint16_t)(M1_ESP32_RPC_HDR_SIZE + M1_ESP32_RPC_CRC_SIZE))
        return -1;

    size_t last = (size_t)buf_len -
                  (size_t)(M1_ESP32_RPC_HDR_SIZE + M1_ESP32_RPC_CRC_SIZE);
    for (size_t off = 0u; off <= last; off++) {
        /* Cheap magic prefilter before the full validation. */
        if (buf[off] != (uint8_t)(M1_ESP32_RPC_MAGIC & 0xFFu) ||
            buf[off + 1u] != (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu))
            continue;
        if (m1link_parse_frame_at(buf + off, (size_t)buf_len - off,
                                  msg_type, msg_id, plen, payload) == 0)
            return 0;
    }
    return -1;
}

/* Write a bare header-only frame (no payload) of the given type/id into @p buf,
 * appending the CRC16.  Used to emit IDLE filler frames. */
static void m1link_build_header_frame(uint8_t *buf, uint8_t msg_type,
                                      uint16_t msg_id)
{
    buf[0] = (uint8_t)(M1_ESP32_RPC_MAGIC        & 0xFFu);
    buf[1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
    buf[2] = M1_ESP32_RPC_VERSION;
    buf[3] = msg_type;
    buf[4] = (uint8_t)(msg_id        & 0xFFu);
    buf[5] = (uint8_t)((msg_id >> 8u) & 0xFFu);
    buf[6] = 0u;
    buf[7] = 0u;
    uint16_t crc = m1_esp32_rpc_crc16(buf, M1_ESP32_RPC_HDR_SIZE);
    buf[M1_ESP32_RPC_HDR_SIZE]      = (uint8_t)(crc        & 0xFFu);
    buf[M1_ESP32_RPC_HDR_SIZE + 1u] = (uint8_t)((crc >> 8u) & 0xFFu);
}

uint8_t m1_esp32_m1link_send_recv(m1_esp32_m1link_xfer_fn xfer, void *ctx,
                                  uint8_t *scratch_tx, uint8_t *scratch_rx,
                                  uint16_t mtu, int max_polls,
                                  const uint8_t *tx_buf, int tx_len,
                                  uint8_t *rx_buf, int rx_buf_size,
                                  int *out_len)
{
    if (out_len) *out_len = 0;

    if (!xfer || !scratch_tx || !scratch_rx || !tx_buf || !rx_buf ||
        mtu < (uint16_t)(M1_ESP32_RPC_HDR_SIZE + M1_ESP32_RPC_CRC_SIZE) ||
        max_polls < 1 || tx_len < (int)M1_ESP32_RPC_HDR_SIZE ||
        tx_len > (int)mtu || rx_buf_size < (int)M1_ESP32_RPC_HDR_SIZE)
        return 1u; /* invalid argument */

    /* The response echoes the request's msg_id (header bytes 4/5). */
    uint16_t expected_id = (uint16_t)tx_buf[4] | ((uint16_t)tx_buf[5] << 8u);

    uint16_t reasm_len    = 0u; /* payload bytes accumulated from FRAG chain */
    uint8_t  reasm_active = 0u;

    /* Transaction 0 carries the request; later ones carry IDLE filler.  The
     * reply is pipelined onto a later transaction, so we must poll. */
    const int total_txns = max_polls + 1;
    for (int i = 0; i < total_txns; i++) {
        memset(scratch_tx, 0, mtu);
        if (i == 0)
            memcpy(scratch_tx, tx_buf, (size_t)tx_len);
        else
            m1link_build_header_frame(scratch_tx, M1_ESP32_RPC_IDLE, 0u);

        if (xfer(scratch_tx, scratch_rx, mtu, ctx) != 0)
            return 2u; /* transport error */

        uint8_t        rtype = 0u;
        uint16_t       rid = 0u, rplen = 0u;
        const uint8_t *rpayload = NULL;
        if (m1link_parse_frame(scratch_rx, mtu, &rtype, &rid, &rplen,
                               &rpayload) != 0)
            continue; /* garbage/padding — poll again */

        if (rtype == M1_ESP32_RPC_IDLE) continue;   /* slave has nothing yet */
        if (rid != expected_id)         continue;   /* EVENT / stale / other */

        if (rtype == M1_ESP32_RPC_FRAG) {
            /* Accumulate payload after the reserved header slot. */
            if ((int)(M1_ESP32_RPC_HDR_SIZE + reasm_len + rplen +
                      M1_ESP32_RPC_CRC_SIZE) > rx_buf_size)
                return 3u; /* reassembly overflow */
            memcpy(rx_buf + M1_ESP32_RPC_HDR_SIZE + reasm_len, rpayload, rplen);
            reasm_len = (uint16_t)(reasm_len + rplen);
            reasm_active = 1u;
            continue;
        }

        if (rtype == M1_ESP32_RPC_RESP || rtype == M1_ESP32_RPC_NAK) {
            if (reasm_active) {
                /* Append the terminating payload and rebuild a single frame. */
                if ((int)(M1_ESP32_RPC_HDR_SIZE + reasm_len + rplen +
                          M1_ESP32_RPC_CRC_SIZE) > rx_buf_size)
                    return 3u;
                memcpy(rx_buf + M1_ESP32_RPC_HDR_SIZE + reasm_len, rpayload,
                       rplen);
                uint16_t total_plen = (uint16_t)(reasm_len + rplen);
                rx_buf[0] = (uint8_t)(M1_ESP32_RPC_MAGIC        & 0xFFu);
                rx_buf[1] = (uint8_t)((M1_ESP32_RPC_MAGIC >> 8u) & 0xFFu);
                rx_buf[2] = M1_ESP32_RPC_VERSION;
                rx_buf[3] = rtype;
                rx_buf[4] = (uint8_t)(expected_id        & 0xFFu);
                rx_buf[5] = (uint8_t)((expected_id >> 8u) & 0xFFu);
                rx_buf[6] = (uint8_t)(total_plen        & 0xFFu);
                rx_buf[7] = (uint8_t)((total_plen >> 8u) & 0xFFu);
                uint16_t crc = m1_esp32_rpc_crc16(
                    rx_buf, M1_ESP32_RPC_HDR_SIZE + total_plen);
                rx_buf[M1_ESP32_RPC_HDR_SIZE + total_plen] =
                    (uint8_t)(crc & 0xFFu);
                rx_buf[M1_ESP32_RPC_HDR_SIZE + total_plen + 1u] =
                    (uint8_t)((crc >> 8u) & 0xFFu);
                if (out_len)
                    *out_len = (int)(M1_ESP32_RPC_HDR_SIZE + total_plen +
                                     M1_ESP32_RPC_CRC_SIZE);
            } else {
                /* Single-frame response: copy it out verbatim from the located
                 * frame base (which may be a few bytes into scratch_rx when a
                 * shifted frame was recovered), not the start of scratch_rx. */
                const uint8_t *base = rpayload - M1_ESP32_RPC_HDR_SIZE;
                int frame = (int)(M1_ESP32_RPC_HDR_SIZE + rplen +
                                  M1_ESP32_RPC_CRC_SIZE);
                if (frame > rx_buf_size) return 3u;
                memcpy(rx_buf, base, (size_t)frame);
                if (out_len) *out_len = frame;
            }
            return 0u; /* matched */
        }
        /* Any other msg_type sharing our id: ignore and keep polling. */
    }

    return 4u; /* no matching response within the poll budget */
}
