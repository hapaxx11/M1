/* See COPYING.txt for license details. */

/**
 * @file   espnow_rpc_parse.h
 * @brief  Pure-logic decoders for M1_RPC_NOW_PEERS_GET / NOW_RECV_GET
 *         response payloads.
 *
 * Extracted from m1_espnow_hal.c so the byte-parsing logic can run on the
 * host without the FreeRTOS/HAL includes that file requires.  No HAL, RTOS,
 * or display dependencies.  Host-testable.
 *
 * M1 Project
 */

#ifndef ESPNOW_RPC_PARSE_H_
#define ESPNOW_RPC_PARSE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "espnow_peer_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Decode a M1_RPC_NOW_PEERS_GET response body.
 *
 * Wire format: count(1) + [mac(6)+rssi(1)+namelen(1)+name(<=namelen)] x N.
 *
 * @param  resp       Response payload bytes (as delivered by the transport;
 *                     caller is responsible for sizing its reception buffer
 *                     to cover the protocol's worst case -- see
 *                     ESPNOW_PEERS_RESP_MAX in m1_espnow_hal.c).
 * @param  rlen        Number of valid bytes in resp.
 * @param  peers_out   Output array of espnow_peer_info_t, at least max_peers
 *                     entries.
 * @param  max_peers   Capacity of peers_out.
 * @param  channel     Channel value to stamp into every returned peer entry.
 * @return Number of peers decoded (<= max_peers).
 */
uint8_t espnow_rpc_parse_peers(const uint8_t *resp, uint16_t rlen,
                               espnow_peer_info_t *peers_out,
                               uint8_t max_peers, uint8_t channel);

/**
 * @brief  Decode a M1_RPC_NOW_RECV_GET response body (first message only).
 *
 * Wire format: count(1) + [mac(6)+len(2 LE)+data] x N -- only the first
 * message is decoded.
 *
 * @param  resp       Response payload bytes.
 * @param  rlen       Number of valid bytes in resp.
 * @param  from_mac   Output: sender's 6-byte MAC.
 * @param  buf        Output buffer for message payload.
 * @param  buf_size   Capacity of buf.
 * @param  out_len    Output: actual message length copied into buf.
 * @return true if a message was decoded, false if resp was empty/malformed.
 */
bool espnow_rpc_parse_recv(const uint8_t *resp, uint16_t rlen,
                           uint8_t from_mac[6], uint8_t *buf,
                           size_t buf_size, uint8_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_RPC_PARSE_H_ */
