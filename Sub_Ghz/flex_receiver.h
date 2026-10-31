/* See COPYING.txt for license details. */

#ifndef FLEX_RECEIVER_H
#define FLEX_RECEIVER_H

#include "pager_message.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLEX_PHASE_WORDS 88U
#define FLEX_FRAGMENT_SLOTS 4U
#define FLEX_FRAGMENT_IDENTITIES 8U
#define FLEX_SEEN_PAGES 16U

typedef enum {
    FLEX_RX_SYNC1 = 0,
    FLEX_RX_FIW,
    FLEX_RX_SYNC2,
    FLEX_RX_DATA,
} flex_rx_state_t;

typedef struct {
    uint32_t hash;
    uint32_t tick;
    uint32_t capcode;
} flex_seen_page_t;

typedef struct {
    bool used;
    uint32_t tick;
    uint32_t capcode;
    uint8_t type;
    uint8_t message_number;
    uint8_t next_fragment;
    uint8_t parts;
    uint8_t signature;
    bool signature_present;
    bool suspect;
    uint32_t signature_sum;
    uint32_t part_identities[FLEX_FRAGMENT_IDENTITIES];
    uint8_t identity_count;
    uint8_t identity_next;
    char phase;
    uint8_t frame;
    size_t text_length;
    char text[PAGER_MESSAGE_TEXT_MAX];
} flex_fragment_slot_t;

typedef void (*flex_receiver_message_fn)(void *context,
                                         const pager_message_t *message);

typedef struct {
    flex_receiver_message_fn callback;
    void *callback_context;
    flex_rx_state_t state;
    uint64_t sync_buffer;
    bool inverted;
    bool symbol_input;
    uint16_t baud;
    uint8_t levels;
    uint8_t fiw_count;
    uint32_t fiw_raw;
    uint8_t cycle;
    uint8_t frame;
    uint16_t sync2_count;
    uint16_t data_count;
    uint16_t data_bit_count;
    bool phase_toggle;
    uint32_t phases[4][FLEX_PHASE_WORDS];
    flex_seen_page_t seen[FLEX_SEEN_PAGES];
    uint8_t seen_next;
    flex_fragment_slot_t fragments[FLEX_FRAGMENT_SLOTS];
    uint32_t page_words[FLEX_PHASE_WORDS];
    bool page_valid[FLEX_PHASE_WORDS];
    pager_message_t message;
    size_t text_length;
    uint8_t alpha_signature;
    uint32_t alpha_signature_sum;
    uint32_t now_ms;
} flex_receiver_t;

void flex_receiver_init(flex_receiver_t *receiver,
                        flex_receiver_message_fn callback,
                        void *callback_context);
void flex_receiver_reset(flex_receiver_t *receiver);
void flex_receiver_feed(flex_receiver_t *receiver, bool level,
                        uint16_t duration_us, uint32_t now_ms);
void flex_receiver_feed_symbol(flex_receiver_t *receiver, uint8_t symbol,
                               uint32_t now_ms);
void flex_receiver_flush_fragments(flex_receiver_t *receiver);

#endif
