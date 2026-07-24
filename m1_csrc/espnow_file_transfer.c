/* See COPYING.txt for license details. */

/**
 * @file   espnow_file_transfer.c
 * @brief  ESP-NOW file transfer protocol — pure logic, streaming-to-SD.
 *
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * M1 Project
 */

#include "espnow_file_transfer.h"
#include <string.h>

/* =========================================================================
 * CRC32 (EDB88320 polynomial — same as bit_util.c, self-contained here)
 * =========================================================================*/

uint32_t espnow_ft_crc32(uint32_t crc, const uint8_t *data, size_t len)
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
 * Internal helpers
 * =========================================================================*/

/** Build and send a simple message (type + seq, no extra payload). */
static bool send_simple(espnow_ft_ctx_t *ctx, uint8_t type, uint8_t seq)
{
    uint8_t msg[2] = { type, seq };
    return ctx->hal->send(ctx->peer_mac, msg, sizeof(msg), ctx->hal->ctx);
}

/* =========================================================================
 * Sender API
 * =========================================================================*/

void espnow_ft_send_init(espnow_ft_ctx_t *ctx,
                          const espnow_ft_hal_ops_t *hal,
                          const uint8_t peer_mac[ESPNOW_FT_MAC_LEN],
                          const char *filename,
                          uint32_t file_size,
                          uint32_t file_crc32,
                          uint8_t chunk_size)
{
    if (!ctx || !hal)
        return;

    memset(ctx, 0, sizeof(*ctx));
    ctx->state = ESPNOW_FT_STATE_IDLE;
    ctx->hal = hal;
    memcpy(ctx->peer_mac, peer_mac, ESPNOW_FT_MAC_LEN);

    if (filename) {
        size_t len = strlen(filename);
        if (len > ESPNOW_FT_FILENAME_MAX)
            len = ESPNOW_FT_FILENAME_MAX;
        memcpy(ctx->filename, filename, len);
        ctx->filename[len] = '\0';
    }

    ctx->file_size = file_size;
    ctx->expected_crc32 = file_crc32;
    ctx->chunk_size = (chunk_size > ESPNOW_FT_CHUNK_MAX)
                      ? ESPNOW_FT_CHUNK_MAX : chunk_size;
    if (ctx->chunk_size == 0)
        ctx->chunk_size = ESPNOW_FT_CHUNK_MAX;
}

bool espnow_ft_send_offer(espnow_ft_ctx_t *ctx)
{
    if (!ctx || ctx->state != ESPNOW_FT_STATE_IDLE)
        return false;

    /* Build OFFER: type(1) + seq(1) + filename(32) + size(4LE) + crc(4LE) + chunk_size(1) */
    uint8_t msg[2 + ESPNOW_FT_FILENAME_MAX + 4 + 4 + 1];
    memset(msg, 0, sizeof(msg));
    msg[0] = ESPNOW_FT_MSG_OFFER;
    msg[1] = 0; /* seq 0 for offer */
    memcpy(&msg[2], ctx->filename, ESPNOW_FT_FILENAME_MAX);

    size_t off = 2 + ESPNOW_FT_FILENAME_MAX;
    msg[off + 0] = (uint8_t)(ctx->file_size >>  0);
    msg[off + 1] = (uint8_t)(ctx->file_size >>  8);
    msg[off + 2] = (uint8_t)(ctx->file_size >> 16);
    msg[off + 3] = (uint8_t)(ctx->file_size >> 24);
    off += 4;
    msg[off + 0] = (uint8_t)(ctx->expected_crc32 >>  0);
    msg[off + 1] = (uint8_t)(ctx->expected_crc32 >>  8);
    msg[off + 2] = (uint8_t)(ctx->expected_crc32 >> 16);
    msg[off + 3] = (uint8_t)(ctx->expected_crc32 >> 24);
    off += 4;
    msg[off] = ctx->chunk_size;

    bool ok = ctx->hal->send(ctx->peer_mac, msg, sizeof(msg), ctx->hal->ctx);
    if (ok) {
        ctx->state = ESPNOW_FT_STATE_OFFER_SENT;
        ctx->last_send_ms = ctx->hal->millis(ctx->hal->ctx);
    }
    return ok;
}

