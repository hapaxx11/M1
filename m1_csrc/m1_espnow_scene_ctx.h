/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_ctx.h
 * @brief  Shared ESP-NOW Peer Link scene context.
 */

#ifndef M1_ESPNOW_SCENE_CTX_H_
#define M1_ESPNOW_SCENE_CTX_H_

#include <stdbool.h>
#include <stdint.h>

#include "m1_espnow_hal.h"
#include "espnow_shareable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define M1_ESPNOW_PEER_NAME_MAX  16u

void m1_espnow_scene_ctx_reset(void);

void m1_espnow_scene_ctx_set_peer(const uint8_t mac[ESPNOW_MAC_LEN],
                                  const char *name);

bool m1_espnow_scene_ctx_get_peer(uint8_t mac[ESPNOW_MAC_LEN],
                                  char *name,
                                  uint8_t name_cap);

const char *m1_espnow_scene_ctx_peer_name(void);

/** Pointer to the currently-paired peer MAC, or NULL if unpaired.
 *  Valid only while paired; callers must not cache it across a re-pair. */
const uint8_t *m1_espnow_scene_ctx_peer_mac(void);

void m1_espnow_scene_ctx_set_share_kind(espnow_share_kind_t kind);
espnow_share_kind_t m1_espnow_scene_ctx_share_kind(void);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESPNOW_SCENE_CTX_H_ */
