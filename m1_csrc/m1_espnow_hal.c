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
#include "espnow_peer_session.h"
#include "ff.h"

/*==========================================================================*/
/* M1_RPC ESP-NOW message IDs (CD3 bedge117/m1-esp32-brain)                 */
/*==========================================================================*/

#define M1_RPC_NOW_START      UINT16_C(0x0600)
#define M1_RPC_NOW_STOP       UINT16_C(0x0601)
#define M1_RPC_NOW_ANNOUNCE   UINT16_C(0x0602)
#define M1_RPC_NOW_PEERS_GET  UINT16_C(0x0603)
#define M1_RPC_NOW_SEND       UINT16_C(0x0604)
#define M1_RPC_NOW_RECV_GET   UINT16_C(0x0605)

/*==========================================================================*/
/* SPI transaction buffer size (64 bytes for SPI-HD half-duplex)             */
/*==========================================================================*/

#define SPI_BUF_SIZE  64

/*==========================================================================*/
/* Module state                                                             */
/*==========================================================================*/

static uint8_t s_our_mac[ESPNOW_MAC_LEN];
static uint8_t s_channel = 1;
static bool    s_started;

/*==========================================================================*/
/* Internal helpers — M1_RPC frame build + send (reuse caps.h inline)        */
/*==========================================================================*/

/**
 * Build and send a simple M1_RPC request, return true if response OK.
 * Uses the 64-byte SPI transaction buffer model.
 */
static bool espnow_rpc_cmd(uint16_t msg_id, const uint8_t *payload,
                            uint8_t payload_len, uint8_t *resp_buf,
                            uint8_t *resp_len)
{
    uint8_t tx[SPI_BUF_SIZE];
    uint8_t rx[SPI_BUF_SIZE];

    memset(tx, 0, sizeof(tx));
    uint16_t frame_sz = m1_esp32_rpc_build_req(tx, SPI_BUF_SIZE, msg_id,
                                                payload, payload_len);
    if (frame_sz == 0) return false;

    int rc = m1_esp32_send_cmd_raw(tx, rx, 200);
    if (rc != 0) return false;

    /* Parse and validate response */
    const uint8_t *resp_payload = NULL;
    uint16_t resp_payload_len = 0;
    if (!m1_esp32_rpc_parse_resp(rx, SPI_BUF_SIZE, msg_id,
                                  &resp_payload, &resp_payload_len))
        return false;

    if (resp_buf && resp_len) {
        uint8_t copy_len = (uint8_t)resp_payload_len;
        if (copy_len > SPI_BUF_SIZE - M1_ESP32_RPC_HDR_SIZE - M1_ESP32_RPC_CRC_SIZE)
            copy_len = SPI_BUF_SIZE - M1_ESP32_RPC_HDR_SIZE - M1_ESP32_RPC_CRC_SIZE;
        memcpy(resp_buf, resp_payload, copy_len);
        *resp_len = copy_len;
    }
    return true;
}

/*==========================================================================*/
/* Public API                                                               */
/*==========================================================================*/

bool m1_espnow_start(uint8_t channel)
{
    s_channel = channel;
    uint8_t payload[24];  /* ch(1) + name string */
    payload[0] = channel;
    const char *name = "M1";
    size_t nlen = strlen(name);
    if (nlen > ESPNOW_NAME_MAX) nlen = ESPNOW_NAME_MAX;
    memcpy(payload + 1, name, nlen);

    uint8_t resp[SPI_BUF_SIZE];
    uint8_t rlen = 0;
    if (!espnow_rpc_cmd(M1_RPC_NOW_START, payload, (uint8_t)(1 + nlen),
                        resp, &rlen)) {
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
    uint8_t resp[SPI_BUF_SIZE];
    uint8_t rlen = 0;
    bool ok = espnow_rpc_cmd(M1_RPC_NOW_STOP, NULL, 0, resp, &rlen);
    s_started = false;
    return ok;
}

bool m1_espnow_announce(void)
{
    uint8_t resp[SPI_BUF_SIZE];
    uint8_t rlen = 0;
    return espnow_rpc_cmd(M1_RPC_NOW_ANNOUNCE, NULL, 0, resp, &rlen);
}

uint8_t m1_espnow_poll_peers(void *peers_out, uint8_t max_peers)
{
    uint8_t resp[SPI_BUF_SIZE];
    uint8_t rlen = 0;
    if (!espnow_rpc_cmd(M1_RPC_NOW_PEERS_GET, NULL, 0, resp, &rlen))
        return 0;

    /* Response: count(1) + [mac(6)+rssi(1)+namelen(1)+name]×N */
    if (rlen < 1) return 0;
    uint8_t count = resp[0];
    if (count > max_peers) count = max_peers;

    espnow_peer_info_t *peers = (espnow_peer_info_t *)peers_out;
    uint8_t offset = 1;
    for (uint8_t i = 0; i < count && offset < rlen; i++) {
        if (offset + 8 > rlen) break;  /* mac(6) + rssi(1) + namelen(1) */
        memcpy(peers[i].mac, resp + offset, ESPNOW_MAC_LEN);
        offset += ESPNOW_MAC_LEN;
        peers[i].rssi = (int8_t)resp[offset++];
        uint8_t namelen = resp[offset++];
        if (namelen > ESPNOW_NAME_MAX) namelen = ESPNOW_NAME_MAX;
        if (offset + namelen > rlen) namelen = rlen - offset;
        memcpy(peers[i].name, resp + offset, namelen);
        peers[i].name[namelen] = '\0';
        offset += namelen;
        peers[i].channel = s_channel;
    }
    return count;
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
    if (len > max_data) len = max_data;
    if (len == 0) return false;

    memcpy(payload, mac, 6);
    memcpy(payload + 6, data, len);

    uint8_t resp[SPI_BUF_SIZE];
    uint8_t rlen = 0;
    return espnow_rpc_cmd(M1_RPC_NOW_SEND, payload, (uint8_t)(6 + len),
                          resp, &rlen);
}

bool m1_espnow_recv_msg(uint8_t from_mac[6], uint8_t *buf,
                         size_t buf_size, uint8_t *out_len)
{
    uint8_t resp[SPI_BUF_SIZE];
    uint8_t rlen = 0;
    if (!espnow_rpc_cmd(M1_RPC_NOW_RECV_GET, NULL, 0, resp, &rlen))
        return false;

    /* Response: count(1) + [mac(6)+len(2 LE)+data]×N — we take first msg */
    if (rlen < 1 || resp[0] == 0) return false;

    uint8_t offset = 1;
    if (offset + 8 > rlen) return false;  /* mac(6) + len(2) min */
    memcpy(from_mac, resp + offset, 6);
    offset += 6;
    uint16_t msg_len = (uint16_t)(resp[offset] | (resp[offset + 1] << 8));
    offset += 2;
    if (msg_len > buf_size) msg_len = (uint16_t)buf_size;
    if (offset + msg_len > rlen) msg_len = rlen - offset;
    memcpy(buf, resp + offset, msg_len);
    *out_len = (uint8_t)msg_len;
    return true;
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

void *m1_espnow_file_open(const char *path)
{
    static FIL s_file;  /* single concurrent transfer supported */
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

void m1_espnow_file_close(void *handle)
{
    if (handle)
        f_close((FIL *)handle);
}