bool espnow_ft_send_on_recv(espnow_ft_ctx_t *ctx, uint8_t type,
                             const uint8_t *data, size_t len)
{
    if (!ctx)
        return false;
    (void)data;
    (void)len;

    switch (ctx->state) {
    case ESPNOW_FT_STATE_OFFER_SENT:
        if (type == ESPNOW_FT_MSG_ACCEPT) {
            ctx->state = ESPNOW_FT_STATE_SENDING;
            ctx->current_seq = 0;
            ctx->bytes_transferred = 0;
            ctx->retry_count = 0;
            return true;
        } else if (type == ESPNOW_FT_MSG_REJECT) {
            ctx->state = ESPNOW_FT_STATE_FAILED;
            return true;
        }
        break;

    case ESPNOW_FT_STATE_WAIT_ACK:
        if (type == ESPNOW_FT_MSG_ACK) {
            /* ACK received — advance to next chunk or complete */
            ctx->retry_count = 0;
            if (ctx->bytes_transferred >= ctx->file_size) {
                /* All data sent — send COMPLETE */
                send_simple(ctx, ESPNOW_FT_MSG_COMPLETE, ctx->current_seq);
                ctx->state = ESPNOW_FT_STATE_DONE;
            } else {
                ctx->state = ESPNOW_FT_STATE_SENDING;
            }
            return true;
        } else if (type == ESPNOW_FT_MSG_ABORT) {
            ctx->state = ESPNOW_FT_STATE_FAILED;
            return true;
        }
        break;

    default:
        break;
    }
    return false;
}

bool espnow_ft_send_chunk(espnow_ft_ctx_t *ctx, const uint8_t *chunk_data,
                           size_t chunk_len)
{
    if (!ctx || ctx->state != ESPNOW_FT_STATE_SENDING)
        return false;
    if (!chunk_data || chunk_len == 0 || chunk_len > ctx->chunk_size)
        return false;

    /* Build DATA: type(1) + seq(1) + offset(4LE) + data[chunk_len] */
    uint8_t msg[2 + 4 + ESPNOW_FT_CHUNK_MAX];
    msg[0] = ESPNOW_FT_MSG_DATA;
    msg[1] = ctx->current_seq;

    uint32_t offset = ctx->bytes_transferred;
    msg[2] = (uint8_t)(offset >>  0);
    msg[3] = (uint8_t)(offset >>  8);
    msg[4] = (uint8_t)(offset >> 16);
    msg[5] = (uint8_t)(offset >> 24);
    memcpy(&msg[6], chunk_data, chunk_len);

    bool ok = ctx->hal->send(ctx->peer_mac, msg, 6 + chunk_len, ctx->hal->ctx);
    if (ok) {
        ctx->bytes_transferred += (uint32_t)chunk_len;
        ctx->current_seq++;
        ctx->state = ESPNOW_FT_STATE_WAIT_ACK;
        ctx->last_send_ms = ctx->hal->millis(ctx->hal->ctx);
    }
    return ok;
}

bool espnow_ft_send_check_timeout(espnow_ft_ctx_t *ctx)
{
    if (!ctx || ctx->state != ESPNOW_FT_STATE_WAIT_ACK)
        return false;

    uint32_t now = ctx->hal->millis(ctx->hal->ctx);
    uint32_t elapsed = now - ctx->last_send_ms;

    if (elapsed < ESPNOW_FT_ACK_TIMEOUT_MS)
        return false;

    ctx->retry_count++;
    if (ctx->retry_count > ESPNOW_FT_MAX_RETRIES) {
        ctx->state = ESPNOW_FT_STATE_FAILED;
        return true;
    }

    /* Retry: go back to SENDING so caller can resend the chunk.
     * Roll back bytes_transferred and seq for the failed chunk. */
    /* Note: we leave bytes_transferred as-is since the receiver will
     * de-dup by offset.  The sender re-transmits from the same offset. */
    ctx->current_seq--;
    ctx->bytes_transferred -= ctx->chunk_size; /* approximate rollback */
    ctx->state = ESPNOW_FT_STATE_SENDING;
    return true;
}

/* =========================================================================
 * Receiver API
 * =========================================================================*/

void espnow_ft_recv_init(espnow_ft_ctx_t *ctx, const espnow_ft_hal_ops_t *hal)
{
    if (!ctx || !hal)
        return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = ESPNOW_FT_STATE_IDLE;
    ctx->hal = hal;
}

