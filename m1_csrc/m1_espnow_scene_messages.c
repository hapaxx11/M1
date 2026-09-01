/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_messages.c
 * @brief  ESP-NOW short-text peer messaging scene.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_espnow_scene.h"
#include "m1_espnow_scene_ctx.h"
#include "m1_scene.h"
#include "m1_espnow_hal.h"
#include "espnow_message.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"
#include "m1_virtual_kb.h"

#define MSG_POLL_INTERVAL_MS  100u
#define MSG_TEXT_UI_MAX       (M1_ESPNOW_SEND_PAYLOAD_MAX - ESPNOW_MSG_HDR_LEN)

static espnow_inbox_t s_inbox;
static uint8_t s_peer_mac[ESPNOW_MAC_LEN];
static char s_peer_name[M1_ESPNOW_PEER_NAME_MAX + 1u];
static uint8_t s_next_seq;
static uint32_t s_last_poll_tick;
static char s_status[24];
static bool s_have_peer;

static void messages_poll(M1SceneApp *app)
{
    uint8_t from_mac[ESPNOW_MAC_LEN];
    uint8_t frame[ESPNOW_MSG_HDR_LEN + ESPNOW_MSG_TEXT_MAX];
    uint8_t frame_len = 0;
    char text[ESPNOW_MSG_TEXT_MAX + 1u];
    uint8_t seq = 0;

    if (!m1_espnow_recv_msg(from_mac, frame, sizeof(frame), &frame_len))
        return;
    if (memcmp(from_mac, s_peer_mac, ESPNOW_MAC_LEN) != 0)
        return;
    if (!espnow_msg_parse(frame, frame_len, &seq, text, sizeof(text), NULL))
        return;
    if (espnow_inbox_is_duplicate(&s_inbox, from_mac, seq))
        return;

    espnow_inbox_push(&s_inbox, from_mac, seq, false, text);
    snprintf(s_status, sizeof(s_status), "Received");
    app->need_redraw = true;
}

static void messages_compose(M1SceneApp *app)
{
    char text[MSG_TEXT_UI_MAX + 1u];
    uint8_t frame[ESPNOW_MSG_HDR_LEN + MSG_TEXT_UI_MAX];
    size_t frame_len = 0;

    text[0] = '\0';
    if (m1_vkb_get_text("Message:", "", text, sizeof(text)) == 0u)
        return;

    for (int i = (int)strlen(text) - 1; i >= 0 && text[i] == ' '; --i)
        text[i] = '\0';
    if (text[0] == '\0')
        return;

    if (!espnow_msg_build(s_next_seq, text, frame, sizeof(frame), &frame_len)) {
        snprintf(s_status, sizeof(s_status), "Invalid text");
        app->need_redraw = true;
        return;
    }
    if (!m1_espnow_send(s_peer_mac, frame, frame_len)) {
        snprintf(s_status, sizeof(s_status), "Send failed");
        app->need_redraw = true;
        return;
    }

    espnow_inbox_push(&s_inbox, s_peer_mac, s_next_seq, true, text);
    s_next_seq++;
    snprintf(s_status, sizeof(s_status), "Sent");
    app->need_redraw = true;
}

static void messages_on_enter(M1SceneApp *app)
{
    s_have_peer = m1_espnow_scene_ctx_get_peer(
        s_peer_mac, s_peer_name, sizeof(s_peer_name));
    if (!s_have_peer)
        snprintf(s_status, sizeof(s_status), "Scan Peers first");
    else
        snprintf(s_status, sizeof(s_status), "OK: compose");
    espnow_inbox_init(&s_inbox);
    s_next_seq = 0;
    s_last_poll_tick = HAL_GetTick();
    app->need_redraw = true;
}

static bool messages_on_event(M1SceneApp *app, M1SceneEvent event)
{
    uint32_t now = HAL_GetTick();
    if (s_have_peer && (now - s_last_poll_tick) >= MSG_POLL_INTERVAL_MS) {
        s_last_poll_tick = now;
        messages_poll(app);
    }

    switch (event) {
    case M1SceneEventOk:
        if (s_have_peer)
            messages_compose(app);
        return true;

    case M1SceneEventBack:
        m1_scene_pop(app);
        return true;

    default:
        return false;
    }
}

static void messages_draw(M1SceneApp *app)
{
    (void)app;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Messages", TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, m1_menu_font());
    if (!s_have_peer) {
        m1_draw_text(&m1_u8g2, 2, 34, 120, "Pair with a peer first",
                     TEXT_ALIGN_CENTER);
    } else if (s_inbox.count == 0u) {
        char line[32];
        snprintf(line, sizeof(line), "Peer: %s", s_peer_name);
        m1_draw_text(&m1_u8g2, 2, 26, 120, line, TEXT_ALIGN_CENTER);
        m1_draw_text(&m1_u8g2, 2, 40, 120, "OK to compose",
                     TEXT_ALIGN_CENTER);
    } else {
        uint8_t visible = M1_MENU_VIS(s_inbox.count);
        uint8_t first = (uint8_t)(s_inbox.count - visible);
        uint8_t y = (uint8_t)(M1_MENU_AREA_TOP + m1_menu_item_h() - 1u);
        for (uint8_t row = 0; row < visible; ++row) {
            const espnow_msg_entry_t *e =
                espnow_inbox_get(&s_inbox, (uint8_t)(first + row));
            char line[32];
            snprintf(line, sizeof(line), "%s: %.24s",
                     (e && e->outgoing) ? "Me" : "Peer",
                     e ? e->text : "");
            u8g2_DrawStr(&m1_u8g2, 2, y, line);
            y = (uint8_t)(y + m1_menu_item_h());
        }
    }

    u8g2_SetFont(&m1_u8g2, u8g2_font_NokiaSmallPlain_tf);
    m1_draw_text(&m1_u8g2, 2, 62, 120, s_status, TEXT_ALIGN_CENTER);
    m1_u8g2_nextpage();
}

static void messages_on_exit(M1SceneApp *app) { (void)app; }

const M1SceneHandlers espnow_scene_messages_handlers = {
    .on_enter = messages_on_enter,
    .on_event = messages_on_event,
    .on_exit  = messages_on_exit,
    .draw     = messages_draw,
};
