/*
 * compat.h — Flipper Zero -> Monstatek M1 compatibility shim.
 *
 * ZeroMesh is a Flipper app: it draws on a furi `Canvas`, reads `InputEvent`s,
 * logs through FURI_LOG, and uses furi mutex / tick / RNG helpers. The M1 app
 * SDK (m1app.h) offers u8g2 drawing, a single-shot button poll, a FreeRTOS
 * heap and libc-ish helpers, but none of the furi surface.
 *
 * This header bridges the two so the protocol and GUI code port with only
 * mechanical edits. It maps:
 *   - Canvas*            -> u8g2_t*        (the M1 shares one display context)
 *   - canvas_* draw ops  -> u8g2_* / m1_* draw ops
 *   - InputEvent / keys  -> synthesised from game_poll_button + long-press
 *   - furi_get_tick/delay -> m1app_get_tick / m1app_delay (both milliseconds)
 *   - furi mutex          -> no-ops (the M1 app is a single cooperative task)
 *   - furi_hal_random_*   -> rand() / m1_crypto_generate_iv
 *   - FURI_LOG_*          -> dropped
 *
 * The app is single-threaded: RX draining, dispatch and rendering all run in
 * one loop, so the mutexes ZeroMesh used to guard its RX thread are unneeded
 * and compile away to nothing.
 */
#pragma once

#include "m1app.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

/* vsnprintf is variadic like the already-exported snprintf; the firmware API
 * table exports it (see firmware/api_additions). va_list is a compiler
 * builtin, so no symbol is needed for it. */
extern int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

/* ---- logging: dropped on-device ---- */
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_T(...) ((void)0)

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

/* ---- timing (both sides are milliseconds) ---- */
#define furi_get_tick() m1app_get_tick()
#define furi_delay_ms(ms) m1app_delay((uint32_t)(ms))

/* ---- mutex: single task, so these are no-ops ---- */
typedef int FuriMutex;
typedef int FuriMutexType;
#define FuriMutexTypeNormal 0
#define FuriWaitForever 0xFFFFFFFFu
#define FuriStatusOk 0
#define furi_mutex_alloc(t) (0)
#define furi_mutex_free(m) ((void)0)
#define furi_mutex_acquire(m, t) ((void)0)
#define furi_mutex_release(m) ((void)0)

/* ---- RNG ---- */
static inline uint32_t furi_hal_random_get(void) {
    /* rand() may be only 15-bit; fold three draws into 32 bits. */
    return ((uint32_t)rand() << 20) ^ ((uint32_t)rand() << 8) ^ (uint32_t)rand();
}
static inline void furi_hal_random_fill_buf(uint8_t *buf, size_t len) {
    /* Use the hardware-backed IV generator where we can (channel PSK), and
     * top up any tail with rand(). */
    size_t i = 0;
    while(i + M1APP_AES_IV_SIZE <= len) {
        m1_crypto_generate_iv(buf + i);
        i += M1APP_AES_IV_SIZE;
    }
    for(; i < len; i++) buf[i] = (uint8_t)rand();
}

