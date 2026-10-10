/* See COPYING.txt for license details. */

#include "pocsag_receiver.h"

#include <stddef.h>
#include <string.h>

#define POCSAG_SYNC_WORD       0x7CD215D8UL
#define POCSAG_IDLE_WORD       0x7A89C197UL
#define POCSAG_CODEWORD_BITS   32U
#define POCSAG_PREAMBLE_BITS   24U
#define POCSAG_FRAME_WORDS     16U
#define POCSAG_MAX_RUN_BITS    32U
#define POCSAG_BCH_GENERATOR   0x769U

typedef struct {
    uint16_t period_us;
    uint16_t baud;
    uint16_t tolerance_us;
} pocsag_rate_t;

static const pocsag_rate_t pocsag_rates[] = {
    {1950U, 512U, 300U},
    {833U, 1200U, 120U},
    {417U, 2400U, 80U},
};

static uint16_t duration_diff(uint16_t a, uint16_t b)
{
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static bool select_rate(uint16_t duration_us, uint16_t *period_us,
                        uint16_t *baud)
{
    for (size_t i = 0; i < sizeof(pocsag_rates) / sizeof(pocsag_rates[0]); ++i)
    {
        if (duration_diff(duration_us, pocsag_rates[i].period_us) <=
            pocsag_rates[i].tolerance_us)
        {
            *period_us = pocsag_rates[i].period_us;
            *baud = pocsag_rates[i].baud;
            return true;
        }
    }

    uint16_t best_diff = UINT16_MAX;
    const pocsag_rate_t *best = NULL;

    for (size_t i = 0; i < sizeof(pocsag_rates) / sizeof(pocsag_rates[0]); ++i)
    {
        for (uint8_t run = 2U; run <= 16U; ++run)
        {
            uint16_t expected = (uint16_t)(pocsag_rates[i].period_us * run);
            uint16_t diff = duration_diff(duration_us, expected);
            uint16_t tolerance = (uint16_t)(pocsag_rates[i].tolerance_us * run);
            if (diff <= tolerance && (uint16_t)(diff / run) < best_diff)
            {
                best_diff = (uint16_t)(diff / run);
                best = &pocsag_rates[i];
            }
        }
    }

    if (best == NULL)
        return false;

    *period_us = best->period_us;
    *baud = best->baud;
    return true;
}

static void wait_for_preamble(pocsag_receiver_t *receiver)
{
    receiver->state = POCSAG_RX_WAIT_PREAMBLE;
    receiver->shift_register = 0;
    receiver->bit_count = 0;
    receiver->alternating_bits = 0;
    receiver->have_previous_bit = false;
}

static void append_char(pocsag_receiver_t *receiver, uint8_t value)
{
    if (value == 0)
        return;
    if (receiver->text_length + 1U >= POCSAG_MESSAGE_TEXT_MAX)
        return;

    receiver->message.text[receiver->text_length++] = (char)value;
    receiver->message.text[receiver->text_length] = '\0';
}

static void finish_message(pocsag_receiver_t *receiver)
{
    if (!receiver->message_active)
        return;

    bool emitted = false;
    if (receiver->text_length > 0U || receiver->function == 1U)
    {
        receiver->message.ric = receiver->ric;
        receiver->message.function = receiver->function;
        receiver->message.baud = receiver->baud;
        receiver->message.repeat_count = 1U;
        receiver->message.frequency_hz = 0U;
        receiver->message_ready = true;
        emitted = true;
    }

    receiver->message_active = false;
    receiver->text_length = 0;
    receiver->char_value = 0;
    receiver->char_bit_count = 0;
    if (!emitted)
        receiver->message.text[0] = '\0';
}

static uint8_t reverse_nibble(uint8_t value)
{
    value = (uint8_t)(((value & 0x5U) << 1) | ((value & 0xAU) >> 1));
    return (uint8_t)(((value & 0x3U) << 2) | ((value & 0xCU) >> 2));
}

static uint16_t bch_remainder(uint32_t word)
{
    uint32_t value = word >> 1;
    for (int8_t bit = 30; bit >= 10; --bit)
    {
        if ((value & (1UL << bit)) != 0U)
            value ^= POCSAG_BCH_GENERATOR << (bit - 10);
    }
    return (uint16_t)(value & 0x3FFU);
}

static bool codeword_valid(uint32_t word)
{
    uint32_t parity = word;
    parity ^= parity >> 16;
    parity ^= parity >> 8;
    parity ^= parity >> 4;
    parity ^= parity >> 2;
    parity ^= parity >> 1;
    return (bch_remainder(word) == 0U) && ((parity & 1U) == 0U);
}

static bool correct_codeword(uint32_t *word)
{
    if (codeword_valid(*word))
        return true;

    uint32_t received = *word;
    for (uint8_t first = 0; first < 32U; ++first)
    {
        uint32_t one_bit = received ^ (1UL << first);
        if (codeword_valid(one_bit))
        {
            *word = one_bit;
            return true;
        }
        for (uint8_t second = (uint8_t)(first + 1U); second < 32U; ++second)
        {
            uint32_t two_bits = one_bit ^ (1UL << second);
            if (codeword_valid(two_bits))
            {
                *word = two_bits;
                return true;
            }
        }
    }
    return false;
}

static void decode_numeric_word(pocsag_receiver_t *receiver, uint32_t word)
{
    static const char extra_chars[] = "*U -)(";

    for (uint8_t i = 0; i < 5U; ++i)
    {
        uint8_t digit = (uint8_t)((word >> (27U - (uint32_t)i * 4U)) & 0x0FU);
        digit = reverse_nibble(digit);
        append_char(receiver, (digit <= 9U) ? (uint8_t)('0' + digit) :
                    (uint8_t)extra_chars[digit - 10U]);
    }
}

static void decode_alphanumeric_word(pocsag_receiver_t *receiver,
                                     uint32_t word)
{
    for (int8_t bit = 19; bit >= 0; --bit)
    {
        receiver->char_value >>= 1;
        if ((word >> (11U + (uint8_t)bit)) & 1U)
            receiver->char_value |= 0x40U;
        receiver->char_bit_count++;
        if (receiver->char_bit_count == 7U)
        {
            if (receiver->char_value == 0U)
            {
                finish_message(receiver);
                return;
            }
            append_char(receiver, receiver->char_value);
            receiver->char_value = 0;
            receiver->char_bit_count = 0;
        }
    }
}

static void process_codeword(pocsag_receiver_t *receiver, uint32_t word)
{
    if (word == POCSAG_SYNC_WORD)
    {
        receiver->codeword_index = 0;
        return;
    }

    if (!correct_codeword(&word))
        goto next_codeword;

    if (word == POCSAG_IDLE_WORD)
    {
        finish_message(receiver);
    }
    else if ((word & 0x80000000UL) == 0U)
    {
        finish_message(receiver);
        receiver->ric = ((word >> 13) << 3) |
                        ((receiver->codeword_index >> 1) & 0x07U);
        receiver->function = (uint8_t)((word >> 11) & 0x03U);
        receiver->message_active = true;
        receiver->text_length = 0;
        receiver->char_value = 0;
        receiver->char_bit_count = 0;
    }
    else if (receiver->message_active)
    {
        if (receiver->function == 0U)
            decode_numeric_word(receiver, word);
        else if (receiver->function == 2U || receiver->function == 3U)
            decode_alphanumeric_word(receiver, word);
    }

next_codeword:
    receiver->codeword_index++;
    if (receiver->codeword_index >= POCSAG_FRAME_WORDS)
    {
        receiver->state = POCSAG_RX_BATCH_SYNC;
        receiver->shift_register = 0;
        receiver->bit_count = 0;
    }
}

static void feed_bit(pocsag_receiver_t *receiver, bool bit)
{
    if (receiver->message_ready)
        return;

    switch (receiver->state)
    {
        case POCSAG_RX_WAIT_PREAMBLE:
            if (receiver->have_previous_bit && bit != receiver->previous_bit)
                receiver->alternating_bits++;
            else
                receiver->alternating_bits = 1U;
            receiver->previous_bit = bit;
            receiver->have_previous_bit = true;
            if (receiver->alternating_bits >= POCSAG_PREAMBLE_BITS)
            {
                receiver->state = POCSAG_RX_WAIT_SYNC;
                receiver->shift_register = 0;
                receiver->bit_count = 0;
            }
            break;

        case POCSAG_RX_WAIT_SYNC:
            receiver->shift_register = (receiver->shift_register << 1) |
                                       (uint32_t)bit;
            if (receiver->bit_count < POCSAG_CODEWORD_BITS)
                receiver->bit_count++;
            if (receiver->bit_count == POCSAG_CODEWORD_BITS)
            {
                if (receiver->shift_register == POCSAG_SYNC_WORD)
                {
                    receiver->inverted = false;
                    receiver->state = POCSAG_RX_CODEWORDS;
                    receiver->codeword = 0;
                    receiver->bit_count = 0;
                    receiver->codeword_index = 0;
                }
                else if (receiver->shift_register == (uint32_t)~POCSAG_SYNC_WORD)
                {
                    receiver->inverted = true;
                    receiver->state = POCSAG_RX_CODEWORDS;
                    receiver->codeword = 0;
                    receiver->bit_count = 0;
                    receiver->codeword_index = 0;
                }
            }
            break;

        case POCSAG_RX_CODEWORDS:
            bit = receiver->inverted ? !bit : bit;
            receiver->codeword = (receiver->codeword << 1) | (uint32_t)bit;
            receiver->bit_count++;
            if (receiver->bit_count == POCSAG_CODEWORD_BITS)
            {
                process_codeword(receiver, receiver->codeword);
                receiver->codeword = 0;
                receiver->bit_count = 0;
            }
            break;

        case POCSAG_RX_BATCH_SYNC:
            bit = receiver->inverted ? !bit : bit;
            receiver->shift_register = (receiver->shift_register << 1) |
                                       (uint32_t)bit;
            receiver->bit_count++;
            if (receiver->bit_count == POCSAG_CODEWORD_BITS)
            {
                if (receiver->shift_register == POCSAG_SYNC_WORD)
                {
                    receiver->state = POCSAG_RX_CODEWORDS;
                    receiver->codeword_index = 0;
                    receiver->codeword = 0;
                    receiver->bit_count = 0;
                }
                else
                {
                    wait_for_preamble(receiver);
                    receiver->bit_period_us = 0;
                    receiver->baud = 0;
                }
            }
            break;
    }
}

void pocsag_receiver_reset(pocsag_receiver_t *receiver)
{
    if (receiver != NULL)
        memset(receiver, 0, sizeof(*receiver));
}

bool pocsag_receiver_feed(pocsag_receiver_t *receiver, bool level,
                          uint16_t duration_us)
{
    if (receiver == NULL || duration_us == 0U)
        return false;
    if (receiver->message_ready)
        return true;

    bool bit = !level;

    if (receiver->bit_period_us == 0U)
    {
        if (!select_rate(duration_us, &receiver->bit_period_us, &receiver->baud))
            return false;
    }

    if ((uint32_t)duration_us > (uint32_t)receiver->bit_period_us * POCSAG_MAX_RUN_BITS)
    {
        finish_message(receiver);
        wait_for_preamble(receiver);
        receiver->bit_period_us = 0;
        receiver->baud = 0;
        return receiver->message_ready;
    }

    uint16_t bit_period = receiver->bit_period_us;
    uint16_t run_bits = (uint16_t)((duration_us + bit_period / 2U) / bit_period);
    if (run_bits == 0U || run_bits > POCSAG_MAX_RUN_BITS ||
        duration_diff(duration_us, (uint16_t)(run_bits * bit_period)) >
            (uint16_t)(bit_period / 3U))
    {
        finish_message(receiver);
        wait_for_preamble(receiver);
        receiver->bit_period_us = 0;
        receiver->baud = 0;
        return receiver->message_ready;
    }

    for (uint16_t i = 0; i < run_bits; ++i)
    {
        feed_bit(receiver, bit);
        if (receiver->message_ready)
            break;
    }

    return receiver->message_ready;
}

bool pocsag_receiver_take_message(pocsag_receiver_t *receiver,
                                  pocsag_message_t *message)
{
    if (receiver == NULL || message == NULL || !receiver->message_ready)
        return false;

    *message = receiver->message;
    receiver->message_ready = false;
    receiver->message.text[0] = '\0';
    return true;
}

void pocsag_receiver_app_state_enter(pocsag_receiver_app_state_t *state,
                                     bool resume_from_child,
                                     uint32_t custom_frequency_hz,
                                     uint8_t frequency_index,
                                     uint8_t modulation_index)
{
    if (state == NULL || resume_from_child)
        return;

    state->saved_custom_frequency_hz = custom_frequency_hz;
    state->saved_frequency_index = frequency_index;
    state->saved_modulation_index = modulation_index;
}

void pocsag_history_reset(pocsag_history_t *history)
{
    if (history != NULL)
        memset(history, 0, sizeof(*history));
}

bool pocsag_history_add(pocsag_history_t *history,
                        const pocsag_message_t *message,
                        uint32_t frequency_hz)
{
    if (history == NULL || message == NULL)
        return false;

    for (uint8_t i = 0; i < history->count; ++i)
    {
        uint8_t index = (uint8_t)((history->next + POCSAG_HISTORY_CAPACITY -
                                   1U - i) % POCSAG_HISTORY_CAPACITY);
        pocsag_message_t *entry = &history->entries[index];
        if (entry->ric == message->ric && entry->function == message->function &&
            entry->baud == message->baud &&
            strncmp(entry->text, message->text, POCSAG_MESSAGE_TEXT_MAX) == 0)
        {
            if (entry->repeat_count < UINT8_MAX)
                entry->repeat_count++;
            entry->frequency_hz = frequency_hz;
            return false;
        }
    }

    pocsag_message_t *entry = &history->entries[history->next];
    *entry = *message;
    entry->frequency_hz = frequency_hz;
    entry->repeat_count = 1U;
    history->next = (uint8_t)((history->next + 1U) % POCSAG_HISTORY_CAPACITY);
    if (history->count < POCSAG_HISTORY_CAPACITY)
        history->count++;
    return true;
}

const pocsag_message_t *pocsag_history_get(const pocsag_history_t *history,
                                           uint8_t newest_index)
{
    if (history == NULL || newest_index >= history->count)
        return NULL;

    uint8_t index = (uint8_t)((history->next + POCSAG_HISTORY_CAPACITY -
                               1U - newest_index) % POCSAG_HISTORY_CAPACITY);
    return &history->entries[index];
}