bool espnow_ft_recv_on_msg(espnow_ft_ctx_t *ctx,
                            const uint8_t peer_mac[ESPNOW_FT_MAC_LEN],
                            uint8_t type, uint8_t seq,
                            const uint8_t *data, size_t len)
{
    if (!ctx)
        return false;

    switch (type) {
    case ESPNOW_FT_MSG_OFFER:
        if (ctx->state != ESPNOW_FT_STATE_IDLE)
            return false;
        /* Parse offer: filename(32) + size(4LE) + crc(4LE) + chunk_size(1) */
        if (len < ESPNOW_FT_FILENAME_MAX + 4 + 4 + 1)
            return false;
        memcpy(ctx->peer_mac, peer_mac, ESPNOW_FT_MAC_LEN);
        memcpy(ctx->filename, data, ESPNOW_FT_FILENAME_MAX);
        ctx->filename[ESPNOW_FT_FILENAME_MAX] = '\0';
        {
            size_t off = ESPNOW_FT_FILENAME_MAX;
            ctx->file_size = (uint32_t)data[off]
                           | ((uint32_t)data[off+1] << 8)
                           | ((uint32_t)data[off+2] << 16)
                           | ((uint32_t)data[off+3] << 24);
            off += 4;
            ctx->expected_crc32 = (uint32_t)data[off]
                                | ((uint32_t)data[off+1] << 8)
                                | ((uint32_t)data[off+2] << 16)
                                | ((uint32_t)data[off+3] << 24);
            off += 4;
            ctx->chunk_size = data[off];
            if (ctx->chunk_size == 0 || ctx->chunk_size > ESPNOW_FT_CHUNK_MAX)
                ctx->chunk_size = ESPNOW_FT_CHUNK_MAX;
        }
        ctx->state = ESPNOW_FT_STATE_OFFER_RECEIVED;
        (void)seq;
        return true;

    case ESPNOW_FT_MSG_DATA:
        if (ctx->state != ESPNOW_FT_STATE_RECEIVING)
            return false;
        /* Parse: offset(4LE) + data */
        if (len < 4)
            return false;
        {
            uint32_t offset = (uint32_t)data[0]
                            | ((uint32_t)data[1] << 8)
                            | ((uint32_t)data[2] << 16)
                            | ((uint32_t)data[3] << 24);
            const uint8_t *chunk = data + 4;
            size_t chunk_len = len - 4;

            /* Only accept sequential data (no out-of-order in stop-and-wait) */
            if (offset != ctx->bytes_transferred)
                return false;

            /* Write to file */
            if (ctx->file_handle && ctx->hal->file_write) {
                if (!ctx->hal->file_write(ctx->file_handle, chunk,
                                          chunk_len, ctx->hal->ctx)) {
                    /* Write failed — abort */
                    send_simple(ctx, ESPNOW_FT_MSG_ABORT, seq);
                    ctx->state = ESPNOW_FT_STATE_FAILED;
                    return true;
                }
            }

            /* Accumulate CRC */
            ctx->running_crc32 = espnow_ft_crc32(ctx->running_crc32,
                                                   chunk, chunk_len);
            ctx->bytes_transferred += (uint32_t)chunk_len;

            /* Send ACK */
            send_simple(ctx, ESPNOW_FT_MSG_ACK, seq);
        }
        return true;

    case ESPNOW_FT_MSG_COMPLETE:
        if (ctx->state != ESPNOW_FT_STATE_RECEIVING)
            return false;
        /* Verify CRC */
        if (ctx->running_crc32 == ctx->expected_crc32 &&
            ctx->bytes_transferred == ctx->file_size) {
            ctx->state = ESPNOW_FT_STATE_DONE;
        } else {
            send_simple(ctx, ESPNOW_FT_MSG_ABORT, seq);
            ctx->state = ESPNOW_FT_STATE_FAILED;
        }
        /* Close file */
        if (ctx->file_handle && ctx->hal->file_close) {
            ctx->hal->file_close(ctx->file_handle, ctx->hal->ctx);
            ctx->file_handle = NULL;
        }
        return true;

    case ESPNOW_FT_MSG_ABORT:
        ctx->state = ESPNOW_FT_STATE_FAILED;
        if (ctx->file_handle && ctx->hal->file_close) {
            ctx->hal->file_close(ctx->file_handle, ctx->hal->ctx);
            ctx->file_handle = NULL;
        }
        return true;

    default:
        break;
    }
    return false;
}

bool espnow_ft_recv_accept(espnow_ft_ctx_t *ctx, const char *save_path)
{
    if (!ctx || ctx->state != ESPNOW_FT_STATE_OFFER_RECEIVED)
        return false;

    /* Open file for writing */
    if (ctx->hal->file_open && save_path) {
        ctx->file_handle = ctx->hal->file_open(save_path, ctx->hal->ctx);
        if (!ctx->file_handle) {
            ctx->state = ESPNOW_FT_STATE_FAILED;
            return false;
        }
    }

    /* Send ACCEPT */
    bool ok = send_simple(ctx, ESPNOW_FT_MSG_ACCEPT, 0);
    if (ok) {
        ctx->state = ESPNOW_FT_STATE_RECEIVING;
        ctx->bytes_transferred = 0;
        ctx->running_crc32 = 0;
        ctx->current_seq = 0;
    }
    return ok;
}

bool espnow_ft_recv_reject(espnow_ft_ctx_t *ctx)
{
    if (!ctx || ctx->state != ESPNOW_FT_STATE_OFFER_RECEIVED)
        return false;

    send_simple(ctx, ESPNOW_FT_MSG_REJECT, 0);
    ctx->state = ESPNOW_FT_STATE_IDLE;
    return true;
}
