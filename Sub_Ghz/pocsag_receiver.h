/* See COPYING.txt for license details. */

#ifndef POCSAG_RECEIVER_H
#define POCSAG_RECEIVER_H

#include "pager_message.h"
#include "flex_receiver.h"

#include <stdbool.h>
#include <stdint.h>

#define POCSAG_MESSAGE_TEXT_MAX PAGER_MESSAGE_TEXT_MAX
#define POCSAG_HISTORY_CAPACITY 10U

typedef pager_message_t pocsag_message_t;

typedef enum {
    POCSAG_RX_WAIT_PREAMBLE = 0,
    POCSAG_RX_WAIT_SYNC,
    POCSAG_RX_CODEWORDS,
    POCSAG_RX_BATCH_SYNC,
} pocsag_rx_state_t;

typedef struct {
    pocsag_rx_state_t state;
    uint32_t shift_register;
    uint32_t codeword;
    uint16_t bit_period_us;
    uint16_t baud;
    uint8_t bit_count;
    uint8_t alternating_bits;
    uint8_t codeword_index;
    uint8_t char_value;
    uint8_t char_bit_count;
    uint16_t text_length;
    uint8_t function;
    uint32_t ric;
    bool previous_bit;
    bool have_previous_bit;
    bool inverted;
    bool message_active;
    bool message_ready;
    pocsag_message_t message;
} pocsag_receiver_t;

typedef struct {
    pocsag_message_t entries[POCSAG_HISTORY_CAPACITY];
    uint8_t count;
    uint8_t next;
} pocsag_history_t;

typedef struct {
    pocsag_receiver_t receiver;
    flex_receiver_t flex;
    pocsag_history_t history;
    uint32_t saved_custom_frequency_hz;
    uint8_t saved_frequency_index;
    uint8_t saved_modulation_index;
    uint8_t selected_message;
    uint16_t detail_offset;
    uint8_t channel_index;
    bool detail_view;
    bool radio_active;
} pocsag_receiver_app_state_t;

void pocsag_receiver_reset(pocsag_receiver_t *receiver);
bool pocsag_receiver_feed(pocsag_receiver_t *receiver, bool level,
                          uint16_t duration_us);
bool pocsag_receiver_take_message(pocsag_receiver_t *receiver,
                                  pocsag_message_t *message);
void pocsag_receiver_app_state_enter(pocsag_receiver_app_state_t *state,
                                     bool resume_from_child,
                                     uint32_t custom_frequency_hz,
                                     uint8_t frequency_index,
                                     uint8_t modulation_index);
void pocsag_receiver_app_state_select_newest(pocsag_receiver_app_state_t *state);

void pocsag_history_reset(pocsag_history_t *history);
bool pocsag_history_add(pocsag_history_t *history,
                        const pocsag_message_t *message,
                        uint32_t frequency_hz);
const pocsag_message_t *pocsag_history_get(const pocsag_history_t *history,
                                           uint8_t newest_index);

#endif
