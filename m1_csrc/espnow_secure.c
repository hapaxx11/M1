/* See COPYING.txt for license details. */

/**
 * @file   espnow_secure.c
 * @brief  Optional ESP-NOW secure-session helpers — pure logic.
 */

#include "espnow_secure.h"

#include <string.h>

#define ESPNOW_SECURE_SECRET_LEN  24u

bool espnow_secure_derive_pair_key(const uint8_t local_mac[ESPNOW_SECURE_MAC_LEN],
                                   const uint8_t peer_mac[ESPNOW_SECURE_MAC_LEN],
                                   uint16_t confirm_code,
                                   espnow_crypto_key_t *out)
{
    uint8_t secret[ESPNOW_SECURE_SECRET_LEN] = {
        'M','1','E','S','P','N','O','W','S','E','C','1'
    };
    const uint8_t *first;
    const uint8_t *second;

    if (local_mac == NULL || peer_mac == NULL || out == NULL)
        return false;

    if (memcmp(local_mac, peer_mac, ESPNOW_SECURE_MAC_LEN) <= 0) {
        first = local_mac;
        second = peer_mac;
    } else {
        first = peer_mac;
        second = local_mac;
    }

    memcpy(&secret[12], first, ESPNOW_SECURE_MAC_LEN);
    memcpy(&secret[18], second, ESPNOW_SECURE_MAC_LEN);
    secret[22] = (uint8_t)(confirm_code >> 8);
    secret[23] = (uint8_t)(confirm_code & 0xFFu);

    return espnow_crypto_derive(secret, sizeof(secret), out) == ESPNOW_CRYPTO_OK;
}

bool espnow_secure_build_control(espnow_secure_ctrl_t type,
                                 uint8_t *out, size_t out_cap,
                                 size_t *out_len)
{
    if (out == NULL || out_len == NULL || out_cap < 2u)
        return false;
    if (type != ESPNOW_SECURE_CTRL_HELLO &&
        type != ESPNOW_SECURE_CTRL_ACK &&
        type != ESPNOW_SECURE_CTRL_NACK)
        return false;

    out[0] = (uint8_t)type;
    out[1] = 1u;  /* negotiation version */
    *out_len = 2u;
    return true;
}

bool espnow_secure_parse_control(const uint8_t *frame, size_t frame_len,
                                 espnow_secure_ctrl_t *out_type)
{
    espnow_secure_ctrl_t type;

    if (frame == NULL || out_type == NULL || frame_len != 2u)
        return false;
    if (frame[1] != 1u)
        return false;

    type = (espnow_secure_ctrl_t)frame[0];
    if (type != ESPNOW_SECURE_CTRL_HELLO &&
        type != ESPNOW_SECURE_CTRL_ACK &&
        type != ESPNOW_SECURE_CTRL_NACK)
        return false;

    *out_type = type;
    return true;
}

bool espnow_secure_should_accept_plaintext(bool fallback_allowed,
                                           bool crypto_enabled)
{
    return fallback_allowed || !crypto_enabled;
}
