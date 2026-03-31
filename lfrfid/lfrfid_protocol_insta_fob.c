/* See COPYING.txt for license details. */
/*
 * LF RFID — InstaFob (Hillman Group) Manchester decoder
 *
 * ASK/Manchester modulation, RF/32 (256 us/bit), 64-bit data (8 bytes).
 * Block 1 magic word is always 0x00107060.  Block 2 is user data.
 * Uses the M1 generic Manchester decoder (manch_decoder_t).
 *
 * Ported from Momentum Firmware (Next-Flip/Momentum-Firmware).
 * Original: Copyright (C) Flipper Devices Inc. (GPLv3).
 * Modifications: Copyright (C) 2026 Monstatek.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "app_freertos.h"
#include "cmsis_os.h"
#include "main.h"
#include "uiView.h"
#include "lfrfid.h"
#include "lfrfid_manchester.h"
#include "lfrfid_bit_lib.h"

static manch_decoder_t g_fob_dec;
static uint8_t         g_fob_dec_data[INSTAFOB_DECODED_DATA_SIZE];

/* Read 32 bits MSB-first from byte buffer at bit offset */
static uint32_t fob_get32(const uint8_t *d, size_t off) {
    uint32_t v = 0;
    for (size_t i = 0; i < 32; i++) {
        size_t idx = off + i;
        bool   b   = (d[idx/8] >> (7 - idx%8)) & 1;
        v = (v << 1) | (b ? 1u : 0u);
    }
    return v;
}

static bool fob_valid(const uint8_t *d) {
    return fob_get32(d, INSTAFOB_DATA_OFFSET_BITS) == INSTAFOB_BLOCK1;
}

static void fob_extract(const uint8_t *enc, uint8_t *dec) {
    for (size_t i = 0; i < INSTAFOB_DECODED_DATA_SIZE; i++) {
        uint8_t byte = 0;
        for (size_t b = 0; b < 8; b++) {
            size_t idx = INSTAFOB_DATA_OFFSET_BITS + i*8 + b;
            bool   bit = (enc[idx/8] >> (7 - idx%8)) & 1;
            byte = (uint8_t)((byte << 1) | (bit ? 1u : 0u));
        }
        dec[i] = byte;
    }
}

static void fob_begin(void) {
    manch_init(&g_fob_dec, INSTAFOB_HALF_BIT_US, INSTAFOB_ENCODED_BITS);
    memset(g_fob_dec_data, 0, sizeof(g_fob_dec_data));
}

static bool fob_execute(void *proto, uint16_t size) {
    lfrfid_evt_t *ev = (lfrfid_evt_t *)proto;
    manch_feed_events(&g_fob_dec, ev, (uint8_t)size);
    if (!manch_is_full(&g_fob_dec)) return false;
    if (!fob_valid(g_fob_dec.frame_buffer)) return false;
    fob_extract(g_fob_dec.frame_buffer, g_fob_dec_data);
    memcpy(lfrfid_tag_info.uid, g_fob_dec_data,
           min(sizeof(lfrfid_tag_info.uid), INSTAFOB_DECODED_DATA_SIZE));
    lfrfid_tag_info.bitrate = 32;
    return true;
}

static uint8_t *fob_get_data(void *p) { (void)p; return g_fob_dec_data; }

static void fob_render(void *p, char *r) {
    (void)p;
    uint32_t b1 = ((uint32_t)g_fob_dec_data[0]<<24)|((uint32_t)g_fob_dec_data[1]<<16)|
                  ((uint32_t)g_fob_dec_data[2]<<8) | g_fob_dec_data[3];
    uint32_t b2 = ((uint32_t)g_fob_dec_data[4]<<24)|((uint32_t)g_fob_dec_data[5]<<16)|
                  ((uint32_t)g_fob_dec_data[6]<<8) | g_fob_dec_data[7];
    sprintf(r, "Blk1: %08lX\nBlk2: %08lX", (unsigned long)b1, (unsigned long)b2);
}

const LFRFIDProtocolBase protocol_insta_fob = {
    .name        = "InstaFob",
    .manufacturer= "Hillman",
    .data_size   = INSTAFOB_DECODED_DATA_SIZE,
    .features    = LFRFIDFeatureASK,
    .get_data    = (lfrfidProtocolGetData)fob_get_data,
    .decoder     = { .begin   = (lfrfidProtocolDecoderBegin)fob_begin,
                     .execute = (lfrfidProtocolDecoderExecute)fob_execute },
    .encoder     = { .begin = NULL, .send = NULL },
    .write       = { .begin = NULL, .send = NULL },
    .render_data = (lfrfidProtocolRenderData)fob_render,
};
