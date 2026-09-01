/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_hal.c
 * @brief  ESP-NOW HAL glue — CD3 M1_RPC transport implementation.
 *
 * Translates ESP-NOW scene API calls into M1_RPC_NOW_* messages
 * (0x0600..0x0605) sent to the ESP32-C6 via SPI.
 *
 * Transport dispatch:
 *   - CD3 (binary M1_RPC): Implemented here
 *   - CD3-AT: Future — AT+M1ESPNOW=... (stub for now)
 *   - SiN360: Not supported (cap bit never set)
 *
 * M1 Project
 */

#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_espnow_hal.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_caps.h"
#include "m1_esp32_cmd.h"
#include "m1_esp32_rpc.h"
#include "espnow_peer_session.h"
#include "espnow_rpc_parse.h"
#include "ff.h"

/* esp_app_main.h declares get_esp32_main_init_status() / esp32_main_init().
 * The CD3 M1_RPC transport rides the same half-duplex SPI-HD command protocol
 * as the AT firmware, driven by the AT RTOS task, so that task must be running
 * before any ESP-NOW frame can be exchanged. */
#include "esp_app_main.h"

/*==========================================================================*/
/* SPI transaction buffer size (64 bytes for SPI-HD half-duplex)             */
/*==========================================================================*/

#define SPI_BUF_SIZE  64

/*==========================================================================*/
/* Bulk-response reception budgets                                           */
/*==========================================================================*/

/* PEERS_GET response: count(1) + up to ESPNOW_MAX_PEERS x
 * [mac(6)+rssi(1)+namelen(1)+name(up to ESPNOW_NAME_MAX)]. The old
 * SPI_BUF_SIZE (64-byte) reception ceiling silently truncated any peer list
 * beyond ~2 peers, desyncing rssi/namelen/name parsing for the remainder --
 * the same class of defect as the WiFi/BLE scan buffer overflow fixed for
 * m1_esp32_rpc_features.c. Size the reception buffer to the protocol's
 * documented worst case instead. */
#define ESPNOW_PEERS_RESP_MAX \
    (1u + (uint16_t)ESPNOW_MAX_PEERS * \
     (ESPNOW_MAC_LEN + 1u + 1u + ESPNOW_NAME_MAX))

/* RECV_GET response: count(1) + mac(6) + len(2) + data (up to ENL_MSG_MAX,
 * documented as 240 bytes below).  249 bytes already exceeds the old
 * 64-byte SPI_BUF_SIZE ceiling, so any message longer than ~55 bytes was
 * silently truncated before parsing ever saw it. */
#define ESPNOW_RECV_RESP_MAX  (1u + ESPNOW_MAC_LEN + 2u + 240u)

/*==========================================================================*/
/* SPI transport timeout for CD3 M1_RPC ESP-NOW commands                     */
/*==========================================================================*/

/* Response timeout in SECONDS for the shared M1_RPC client.  ESP-NOW
 * operations (announce, peer list, send) complete promptly on the ESP32; 2 s
 * is generous and matches the CD3 detection probe timeout in m1_esp32_caps.c. */
#define ESPNOW_RPC_TIMEOUT_S  2

/*==========================================================================*/
/* Module state                                                             */
/*==========================================================================*/

static uint8_t s_our_mac[ESPNOW_MAC_LEN];
static uint8_t s_channel = 1;
static bool    s_started;

/*==========================================================================*/
/* Internal helper — thin wrapper over the shared M1_RPC client              */
/*==========================================================================*/

/**
 * Send a simple M1_RPC request, return true if the ESP32 answered OK.
 *
 * Delegates framing, half-duplex SPI-HD transport and response/NAK decoding to
 * the shared m1_esp32_rpc_call() client (m1_esp32_rpc.c).  ESP-NOW is the first
 * consumer of that reusable layer; the M1_ESP32_RPC_NOW_* opcodes it uses are
 * defined once in m1_esp32_rpc.h.
 *
 * @param resp_cap  Capacity of resp_buf in bytes.  Must match the caller's
 *                  actual buffer size -- previously this was hardcoded to
 *                  SPI_BUF_SIZE (64) regardless of the buffer passed in,
 *                  silently truncating any PEERS_GET/RECV_GET response
 *                  larger than 64 bytes.
 */
static bool espnow_rpc_cmd(uint16_t msg_id, const uint8_t *payload,
                            uint8_t payload_len, uint8_t *resp_buf,
                            uint16_t resp_cap, uint16_t *resp_len)
{
    uint16_t rlen = 0u;
    m1_esp32_rpc_status_t st =
        m1_esp32_rpc_call(msg_id, payload, payload_len,
                          resp_buf, resp_cap, &rlen, ESPNOW_RPC_TIMEOUT_S);
    if (resp_len)
        *resp_len = rlen;
    return st == M1_ESP32_RPC_OK;
}

/*==========================================================================*/
/* Public API                                                               */
/*==========================================================================*/