/* ---- small libc helpers not in the export table ---- */
static inline size_t strlcpy(char *dst, const char *src, size_t size) {
    size_t slen = strlen(src);
    if(size) {
        size_t copy = (slen < size - 1) ? slen : size - 1;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return slen;
}

/* ==================================================================
 *  Canvas shim  (furi Canvas -> u8g2)
 * ================================================================== */

typedef u8g2_t Canvas;

typedef enum {
    ColorWhite = 0,
    ColorBlack = 1,
    ColorXOR = 2,
} Color;

typedef enum {
    FontPrimary = 0,   /* bold header font */
    FontSecondary,     /* small body font */
    FontKeyboard,
    FontBigNumbers,
} Font;

/* The M1 is a 128x64 mono panel, same geometry as the Flipper. On both,
 * "draw color 1" lights a dark pixel, so ColorBlack -> 1, ColorWhite -> 0. */
static inline void canvas_set_color(Canvas *c, Color col) {
    u8g2_SetDrawColor(c, col == ColorWhite ? 0 : (col == ColorXOR ? 2 : 1));
}

static inline void canvas_set_font(Canvas *c, Font f) {
    switch(f) {
    case FontPrimary:
        u8g2_SetFont(c, u8g2_font_helvB08_tr);
        break;
    case FontSecondary:
    default:
        u8g2_SetFont(c, u8g2_font_5x8_tr);
        break;
    }
}

static inline void canvas_draw_str(Canvas *c, int x, int y, const char *s) {
    if(s) u8g2_DrawStr(c, (u8g2_uint_t)x, (u8g2_uint_t)y, s);
}
static inline uint16_t canvas_string_width(Canvas *c, const char *s) {
    return s ? u8g2_GetStrWidth(c, s) : 0;
}
static inline void canvas_draw_box(Canvas *c, int x, int y, int w, int h) {
    u8g2_DrawBox(c, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w, (u8g2_uint_t)h);
}
static inline void canvas_draw_frame(Canvas *c, int x, int y, int w, int h) {
    u8g2_DrawFrame(c, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w, (u8g2_uint_t)h);
}
static inline void canvas_draw_rbox(Canvas *c, int x, int y, int w, int h, int r) {
    u8g2_DrawRBox(c, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w, (u8g2_uint_t)h, (u8g2_uint_t)r);
}
static inline void canvas_draw_rframe(Canvas *c, int x, int y, int w, int h, int r) {
    u8g2_DrawRFrame(c, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w, (u8g2_uint_t)h, (u8g2_uint_t)r);
}
static inline void canvas_draw_line(Canvas *c, int x0, int y0, int x1, int y1) {
    u8g2_DrawLine(c, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)x1, (u8g2_uint_t)y1);
}
static inline void canvas_draw_dot(Canvas *c, int x, int y) {
    u8g2_DrawPixel(c, (u8g2_uint_t)x, (u8g2_uint_t)y);
}
static inline void canvas_draw_disc(Canvas *c, int x, int y, int r) {
    u8g2_DrawDisc(c, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)r, U8G2_DRAW_ALL);
}
static inline void canvas_draw_circle(Canvas *c, int x, int y, int r) {
    u8g2_DrawCircle(c, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)r, U8G2_DRAW_ALL);
}
static inline int canvas_width(Canvas *c) {
    (void)c;
    return 128;
}
static inline int canvas_height(Canvas *c) {
    (void)c;
    return 64;
}
/* In the M1's paged draw loop each page starts blank, so an explicit clear is
 * a no-op. */
static inline void canvas_clear(Canvas *c) {
    (void)c;
}

/* ==================================================================
 *  Input shim
 * ================================================================== */

typedef enum {
    InputKeyUp = 0,
    InputKeyDown,
    InputKeyRight,
    InputKeyLeft,
    InputKeyOk,
    InputKeyBack,
    InputKeyMAX,
} InputKey;

typedef enum {
    InputTypePress = 0,
    InputTypeRelease,
    InputTypeShort,
    InputTypeLong,
    InputTypeRepeat,
} InputType;

typedef struct {
    InputKey key;
    InputType type;
} InputEvent;

/* Extended button poll added to the firmware API table: returns the button
 * plus its event class (1=click, 2=double-click, 3=long-click). Declared here
 * so every module sees it. */
#define M1_BTN_EVT_NONE   0
#define M1_BTN_EVT_CLICK  1
#define M1_BTN_EVT_DBL    2
#define M1_BTN_EVT_LONG   3
extern m1app_button_t m1_poll_button_ex(uint32_t timeout_ms, uint8_t *out_evt);

extern uint8_t m1_menu_item_h(void);
extern uint8_t m1_menu_max_visible(void);
extern const uint8_t* m1_menu_font(void);
