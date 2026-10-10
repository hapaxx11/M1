/* See COPYING.txt for license details. */

#ifndef POCSAG_RECEIVER_H
#define POCSAG_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

#define POCSAG_MESSAGE_TEXT_MAX 48U
#define POCSAG_HISTORY_CAPACITY 10U

typedef struct {
    uint32_t ric;
    uint16_t baud;
    uint8_t function;
    uint8_t repeat_count;
    uint32_t frequency_hz;
    char text[POCSAG_MESSAGE_TEXT_MAX];
} pocsag_message_t;

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
    uint8_t pulse_level;
    uint8_t bit_count;
    uint8_t alternating_bits;
    uint8_t codeword_index;
    uint8_t char_value;
    uint8_t char_bit_count;
    uint8_t text_length;
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

void pocsag_receiver_reset(pocsag_receiver_t *receiver);
bool pocsag_receiver_feed(pocsag_receiver_t *receiver, uint16_t duration_us);
bool pocsag_receiver_take_message(pocsag_receiver_t *receiver,
                                  pocsag_message_t *message);

void pocsag_history_reset(pocsag_history_t *history);
bool pocsag_history_add(pocsag_history_t *history,
                        const pocsag_message_t *message,
                        uint32_t frequency_hz);
const pocsag_message_t *pocsag_history_get(const pocsag_history_t *history,
                                           uint8_t newest_index);

#endif
