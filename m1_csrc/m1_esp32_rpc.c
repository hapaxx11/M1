/* See COPYING.txt for license details. */

/**
 * @file   m1_esp32_rpc.c
 * @brief  M1_RPC compatibility layer — reusable CD3 RPC client (host side).
 *
 * Generalises the ESP-NOW-only M1_RPC transport (m1_espnow_hal.c) into a
 * single request/response client that every ESP32-dependent feature can call
 * when the attached firmware is the CD3 native build (bedge117/m1-esp32-brain).
 *
 * CD3 is an esp `spi_slave_hd` slave, so its M1_RPC frames ride the binary-safe
 * half-duplex SPI-HD path (spi_AT_send_recv_bin) — the same transport used by
 * the AT firmware and by the CD3 detection probe in m1_esp32_caps.c — NOT the
 * full-duplex 64-byte transfer used by the SiN360 binary protocol.
 *
 * M1 Project
 */

#include <string.h>

#include "m1_esp32_rpc.h"

/* Binary-safe half-duplex SPI-HD send/receive (Esp_spi_at master driver).
 * Declared extern exactly as in m1_esp32_caps.c / m1_espnow_hal.c. */
extern uint8_t spi_AT_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                    uint8_t *rx_buf, int rx_buf_size,
                                    int *out_len, int timeout_sec);

/*==========================================================================*/
/* Transport selection                                                      */
/*==========================================================================*/

/* Active transport function pointer.  Defaults to the SPI-HD binary path;
 * host tests swap in a fake via m1_esp32_rpc_set_transport(). */
static m1_esp32_rpc_transport_fn s_transport = spi_AT_send_recv_bin;

void m1_esp32_rpc_set_transport(m1_esp32_rpc_transport_fn fn)
{
    s_transport = fn ? fn : spi_AT_send_recv_bin;
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
    uint8_t tx[M1_ESP32_RPC_FRAME_MAX];
    uint8_t rx[M1_ESP32_RPC_FRAME_MAX];
    int     rx_len = 0;

    if (resp_len) *resp_len = 0u;

    if (req_len > M1_ESP32_RPC_PAYLOAD_MAX || (req_len > 0u && !req))
        return M1_ESP32_RPC_ERR_INVALID;

    uint16_t frame_sz = m1_esp32_rpc_build_req(tx, (uint16_t)sizeof(tx),
                                               msg_id, req, req_len);
    if (frame_sz == 0u)
        return M1_ESP32_RPC_ERR_INVALID;

    memset(rx, 0, sizeof(rx));
    if (s_transport(tx, (int)frame_sz, rx, (int)sizeof(rx),
                    &rx_len, timeout_sec) != 0 || rx_len <= 0)
        return M1_ESP32_RPC_ERR_TRANSPORT;

    const uint8_t *payload = NULL;
    uint16_t       payload_len = 0u;
    m1_esp32_rpc_status_t st = m1_esp32_rpc_decode_resp(rx, (uint16_t)rx_len,
                                                        msg_id, &payload,
                                                        &payload_len);
    if (st != M1_ESP32_RPC_OK)
        return st;

    if (resp && resp_cap > 0u && payload_len > 0u) {
        uint16_t copy_len = (payload_len < resp_cap) ? payload_len : resp_cap;
        memcpy(resp, payload, copy_len);
        if (resp_len) *resp_len = copy_len;
    }
    return M1_ESP32_RPC_OK;
}
