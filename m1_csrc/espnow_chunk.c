/* See COPYING.txt for license details. */

/**
 * @file   espnow_chunk.c
 * @brief  ESP-NOW payload fragmentation / reassembly — pure logic.
 *
 * See espnow_chunk.h for the wire format and rationale.
 *
 * M1 Project
 */

#include "espnow_chunk.h"

#include <string.h>

/* =========================================================================
 * Fragment-count helper
 * =========================================================================*/

uint8_t espnow_chunk_frag_count(size_t len)
{
    if (len == 0u || len > ESPNOW_CHUNK_MSG_MAX)
        return 0u;
    return (uint8_t)((len + ESPNOW_CHUNK_DATA_MAX - 1u) / ESPNOW_CHUNK_DATA_MAX);
}

/* =========================================================================
 * Splitter
 * =========================================================================*/

bool espnow_chunk_split_init(espnow_chunk_splitter_t *s,
                             uint8_t msg_id,
                             const uint8_t *data, size_t len)
{
    if (s == NULL || data == NULL)
        return false;

    uint8_t cnt = espnow_chunk_frag_count(len);
    if (cnt == 0u)
        return false;

    s->data     = data;
    s->len      = len;
    s->msg_id   = msg_id;
    s->frag_cnt = cnt;
    s->next_idx = 0u;
    return true;
}

bool espnow_chunk_split_next(espnow_chunk_splitter_t *s,
                             uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (s == NULL || out == NULL || out_len == NULL)
        return false;
    if (s->next_idx >= s->frag_cnt)
        return false;
    if (out_cap < ESPNOW_CHUNK_FRAME_MAX)
        return false;

    size_t offset = (size_t)s->next_idx * ESPNOW_CHUNK_DATA_MAX;
    size_t remain = s->len - offset;
    size_t take   = (remain > ESPNOW_CHUNK_DATA_MAX)
                        ? ESPNOW_CHUNK_DATA_MAX : remain;

    out[0] = ESPNOW_APP_FRAG;
    out[1] = s->msg_id;
    out[2] = s->next_idx;
    out[3] = s->frag_cnt;
    memcpy(&out[ESPNOW_CHUNK_HDR_LEN], &s->data[offset], take);

    *out_len = ESPNOW_CHUNK_HDR_LEN + take;
    s->next_idx++;
    return true;
}

/* =========================================================================
 * Reassembler
 * =========================================================================*/

void espnow_chunk_reasm_init(espnow_chunk_reasm_t *r)
{
    if (r == NULL)
        return;
    memset(r, 0, sizeof(*r));
}

/** Begin (or restart) reassembly for a new message id / fragment count. */
static void reasm_begin(espnow_chunk_reasm_t *r, uint8_t msg_id, uint8_t cnt)
{
    r->active        = true;
    r->msg_id        = msg_id;
    r->frag_cnt      = cnt;
    r->recv_mask_cnt = 0u;
    r->msg_len       = 0u;
    memset(r->got, 0, sizeof(r->got));
    memset(r->frag_len, 0, sizeof(r->frag_len));
}

espnow_chunk_status_t espnow_chunk_reasm_feed(espnow_chunk_reasm_t *r,
                                              const uint8_t *frame, size_t len)
{
    if (r == NULL || frame == NULL)
        return ESPNOW_CHUNK_ERROR;

    /* Not a fragment frame — leave for other handlers. */
    if (len < ESPNOW_CHUNK_HDR_LEN || frame[0] != ESPNOW_APP_FRAG)
        return ESPNOW_CHUNK_IGNORED;

    uint8_t msg_id = frame[1];
    uint8_t idx    = frame[2];
    uint8_t cnt    = frame[3];
    size_t  dlen   = len - ESPNOW_CHUNK_HDR_LEN;

    /* Validate framing against protocol limits. */
    if (cnt == 0u || cnt > ESPNOW_CHUNK_MAX_FRAGS || idx >= cnt ||
        dlen > ESPNOW_CHUNK_DATA_MAX)
        return ESPNOW_CHUNK_ERROR;

    /* A non-final fragment must be full-size; only the last may be short. */
    if (idx != (uint8_t)(cnt - 1u) && dlen != ESPNOW_CHUNK_DATA_MAX)
        return ESPNOW_CHUNK_ERROR;

    /* Start a fresh message, or restart if the id/count changed mid-stream. */
    if (!r->active || r->msg_id != msg_id || r->frag_cnt != cnt)
        reasm_begin(r, msg_id, cnt);

    size_t dst = (size_t)idx * ESPNOW_CHUNK_DATA_MAX;
    if (dst + dlen > ESPNOW_CHUNK_MSG_MAX)
        return ESPNOW_CHUNK_ERROR;

    if (!r->got[idx]) {
        r->got[idx]      = true;
        r->frag_len[idx] = (uint16_t)dlen;
        r->recv_mask_cnt++;
    } else if (r->frag_len[idx] != (uint16_t)dlen) {
        /* Inconsistent duplicate — treat as corruption. */
        return ESPNOW_CHUNK_ERROR;
    } else if (memcmp(&r->msg[dst], &frame[ESPNOW_CHUNK_HDR_LEN], dlen) != 0) {
        /* A duplicate must be byte-for-byte identical to the first copy. */
        return ESPNOW_CHUNK_ERROR;
    }
    memcpy(&r->msg[dst], &frame[ESPNOW_CHUNK_HDR_LEN], dlen);

    if (r->recv_mask_cnt < r->frag_cnt)
        return ESPNOW_CHUNK_NEED_MORE;

    /* All fragments present — compute total length from per-index lengths. */
    uint16_t total = 0u;
    for (uint8_t i = 0u; i < r->frag_cnt; ++i)
        total = (uint16_t)(total + r->frag_len[i]);

    r->msg_len = total;
    r->active  = false;   /* Ready for the next message. */
    return ESPNOW_CHUNK_COMPLETE;
}
