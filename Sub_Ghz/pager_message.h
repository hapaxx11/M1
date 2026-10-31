/* See COPYING.txt for license details. */

#ifndef PAGER_MESSAGE_H
#define PAGER_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

#define PAGER_MESSAGE_TEXT_MAX 265U

typedef enum {
    PAGER_PROTOCOL_POCSAG = 0,
    PAGER_PROTOCOL_FLEX = 1,
} pager_protocol_t;

typedef struct {
    uint32_t ric;
    uint16_t baud;
    uint8_t function;
    uint8_t repeat_count;
    uint32_t frequency_hz;
    char text[PAGER_MESSAGE_TEXT_MAX];
    uint8_t protocol;
    uint8_t levels;
    uint8_t cycle;
    uint8_t frame;
    char phase;
    bool partial;
} pager_message_t;

#endif
