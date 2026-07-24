/* See COPYING.txt for license details. */

/**
 * @file   espnow_peer_session.c
 * @brief  ESP-NOW peer discovery & pairing state machine — pure logic.
 *
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * M1 Project
 */

#include "espnow_peer_session.h"
#include <string.h>

/* =========================================================================
 * CRC32 (same polynomial as bit_util.c — duplicated here to keep this
 * module self-contained with zero external dependencies)
 * =========================================================================*/

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

/* =========================================================================
 * Public API
 * =========================================================================*/

void espnow_session_init(espnow_session_t *s, const char *name,
                          const uint8_t mac[ESPNOW_MAC_LEN], uint8_t channel)
{
    if (!s)
        return;

    memset(s, 0, sizeof(*s));
    s->state = ESPNOW_STATE_IDLE;
    s->our_channel = channel;

    if (mac)
        memcpy(s->our_mac, mac, ESPNOW_MAC_LEN);

    if (name) {
        size_t len = strlen(name);
        if (len > ESPNOW_NAME_MAX)
            len = ESPNOW_NAME_MAX;
        memcpy(s->our_name, name, len);
        s->our_name[len] = '\0';
    }
}

bool espnow_session_start_scan(espnow_session_t *s)
{
    if (!s || s->state != ESPNOW_STATE_IDLE)
        return false;
    s->state = ESPNOW_STATE_SCANNING;
    s->peer_count = 0;
    return true;
}

void espnow_session_update_peers(espnow_session_t *s,
                                  const espnow_peer_info_t *peers,
                                  uint8_t count)
{
    if (!s || s->state != ESPNOW_STATE_SCANNING)
        return;
    if (!peers || count == 0)
        return;

    for (uint8_t i = 0; i < count; i++) {
        /* Check if this MAC already exists in our table */
        bool found = false;
        for (uint8_t j = 0; j < s->peer_count; j++) {
            if (memcmp(s->peers[j].mac, peers[i].mac, ESPNOW_MAC_LEN) == 0) {
                /* Update RSSI and name */
                s->peers[j].rssi = peers[i].rssi;
                s->peers[j].channel = peers[i].channel;
                memcpy(s->peers[j].name, peers[i].name, ESPNOW_NAME_MAX + 1);
                s->peers[j].name[ESPNOW_NAME_MAX] = '\0';
                found = true;
                break;
            }
        }
        if (!found && s->peer_count < ESPNOW_MAX_PEERS) {
            s->peers[s->peer_count] = peers[i];
            s->peers[s->peer_count].name[ESPNOW_NAME_MAX] = '\0';
            s->peer_count++;
        }
    }
}

bool espnow_session_select_peer(espnow_session_t *s, uint8_t idx)
{
    if (!s || s->state != ESPNOW_STATE_SCANNING)
        return false;
    if (idx >= s->peer_count)
        return false;
    s->selected_peer_idx = idx;
    s->state = ESPNOW_STATE_PEER_FOUND;
    return true;
}

bool espnow_session_pair_request_sent(espnow_session_t *s)
{
    if (!s || s->state != ESPNOW_STATE_PEER_FOUND)
        return false;
    s->state = ESPNOW_STATE_PAIR_SENT;
    return true;
}

bool espnow_session_pair_accepted(espnow_session_t *s)
{
    if (!s || s->state != ESPNOW_STATE_PAIR_SENT)
        return false;
    s->state = ESPNOW_STATE_PAIRED;
    /* Compute the visual confirmation code */
    s->confirm_code = espnow_compute_confirm_code(
        s->our_mac, s->peers[s->selected_peer_idx].mac);
    return true;
}

bool espnow_session_pair_rejected(espnow_session_t *s)
{
    if (!s || s->state != ESPNOW_STATE_PAIR_SENT)
        return false;
    s->state = ESPNOW_STATE_PAIR_REJECTED;
    return true;
}

bool espnow_session_ack_rejection(espnow_session_t *s)
{
    if (!s || s->state != ESPNOW_STATE_PAIR_REJECTED)
        return false;
    s->state = ESPNOW_STATE_SCANNING;
    return true;
}

void espnow_session_stop(espnow_session_t *s)
{
    if (!s)
        return;
    s->state = ESPNOW_STATE_IDLE;
}

uint16_t espnow_compute_confirm_code(const uint8_t mac_a[ESPNOW_MAC_LEN],
                                      const uint8_t mac_b[ESPNOW_MAC_LEN])
{
    uint8_t buf[ESPNOW_MAC_LEN * 2];
    memcpy(buf, mac_a, ESPNOW_MAC_LEN);
    memcpy(buf + ESPNOW_MAC_LEN, mac_b, ESPNOW_MAC_LEN);
    uint32_t crc = crc32_update(0, buf, sizeof(buf));
    return (uint16_t)(crc % 10000u);
}
