#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZEROMESH_MAGIC0 0x94
#define ZEROMESH_MAGIC1 0xC3
#define MAX_FRAME_SIZE 512

typedef struct {
    uint8_t hdr[4];
    uint8_t hdr_pos;
    uint16_t frame_len;
    uint16_t frame_pos;
    uint8_t frame_buf[MAX_FRAME_SIZE];
} meshtastic_framing_t;

typedef enum {
    MESHTASTIC_FRAME_PENDING = 0,
    MESHTASTIC_FRAME_COMPLETE,
    MESHTASTIC_FRAME_BAD_MAGIC,
    MESHTASTIC_FRAME_BAD_LENGTH,
} meshtastic_frame_result_t;

void meshtastic_framing_reset(meshtastic_framing_t* framing);
meshtastic_frame_result_t meshtastic_framing_feed(meshtastic_framing_t* framing, uint8_t byte);
