/* See COPYING.txt for license details. */

/*
 * rpc_parse_impl.h — host-testable interface for RPC frame parser.
 *
 * Mirrors the state machine from m1_csrc/m1_rpc.c but operates on an
 * explicit context struct instead of static globals, enabling unit tests
 * to feed bytes one-at-a-time and inspect resulting frames.
 */

#ifndef RPC_PARSE_IMPL_H_
#define RPC_PARSE_IMPL_H_

#include <stdint.h>
#include <stdbool.h>
#include "rpc_crc16_impl.h"

/* Frame constants — match m1_rpc.h */
#define RPC_SYNC_BYTE       0xAA
#define RPC_MAX_PAYLOAD     8192

/* Parser state machine — match m1_rpc.h */
typedef enum {
    RPC_STATE_IDLE = 0,
    RPC_STATE_HEADER,
    RPC_STATE_PAYLOAD,
    RPC_STATE_CRC
} E_RPC_ParseState;

/* Parsed frame — match m1_rpc.h */
typedef struct {
    uint8_t  cmd;
    uint8_t  seq;
    uint16_t len;
    uint8_t  payload[RPC_MAX_PAYLOAD];
} S_RPC_Frame;

/* Parser context (replaces static globals for testability) */
typedef struct {
    E_RPC_ParseState state;
    uint8_t  header_buf[4];
    uint8_t  header_idx;
    uint16_t payload_idx;
    uint8_t  crc_buf[2];
    uint8_t  crc_idx;
    S_RPC_Frame frame;

    /* Callback: set when a valid frame has been parsed */
    bool     frame_ready;
} RPC_ParseCtx;

/**
 * @brief  Initialize the parser context to idle state.
 */
void rpc_parse_init(RPC_ParseCtx *ctx);

/**
 * @brief  Feed one byte to the parser state machine.
 *         When a complete, CRC-valid frame is received, ctx->frame_ready
 *         is set to true and the frame is available in ctx->frame.
 */
void rpc_parse_byte(RPC_ParseCtx *ctx, uint8_t byte);

/**
 * @brief  Build a complete RPC frame with CRC into buf.
 *         Returns the total frame size written.
 *         buf must be at least 1 + 4 + payload_len + 2 bytes.
 */
uint16_t rpc_build_frame(uint8_t *buf, uint8_t cmd, uint8_t seq,
                         const uint8_t *payload, uint16_t payload_len);

#endif /* RPC_PARSE_IMPL_H_ */