bool m1_espnow_start(uint8_t channel)
{
    /* Bring up the SPI HAL and the AT/SPI-HD RTOS task that carries the CD3
     * M1_RPC frames.  DELEGATE_FEATURE only guarantees the SPI HAL is up (via
     * m1_esp32_ensure_init()); the transport task is otherwise started deep
     * inside individual features, so start it here — mirroring the CD3
     * detection probe in m1_esp32_caps_init(). */
    m1_esp32_ensure_init();
    if (!get_esp32_main_init_status())
        esp32_main_init();

    s_channel = channel;
    uint8_t payload[24];  /* ch(1) + name string */
    payload[0] = channel;
    const char *name = "M1";
    size_t nlen = strlen(name);
    if (nlen > ESPNOW_NAME_MAX) nlen = ESPNOW_NAME_MAX;
    memcpy(payload + 1, name, nlen);

    uint8_t  resp[SPI_BUF_SIZE];
    uint16_t rlen = 0;
    if (!espnow_rpc_cmd(M1_ESP32_RPC_NOW_START, payload, (uint8_t)(1 + nlen),
                        resp, sizeof(resp), &rlen)) {
        return false;
    }

    /* Response: status(1) + mac(6) */
    if (rlen >= 7 && resp[0] == 0x00) {
        memcpy(s_our_mac, resp + 1, ESPNOW_MAC_LEN);
        s_started = true;
        return true;
    }
    return false;
}

bool m1_espnow_stop(void)
{
    if (!s_started) return true;
    uint8_t  resp[SPI_BUF_SIZE];
    uint16_t rlen = 0;
    bool ok = espnow_rpc_cmd(M1_ESP32_RPC_NOW_STOP, NULL, 0, resp,
                             sizeof(resp), &rlen);
    s_started = false;
    return ok;
}

bool m1_espnow_announce(void)
{
    uint8_t  resp[SPI_BUF_SIZE];
    uint16_t rlen = 0;
    return espnow_rpc_cmd(M1_ESP32_RPC_NOW_ANNOUNCE, NULL, 0, resp,
                          sizeof(resp), &rlen);
}

uint8_t m1_espnow_poll_peers(void *peers_out, uint8_t max_peers)
{
    uint8_t  resp[ESPNOW_PEERS_RESP_MAX];
    uint16_t rlen = 0;
    if (!espnow_rpc_cmd(M1_ESP32_RPC_NOW_PEERS_GET, NULL, 0, resp,
                        sizeof(resp), &rlen))
        return 0;

    return espnow_rpc_parse_peers(resp, rlen,
                                  (espnow_peer_info_t *)peers_out,
                                  max_peers, s_channel);
}

bool m1_espnow_send(const uint8_t mac[6], const uint8_t *data, size_t len)
{
    /* Payload buffer: SPI_BUF_SIZE - 16 bytes for RPC framing overhead.
     * With 6 MAC prefix bytes, the maximum ESP-NOW data payload is
     * sizeof(payload) - 6 bytes (limited by the 64-byte SPI transaction).
     * ENL_MSG_MAX (240) is an ESP-NOW protocol limit but cannot be reached
     * in a single SPI call without RPC multi-transaction chunking. */
    uint8_t payload[SPI_BUF_SIZE - 16];
    const size_t max_data = sizeof(payload) - 6;
    if (max_data != M1_ESPNOW_SEND_PAYLOAD_MAX)
        return false;
    if (len > max_data)
        return false;
    if (len == 0) return false;

    memcpy(payload, mac, 6);
    memcpy(payload + 6, data, len);

    uint8_t  resp[SPI_BUF_SIZE];
    uint16_t rlen = 0;
    return espnow_rpc_cmd(M1_ESP32_RPC_NOW_SEND, payload, (uint8_t)(6 + len),
                          resp, sizeof(resp), &rlen);
}

bool m1_espnow_recv_msg(uint8_t from_mac[6], uint8_t *buf,
                         size_t buf_size, uint8_t *out_len)
{
    uint8_t  resp[ESPNOW_RECV_RESP_MAX];
    uint16_t rlen = 0;
    if (!espnow_rpc_cmd(M1_ESP32_RPC_NOW_RECV_GET, NULL, 0, resp,
                        sizeof(resp), &rlen))
        return false;

    return espnow_rpc_parse_recv(resp, rlen, from_mac, buf, buf_size,
                                 out_len);
}

void m1_espnow_get_mac(uint8_t mac[6])
{
    memcpy(mac, s_our_mac, ESPNOW_MAC_LEN);
}

uint8_t m1_espnow_get_channel(void)
{
    return s_channel;
}

/*==========================================================================*/
/* File operations (FatFS adapter)                                          */
/*==========================================================================*/

/* Only one ESP-NOW file transfer (send or receive) is ever active at a
 * time, so a single static FIL is shared across all open/read/write/close
 * helpers below rather than each caller paying for its own 500+ byte
 * FatFS file object. */
static FIL s_file;

void *m1_espnow_file_open(const char *path)
{
    FRESULT fr = f_open(&s_file, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return NULL;
    return &s_file;
}

bool m1_espnow_file_write(void *handle, const uint8_t *data, size_t len)
{
    if (!handle) return false;
    UINT bw;
    FRESULT fr = f_write((FIL *)handle, data, (UINT)len, &bw);
    return (fr == FR_OK && bw == (UINT)len);
}

void *m1_espnow_file_open_read(const char *path)
{
    FRESULT fr = f_open(&s_file, path, FA_READ | FA_OPEN_EXISTING);
    if (fr != FR_OK) return NULL;
    return &s_file;
}

bool m1_espnow_file_read(void *handle, uint8_t *data, size_t len,
                         size_t *out_len)
{
    if (!handle) return false;
    UINT br = 0;
    FRESULT fr = f_read((FIL *)handle, data, (UINT)len, &br);
    if (out_len) *out_len = br;
    return fr == FR_OK;
}

bool m1_espnow_file_seek(void *handle, uint32_t offset)
{
    if (!handle) return false;
    return f_lseek((FIL *)handle, offset) == FR_OK;
}

void m1_espnow_file_close(void *handle)
{
    if (handle)
        f_close((FIL *)handle);
}
