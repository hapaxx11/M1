/* See COPYING.txt for license details. */
/*
 * LF RFID — Indala 224-bit (long-format) PSK2 decoder
 *
 * PSK2 modulation, 255 us/bit, 224-bit encoded frame.
 * Preamble: 1 followed by 29 zeros (30 bits).
 * Two frames are needed: second may have same or inverted preamble.
 * PSK2 differential decode: data[n] = phase[n] XOR phase[n+1].
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

/*---------------------------------------------------------------------------*/
/* Local bit helpers (same style as indala26 to avoid name clashes)          */
/*---------------------------------------------------------------------------*/
static inline void i224_push(uint8_t *d, size_t dsz, bool b) {
    for (size_t i = 0; i < dsz - 1; i++)
        d[i] = (uint8_t)((d[i] << 1) | (d[i+1] >> 7));
    d[dsz-1] = (uint8_t)((d[dsz-1] << 1) | (b ? 1u : 0u));
}
static inline bool i224_get(const uint8_t *d, size_t idx) {
    return (d[idx/8] >> (7 - idx%8)) & 1;
}
static inline void i224_set(uint8_t *d, size_t idx, bool v) {
    if (v) d[idx/8] |=  (uint8_t)(1u << (7 - idx%8));
    else   d[idx/8] &= (uint8_t)~(1u << (7 - idx%8));
}

/*---------------------------------------------------------------------------*/
static uint8_t g_enc[INDALA224_ENCODED_DATA_SIZE];
static uint8_t g_enc_neg[INDALA224_ENCODED_DATA_SIZE];
static uint8_t g_dec[INDALA224_DECODED_DATA_SIZE];

static bool check_preamble(const uint8_t *d, size_t off) {
    if (!i224_get(d, off)) return false;            /* bit 0 must be 1 */
    for (size_t i = 1; i < INDALA224_PREAMBLE_BIT_SIZE; i++)
        if (i224_get(d, off + i)) return false;     /* bits 1..29 must be 0 */
    return true;
}
static bool check_inv_preamble(const uint8_t *d, size_t off) {
    if (i224_get(d, off)) return false;             /* bit 0 must be 0 */
    for (size_t i = 1; i < INDALA224_PREAMBLE_BIT_SIZE; i++)
        if (!i224_get(d, off + i)) return false;    /* bits 1..29 must be 1 */
    return true;
}
static bool can_decode(const uint8_t *d) {
    if (!check_preamble(d, 0)) return false;
    if (!check_preamble(d, INDALA224_ENCODED_BIT_SIZE) &&
        !check_inv_preamble(d, INDALA224_ENCODED_BIT_SIZE)) return false;
    return true;
}
static bool feed_buf(bool pol, uint32_t t, uint8_t *d) {
    t += INDALA224_US_PER_BIT / 2;
    size_t n = t / INDALA224_US_PER_BIT;
    if (n >= INDALA224_ENCODED_BIT_SIZE) return false;
    for (size_t i = 0; i < n; i++) {
        i224_push(d, INDALA224_ENCODED_DATA_SIZE, pol);
        if (can_decode(d)) return true;
    }
    return false;
}
static void psk2_decode(const uint8_t *enc, uint8_t *dec) {
    memset(dec, 0, INDALA224_DECODED_DATA_SIZE);
    for (size_t i = 0; i < INDALA224_ENCODED_BIT_SIZE; i++)
        i224_set(dec, i, i224_get(enc, i) ^ i224_get(enc, i+1));
}

/*---------------------------------------------------------------------------*/
static void indala224_begin(void) {
    memset(g_enc,     0, sizeof(g_enc));
    memset(g_enc_neg, 0, sizeof(g_enc_neg));
    memset(g_dec,     0, sizeof(g_dec));
}
static bool indala224_execute(void *proto, uint16_t size) {
    lfrfid_evt_t *ev = (lfrfid_evt_t *)proto;
    for (int i = 0; i < size; i++) {
        bool     lvl = (ev[i].edge != 0);
        uint32_t dur = ev[i].t_us;
        if (dur <= (uint32_t)(INDALA224_US_PER_BIT / 2)) continue;
        if (feed_buf( lvl, dur, g_enc))     { psk2_decode(g_enc,     g_dec); goto hit; }
        if (feed_buf(!lvl, dur, g_enc_neg)) { psk2_decode(g_enc_neg, g_dec); goto hit; }
    }
    return false;
hit:
    memcpy(lfrfid_tag_info.uid, g_dec, min(sizeof(lfrfid_tag_info.uid), INDALA224_DECODED_DATA_SIZE));
    return true;
}
static uint8_t *indala224_get_data(void *p) { (void)p; return g_dec; }
static void indala224_render(void *p, char *r) {
    (void)p;
    sprintf(r,
        "Raw: %02X%02X%02X%02X\n"
        "     %02X%02X%02X%02X...",
        g_dec[0], g_dec[1], g_dec[2], g_dec[3],
        g_dec[4], g_dec[5], g_dec[6], g_dec[7]);
}

const LFRFIDProtocolBase protocol_indala224 = {
    .name        = "Indala224",
    .manufacturer= "Motorola",
    .data_size   = INDALA224_DECODED_DATA_SIZE,
    .features    = LFRFIDFeaturePSK,
    .get_data    = (lfrfidProtocolGetData)indala224_get_data,
    .decoder     = { .begin   = (lfrfidProtocolDecoderBegin)indala224_begin,
                     .execute = (lfrfidProtocolDecoderExecute)indala224_execute },
    .encoder     = { .begin = NULL, .send = NULL },
    .write       = { .begin = NULL, .send = NULL },
    .render_data = (lfrfidProtocolRenderData)indala224_render,
};
