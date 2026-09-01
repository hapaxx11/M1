/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_secure_link.h
 * @brief  Firmware transport wrapper for optional ESP-NOW app encryption.
 */

#ifndef M1_ESPNOW_SECURE_LINK_H_
#define M1_ESPNOW_SECURE_LINK_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "m1_espnow_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void m1_espnow_secure_link_reset(void);

bool m1_espnow_secure_link_configure(
    const uint8_t local_mac[ESPNOW_MAC_LEN],
    const uint8_t peer_mac[ESPNOW_MAC_LEN],
    uint16_t confirm_code);

bool m1_espnow_secure_link_send(const uint8_t peer_mac[ESPNOW_MAC_LEN],
                                const uint8_t *payload, size_t len);

bool m1_espnow_secure_link_recv(uint8_t from_mac[ESPNOW_MAC_LEN],
                                uint8_t *buf, size_t buf_size,
                                uint8_t *out_len);

bool m1_espnow_secure_link_encrypted(void);
bool m1_espnow_secure_link_fallback(void);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESPNOW_SECURE_LINK_H_ */
