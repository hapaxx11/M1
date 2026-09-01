/* See COPYING.txt for license details. */

/**
 * @file   espnow_secure.h
 * @brief  Optional ESP-NOW secure-session helpers — pure logic.
 *
 * This module contains only key-derivation and negotiation-frame helpers for
 * the optional app-layer encryption used by the Peer Link UI.  Firmware scene
 * code owns transport polling and fallback timing; the decisions here are kept
 * host-testable.
 */

#ifndef ESPNOW_SECURE_H_
#define ESPNOW_SECURE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "espnow_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESPNOW_SECURE_MAC_LEN  6u

typedef enum {
    ESPNOW_SECURE_CTRL_NONE = 0,
    ESPNOW_SECURE_CTRL_HELLO = 0xE1,
    ESPNOW_SECURE_CTRL_ACK   = 0xE2,
    ESPNOW_SECURE_CTRL_NACK  = 0xE3,
} espnow_secure_ctrl_t;

/**
 * @brief  Derive stable pair key material from the two peer MACs and confirm code.
 *
 * MAC addresses are sorted before derivation so both devices derive the same
 * key regardless of initiator/responder role.  The confirm code is included as
 * pairing transcript material; callers still use an explicit HELLO/ACK before
 * sending encrypted payloads so older peers can fall back to plaintext.
 */
bool espnow_secure_derive_pair_key(const uint8_t local_mac[ESPNOW_SECURE_MAC_LEN],
                                   const uint8_t peer_mac[ESPNOW_SECURE_MAC_LEN],
                                   uint16_t confirm_code,
                                   espnow_crypto_key_t *out);

bool espnow_secure_build_control(espnow_secure_ctrl_t type,
                                 uint8_t *out, size_t out_cap,
                                 size_t *out_len);

bool espnow_secure_parse_control(const uint8_t *frame, size_t frame_len,
                                 espnow_secure_ctrl_t *out_type);

bool espnow_secure_should_accept_plaintext(bool fallback_allowed,
                                           bool crypto_enabled);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_SECURE_H_ */
