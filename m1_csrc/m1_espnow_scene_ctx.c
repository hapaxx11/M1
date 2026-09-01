/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_ctx.c
 * @brief  Shared ESP-NOW Peer Link scene context.
 */

#include "m1_espnow_scene_ctx.h"

#include <string.h>

static uint8_t s_peer_mac[ESPNOW_MAC_LEN];
static char s_peer_name[M1_ESPNOW_PEER_NAME_MAX + 1u];
static bool s_peer_valid;
static espnow_share_kind_t s_share_kind;

void m1_espnow_scene_ctx_reset(void)
{
    memset(s_peer_mac, 0, sizeof(s_peer_mac));
    s_peer_name[0] = '\0';
    s_peer_valid = false;
    s_share_kind = ESPNOW_SHARE_KIND_UNKNOWN;
}

void m1_espnow_scene_ctx_set_peer(const uint8_t mac[ESPNOW_MAC_LEN],
                                  const char *name)
{
    if (mac == NULL)
        return;

    memcpy(s_peer_mac, mac, ESPNOW_MAC_LEN);
    if (name != NULL && name[0] != '\0') {
        size_t len = strlen(name);
        if (len > M1_ESPNOW_PEER_NAME_MAX)
            len = M1_ESPNOW_PEER_NAME_MAX;
        memcpy(s_peer_name, name, len);
        s_peer_name[len] = '\0';
    } else {
        strcpy(s_peer_name, "M1");
    }
    s_peer_valid = true;
}

bool m1_espnow_scene_ctx_get_peer(uint8_t mac[ESPNOW_MAC_LEN],
                                  char *name,
                                  uint8_t name_cap)
{
    if (!s_peer_valid)
        return false;
    if (mac != NULL)
        memcpy(mac, s_peer_mac, ESPNOW_MAC_LEN);
    if (name != NULL && name_cap > 0u) {
        size_t len = strlen(s_peer_name);
        if (len >= name_cap)
            len = (size_t)name_cap - 1u;
        memcpy(name, s_peer_name, len);
        name[len] = '\0';
    }
    return true;
}

const char *m1_espnow_scene_ctx_peer_name(void)
{
    return s_peer_valid ? s_peer_name : "";
}

void m1_espnow_scene_ctx_set_share_kind(espnow_share_kind_t kind)
{
    s_share_kind = kind;
}

espnow_share_kind_t m1_espnow_scene_ctx_share_kind(void)
{
    return s_share_kind;
}
