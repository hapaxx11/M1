/* See COPYING.txt for license details. */

/**
 * @file   espnow_chunk.h
 * @brief  ESP-NOW payload fragmentation / reassembly — pure logic.
 *
 * The fixed 64-byte SPI-HD transaction that carries M1_RPC_NOW_SEND leaves at
 * most 42 bytes of ESP-NOW application data per call
 * (documentation/esp32_firmware.md).  Logical app messages larger than that
 * (short text, encrypted envelopes, trigger requests) must therefore be split
 * across several NOW_SEND calls and reassembled on the receiver.
 *
 * This module implements a pure app-layer fragmentation codec that needs **no
 * brain-firmware change**: each fragment is an ordinary small ESP-NOW DATA
 * frame carrying a 4-byte header, and the receiving M1 reassembles the original
 * message from the frames it drains via NOW_RECV_GET.
 *
 * Fragment frame layout (one per NOW_SEND, ≤ ESPNOW_CHUNK_FRAME_MAX bytes):
 *
 *   offset 0 : type      = ESPNOW_APP_FRAG (0x50)
 *   offset 1 : msg_id    rolling id shared by all fragments of one message
 *   offset 2 : frag_idx  0-based fragment index
 *   offset 3 : frag_cnt  total fragments (1..ESPNOW_CHUNK_MAX_FRAGS)
 *   offset 4+: fragment data (≤ ESPNOW_CHUNK_DATA_MAX)
 *
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * M1 Project
 */

#ifndef ESPNOW_CHUNK_H_
#define ESPNOW_CHUNK_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Fragment-frame app type (byte 0). Lives in a dedicated app block. */
#define ESPNOW_APP_FRAG            0x50u

/** Fragment header size in bytes (type, msg_id, frag_idx, frag_cnt). */
#define ESPNOW_CHUNK_HDR_LEN       4u

/** Max app-data bytes per NOW_SEND SPI call (see esp32_firmware.md). */
#define ESPNOW_CHUNK_FRAME_MAX     42u

/** Usable fragment payload bytes per frame. */
#define ESPNOW_CHUNK_DATA_MAX      (ESPNOW_CHUNK_FRAME_MAX - ESPNOW_CHUNK_HDR_LEN)

/** Largest logical message that can be reassembled (ESP-NOW app limit).
 *  Sized to comfortably fit the largest sealed envelope the peer-link
 *  protocols actually produce today (see ESPNOW_CRYPTO_PLAINTEXT_MAX) —
 *  kept tight since this reassembly buffer is a permanently resident
 *  (.bss) buffer on a tightly RAM-constrained target. */
#define ESPNOW_CHUNK_MSG_MAX       104u

/** Maximum fragments for the largest message. */
#define ESPNOW_CHUNK_MAX_FRAGS \
    ((ESPNOW_CHUNK_MSG_MAX + ESPNOW_CHUNK_DATA_MAX - 1u) / ESPNOW_CHUNK_DATA_MAX)

/* =========================================================================
 * Fragment-count helper
 * =========================================================================*/

/**
 * @brief  Number of fragments a message of @p len bytes will split into.
 * @return 0 if @p len is 0 or exceeds ESPNOW_CHUNK_MSG_MAX, else 1..MAX_FRAGS.
 */
uint8_t espnow_chunk_frag_count(size_t len);

/* =========================================================================
 * Splitter (sender side)
 * =========================================================================*/

typedef struct {
    const uint8_t *data;   /**< Source message (borrowed, must outlive split). */
    size_t   len;          /**< Source length. */
    uint8_t  msg_id;       /**< Rolling id stamped into every fragment. */
    uint8_t  frag_cnt;     /**< Total fragments. */
    uint8_t  next_idx;     /**< Next fragment index to emit. */
} espnow_chunk_splitter_t;

/**
 * @brief  Initialise a splitter over a message buffer.
 * @return true if @p len is valid (1..ESPNOW_CHUNK_MSG_MAX), false otherwise.
 */
bool espnow_chunk_split_init(espnow_chunk_splitter_t *s,
                             uint8_t msg_id,
                             const uint8_t *data, size_t len);

/**
 * @brief  Emit the next fragment frame.
 *
 * @param  s        Splitter.
 * @param  out      Output frame buffer (≥ ESPNOW_CHUNK_FRAME_MAX bytes).
 * @param  out_cap  Capacity of @p out.
 * @param  out_len  Receives the frame length written.
 * @return true if a fragment was written, false when all fragments are emitted
 *         or on a buffer/argument error.
 */
bool espnow_chunk_split_next(espnow_chunk_splitter_t *s,
                             uint8_t *out, size_t out_cap, size_t *out_len);

/* =========================================================================
 * Reassembler (receiver side)
 * =========================================================================*/

typedef enum {
    ESPNOW_CHUNK_IGNORED = 0,  /**< Frame is not a fragment (byte0 != FRAG). */
    ESPNOW_CHUNK_NEED_MORE,    /**< Fragment accepted, more expected. */
    ESPNOW_CHUNK_COMPLETE,     /**< Message fully reassembled — read msg[]. */
    ESPNOW_CHUNK_ERROR,        /**< Malformed / inconsistent fragment. */
} espnow_chunk_status_t;

typedef struct {
    bool     active;       /**< A message is currently being reassembled. */
    uint8_t  msg_id;       /**< Id of the in-progress message. */
    uint8_t  frag_cnt;     /**< Total fragments expected. */
    uint8_t  recv_mask_cnt;/**< Distinct fragments received so far. */
    bool     got[ESPNOW_CHUNK_MAX_FRAGS]; /**< Per-index received flags. */
    uint16_t frag_len[ESPNOW_CHUNK_MAX_FRAGS]; /**< Per-index data length. */
    uint8_t  msg[ESPNOW_CHUNK_MSG_MAX];   /**< Reassembly buffer. */
    uint16_t msg_len;      /**< Valid on ESPNOW_CHUNK_COMPLETE. */
} espnow_chunk_reasm_t;

/** Reset a reassembler to the empty state. */
void espnow_chunk_reasm_init(espnow_chunk_reasm_t *r);

/**
 * @brief  Feed one received frame into the reassembler.
 *
 * A frame whose byte 0 is not ESPNOW_APP_FRAG is ignored (returns
 * ESPNOW_CHUNK_IGNORED) so callers can hand every inbound frame here and only
 * treat non-IGNORED results specially.  Starting a new msg_id resets any
 * partially-received message.
 *
 * @return See ::espnow_chunk_status_t.  On COMPLETE, r->msg / r->msg_len hold
 *         the reassembled message and the reassembler is reset for reuse.
 */
espnow_chunk_status_t espnow_chunk_reasm_feed(espnow_chunk_reasm_t *r,
                                              const uint8_t *frame, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_CHUNK_H_ */
