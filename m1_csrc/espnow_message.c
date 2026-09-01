/* See COPYING.txt for license details. */

/**
 * @file   espnow_message.c
 * @brief  ESP-NOW short-text peer messaging — pure logic.
 *
 * See espnow_message.h.
 *
 * M1 Project
 */

#include "espnow_message.h"

#include <string.h>

/* =========================================================================
 * Framing
 * =========================================================================*/

bool espnow_msg_build(uint8_t seq, const char *text,
                      uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (text == NULL || out == NULL || out_len == NULL)
        return false;

    size_t tlen = strlen(text);
    if (tlen == 0u || tlen > ESPNOW_MSG_TEXT_MAX)
        return false;
    if (ESPNOW_MSG_HDR_LEN + tlen > out_cap)
        return false;

    out[0] = ESPNOW_MSG_TYPE_TEXT;
    out[1] = seq;
    memcpy(&out[ESPNOW_MSG_HDR_LEN], text, tlen);

    *out_len = ESPNOW_MSG_HDR_LEN + tlen;
    return true;
}

bool espnow_msg_parse(const uint8_t *frame, size_t len,
                      uint8_t *out_seq,
                      char *text_out, size_t text_cap, size_t *text_len)
{
    if (frame == NULL || text_out == NULL || text_cap == 0u)
        return false;
    if (len < ESPNOW_MSG_HDR_LEN)
        return false;
    if (frame[0] != ESPNOW_MSG_TYPE_TEXT)
        return false;

    size_t tlen = len - ESPNOW_MSG_HDR_LEN;
    if (tlen == 0u || tlen > ESPNOW_MSG_TEXT_MAX)
        return false;
    if (tlen + 1u > text_cap)               /* need room for NUL */
        return false;

    /* Reject embedded NULs — text must be a clean string. */
    for (size_t i = 0; i < tlen; ++i) {
        if (frame[ESPNOW_MSG_HDR_LEN + i] == 0u)
            return false;
    }

    memcpy(text_out, &frame[ESPNOW_MSG_HDR_LEN], tlen);
    text_out[tlen] = '\0';

    if (out_seq != NULL)
        *out_seq = frame[1];
    if (text_len != NULL)
        *text_len = tlen;
    return true;
}

/* =========================================================================
 * Inbox ring
 * =========================================================================*/

void espnow_inbox_init(espnow_inbox_t *ib)
{
    if (ib == NULL)
        return;
    memset(ib, 0, sizeof(*ib));
}

bool espnow_inbox_push(espnow_inbox_t *ib, const uint8_t mac[ESPNOW_MSG_MAC_LEN],
                       uint8_t seq, bool outgoing, const char *text)
{
    if (ib == NULL || mac == NULL || text == NULL)
        return false;

    /* Index of the slot to write: append after the last valid entry, wrapping
     * and advancing head once the ring is full. */
    uint8_t slot;
    if (ib->count < ESPNOW_INBOX_CAP) {
        slot = (uint8_t)((ib->head + ib->count) % ESPNOW_INBOX_CAP);
        ib->count++;
    } else {
        slot = ib->head;
        ib->head = (uint8_t)((ib->head + 1u) % ESPNOW_INBOX_CAP);
    }

    espnow_msg_entry_t *e = &ib->items[slot];
    memcpy(e->mac, mac, ESPNOW_MSG_MAC_LEN);
    e->seq = seq;
    e->outgoing = outgoing;

    size_t tlen = strlen(text);
    if (tlen > ESPNOW_MSG_TEXT_MAX)
        tlen = ESPNOW_MSG_TEXT_MAX;
    memcpy(e->text, text, tlen);
    e->text[tlen] = '\0';

    ib->total++;
    return true;
}

const espnow_msg_entry_t *espnow_inbox_get(const espnow_inbox_t *ib, uint8_t idx)
{
    if (ib == NULL || idx >= ib->count)
        return NULL;
    uint8_t slot = (uint8_t)((ib->head + idx) % ESPNOW_INBOX_CAP);
    return &ib->items[slot];
}

bool espnow_inbox_is_duplicate(const espnow_inbox_t *ib,
                               const uint8_t mac[ESPNOW_MSG_MAC_LEN], uint8_t seq)
{
    if (ib == NULL || mac == NULL || ib->count == 0u)
        return false;

    /* Scan from newest to oldest for the most recent *received* entry from this
     * peer; a match on seq is a duplicate. */
    for (uint8_t i = ib->count; i > 0u; --i) {
        const espnow_msg_entry_t *e = espnow_inbox_get(ib, (uint8_t)(i - 1u));
        if (e->outgoing)
            continue;
        if (memcmp(e->mac, mac, ESPNOW_MSG_MAC_LEN) == 0)
            return e->seq == seq;
    }
    return false;
}
