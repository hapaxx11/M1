/* See COPYING.txt for license details. */

/**
 * @file   espnow_message.h
 * @brief  ESP-NOW short-text peer messaging — pure logic.
 *
 * Phase 2 of the peer link adds short text messages over the shared DATA
 * channel (app type block 0x20, see espnow_appmsg.h).  A message frame is:
 *
 *   byte 0 : type = ESPNOW_MSG_TYPE_TEXT (0x20)
 *   byte 1 : seq  rolling sequence number (sender-local, for de-dup/display)
 *   byte 2+: UTF-8 text bytes (length implied by frame length)
 *
 * A message may exceed the 42-byte per-NOW_SEND budget, so on the wire it is
 * handed to espnow_chunk for fragmentation; this module operates on the fully
 * reassembled frame and never touches the transport.
 *
 * The module also provides a small fixed-capacity inbox ring so a scene can
 * display the most recent conversation without dynamic allocation.
 *
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * M1 Project
 */

#ifndef ESPNOW_MESSAGE_H_
#define ESPNOW_MESSAGE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "espnow_appmsg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** DATA-channel type byte for a text message (within the 0x20 block). */
#define ESPNOW_MSG_TYPE_TEXT     ESPNOW_APP_MSG_BASE   /* 0x20 */

/** Message frame header length (type + seq). */
#define ESPNOW_MSG_HDR_LEN       2u

/** Maximum text length (bytes) in a single message. */
#define ESPNOW_MSG_TEXT_MAX      120u

/** MAC address length. */
#define ESPNOW_MSG_MAC_LEN       6u

/** Number of messages retained in the inbox ring. */
#define ESPNOW_INBOX_CAP         8u

/* =========================================================================
 * Framing
 * =========================================================================*/

/**
 * @brief  Build a text message frame ready for fragmentation/sending.
 *
 * @param  seq      Sender-local sequence number stamped into the frame.
 * @param  text     Null-terminated UTF-8 text (must be non-empty).
 * @param  out      Output buffer.
 * @param  out_cap  Capacity of @p out.
 * @param  out_len  Receives the frame length written.
 * @return true on success; false if text is empty, too long, contains an
 *         embedded NUL, or does not fit in @p out.
 */
bool espnow_msg_build(uint8_t seq, const char *text,
                      uint8_t *out, size_t out_cap, size_t *out_len);

/**
 * @brief  Parse a received (reassembled) message frame.
 *
 * @param  frame     Frame bytes.
 * @param  len       Frame length.
 * @param  out_seq   Receives the sequence number (may be NULL).
 * @param  text_out  Receives the null-terminated text.
 * @param  text_cap  Capacity of @p text_out (needs room for text + NUL).
 * @param  text_len  Receives the text length excluding NUL (may be NULL).
 * @return true if the frame is a well-formed text message that fits; false on
 *         a wrong type byte, truncated header, empty text, or buffer overflow.
 */
bool espnow_msg_parse(const uint8_t *frame, size_t len,
                      uint8_t *out_seq,
                      char *text_out, size_t text_cap, size_t *text_len);

/* =========================================================================
 * Inbox ring
 * =========================================================================*/

typedef struct {
    uint8_t mac[ESPNOW_MSG_MAC_LEN];
    uint8_t seq;
    bool    outgoing;                     /**< true if sent by us, false if received. */
    char    text[ESPNOW_MSG_TEXT_MAX + 1];
} espnow_msg_entry_t;

typedef struct {
    espnow_msg_entry_t items[ESPNOW_INBOX_CAP];
    uint8_t count;                        /**< Valid entries (≤ ESPNOW_INBOX_CAP). */
    uint8_t head;                         /**< Index of the oldest entry. */
    uint32_t total;                       /**< Lifetime messages recorded. */
} espnow_inbox_t;

/** Reset the inbox to empty. */
void espnow_inbox_init(espnow_inbox_t *ib);

/**
 * @brief  Append a message to the inbox ring (evicting the oldest if full).
 *
 * @param  ib        Inbox.
 * @param  mac       Peer MAC (sender for received, recipient for outgoing).
 * @param  seq       Sequence number.
 * @param  outgoing  true for a message we sent, false for a received one.
 * @param  text      Null-terminated text (truncated to ESPNOW_MSG_TEXT_MAX).
 * @return true on success, false on a NULL argument.
 */
bool espnow_inbox_push(espnow_inbox_t *ib, const uint8_t mac[ESPNOW_MSG_MAC_LEN],
                       uint8_t seq, bool outgoing, const char *text);

/**
 * @brief  Access an inbox entry by display index (0 = oldest retained).
 * @return Pointer to the entry, or NULL if @p idx is out of range.
 */
const espnow_msg_entry_t *espnow_inbox_get(const espnow_inbox_t *ib, uint8_t idx);

/**
 * @brief  True if (mac, seq) matches the most recent received entry.
 *
 * Lets a scene drop an immediate duplicate caused by fragment/frame retries
 * without maintaining a separate de-dup table.
 */
bool espnow_inbox_is_duplicate(const espnow_inbox_t *ib,
                               const uint8_t mac[ESPNOW_MSG_MAC_LEN], uint8_t seq);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_MESSAGE_H_ */
