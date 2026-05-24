/* See COPYING.txt for license details. */

/*
 * rpc_parse_impl.c — host-testable RPC frame parser.
 *
 * Mirrors the state machine from m1_csrc/m1_rpc.c::rpc_parse_byte()
 * but uses an explicit context struct for testability.
 *
 * Keep in sync with m1_csrc/m1_rpc.c.
 */

#include "rpc_parse_impl.h"
#include <string.h>

void rpc_parse_init(RPC_ParseCtx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = RPC_STATE_IDLE;
}

void rpc_parse_byte(RPC_ParseCtx *ctx, uint8_t byte)
{
    switch (ctx->state)
    {
    case RPC_STATE_IDLE:
        if (byte == RPC_SYNC_BYTE)
        {
            ctx->header_idx  = 0;
            ctx->payload_idx = 0;
            ctx->crc_idx     = 0;
            ctx->frame_ready = false;
            ctx->state       = RPC_STATE_HEADER;
        }
        break;

    case RPC_STATE_HEADER:
        ctx->header_buf[ctx->header_idx++] = byte;
        if (ctx->header_idx >= 4)
        {
            ctx->frame.cmd = ctx->header_buf[0];
            ctx->frame.seq = ctx->header_buf[1];
            ctx->frame.len = (uint16_t)ctx->header_buf[2] |
                             ((uint16_t)ctx->header_buf[3] << 8);

            if (ctx->frame.len > RPC_MAX_PAYLOAD)
            {
                ctx->state = RPC_STATE_IDLE;
            }
            else if (ctx->frame.len == 0)
            {
                ctx->state = RPC_STATE_CRC;
            }
            else
            {
                ctx->state = RPC_STATE_PAYLOAD;
            }
        }
        break;

    case RPC_STATE_PAYLOAD:
        ctx->frame.payload[ctx->payload_idx++] = byte;
        if (ctx->payload_idx >= ctx->frame.len)
        {
            ctx->state = RPC_STATE_CRC;
        }
        break;

    case RPC_STATE_CRC:
        ctx->crc_buf[ctx->crc_idx++] = byte;
        if (ctx->crc_idx >= 2)
        {
            uint16_t rx_crc = (uint16_t)ctx->crc_buf[0] |
                              ((uint16_t)ctx->crc_buf[1] << 8);

            uint16_t computed_crc = rpc_crc16(ctx->header_buf, 4);
            if (ctx->frame.len > 0)
            {
                computed_crc = rpc_crc16_continue(computed_crc,
                                                   ctx->frame.payload,
                                                   ctx->frame.len);
            }

            if (rx_crc == computed_crc)
            {
                ctx->frame_ready = true;
            }

            ctx->state = RPC_STATE_IDLE;
        }
        break;

    default:
        ctx->state = RPC_STATE_IDLE;
        break;
    }
}

uint16_t rpc_build_frame(uint8_t *buf, uint8_t cmd, uint8_t seq,
                         const uint8_t *payload, uint16_t payload_len)
{
    uint16_t idx = 0;

    /* Sync byte */
    buf[idx++] = RPC_SYNC_BYTE;

    /* Header: CMD, SEQ, LEN (LE) */
    uint8_t header[4];
    header[0] = cmd;
    header[1] = seq;
    header[2] = (uint8_t)(payload_len & 0xFF);
    header[3] = (uint8_t)(payload_len >> 8);
    memcpy(&buf[idx], header, 4);
    idx += 4;

    /* Payload */
    if (payload_len > 0 && payload)
    {
        memcpy(&buf[idx], payload, payload_len);
        idx += payload_len;
    }

    /* CRC-16 over header + payload */
    uint16_t crc = rpc_crc16(header, 4);
    if (payload_len > 0 && payload)
    {
        crc = rpc_crc16_continue(crc, payload, payload_len);
    }

    /* CRC little-endian */
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}
