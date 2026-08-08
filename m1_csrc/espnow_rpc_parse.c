/* See COPYING.txt for license details. */

/**
 * @file   espnow_rpc_parse.c
 * @brief  Pure-logic decoders for M1_RPC_NOW_PEERS_GET / NOW_RECV_GET
 *         response payloads.  See espnow_rpc_parse.h.
 *
 * M1 Project
 */

#include <string.h>

#include "espnow_rpc_parse.h"

uint8_t espnow_rpc_parse_peers(const uint8_t *resp, uint16_t rlen,
                               espnow_peer_info_t *peers_out,
                               uint8_t max_peers, uint8_t channel)
{
    if (!resp || !peers_out || max_peers == 0u || rlen < 1u)
        return 0u;

    uint8_t count = resp[0];
    if (count > max_peers) count = max_peers;

    uint16_t offset = 1u;
    uint8_t  got = 0u;
    for (uint8_t i = 0u; i < count && offset < rlen; i++) {
        if ((uint32_t)offset + 8u > rlen)
            break;  /* mac(6) + rssi(1) + namelen(1) */
        memcpy(peers_out[i].mac, resp + offset, ESPNOW_MAC_LEN);
        offset += ESPNOW_MAC_LEN;
        peers_out[i].rssi = (int8_t)resp[offset++];
        uint8_t namelen = resp[offset++];
        if (namelen > ESPNOW_NAME_MAX) namelen = ESPNOW_NAME_MAX;
        if ((uint32_t)offset + namelen > rlen)
            namelen = (uint8_t)(rlen - offset);
        memcpy(peers_out[i].name, resp + offset, namelen);
        peers_out[i].name[namelen] = '\0';
        offset += namelen;
        peers_out[i].channel = channel;
        got++;
    }
    return got;
}

bool espnow_rpc_parse_recv(const uint8_t *resp, uint16_t rlen,
                           uint8_t from_mac[6], uint8_t *buf,
                           size_t buf_size, uint8_t *out_len)
{
    if (!resp || !from_mac || !buf || !out_len || rlen < 1u || resp[0] == 0u)
        return false;

    uint16_t offset = 1u;
    if ((uint32_t)offset + 8u > rlen)
        return false;  /* mac(6) + len(2) min */
    memcpy(from_mac, resp + offset, 6u);
    offset += 6u;
    uint16_t msg_len = (uint16_t)(resp[offset] | (resp[offset + 1u] << 8u));
    offset += 2u;
    if (msg_len > buf_size) msg_len = (uint16_t)buf_size;
    if ((uint32_t)offset + msg_len > rlen) msg_len = (uint16_t)(rlen - offset);
    memcpy(buf, resp + offset, msg_len);
    *out_len = (uint8_t)msg_len;
    return true;
}
