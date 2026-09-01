/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_scan.c
 * @brief  ESP-NOW peer discovery scan list and pairing scenes.
 *
 * Uses the espnow_peer_session pure-logic state machine for peer tracking
 * and the m1_espnow_hal transport layer for ESP-NOW RPC communication.
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
#include "m1_espnow_secure_link.h"
#include "espnow_peer_session.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Scan scene state                                                         */
/*==========================================================================*/

static espnow_session_t s_session;
static uint8_t s_sel;
static uint8_t s_scroll;
static uint32_t s_last_poll_tick;

#define SCAN_POLL_INTERVAL_MS  500

/*==========================================================================*/
/* Scan scene — on_enter                                                    */
/*==========================================================================*/

static void scan_on_enter(M1SceneApp *app)
{
    uint8_t mac[ESPNOW_MAC_LEN];

    if (!m1_espnow_start(m1_espnow_get_channel())) {
        app->need_redraw = true;
        m1_scene_pop(app);
        return;
    }

    m1_espnow_get_mac(mac);
    espnow_session_init(&s_session, "M1", mac, m1_espnow_get_channel());
    espnow_session_start_scan(&s_session);
    m1_espnow_announce();
    s_sel = 0;
    s_scroll = 0;
    s_last_poll_tick = HAL_GetTick();
    app->need_redraw = true;
}

/*==========================================================================*/
/* Scan scene — on_event                                                    */
/*==========================================================================*/

static bool scan_on_event(M1SceneApp *app, M1SceneEvent event)
{
    /* Periodic peer poll */
    uint32_t now = HAL_GetTick();
    if ((now - s_last_poll_tick) >= SCAN_POLL_INTERVAL_MS) {
        s_last_poll_tick = now;
        espnow_peer_info_t peers[ESPNOW_MAX_PEERS];
        uint8_t count = m1_espnow_poll_peers(peers, ESPNOW_MAX_PEERS);
        if (count > 0) {
            espnow_session_update_peers(&s_session, peers, count);
            app->need_redraw = true;
        }
    }

    switch (event) {
    case M1SceneEventUp:
        if (s_session.peer_count > 0 && s_sel > 0) {
            s_sel--;
            if (s_sel < s_scroll) s_scroll = s_sel;
            app->need_redraw = true;
        }
        return true;

    case M1SceneEventDown:
        if (s_session.peer_count > 0 && s_sel < (s_session.peer_count - 1)) {
            s_sel++;
            uint8_t vis = 4;  /* max visible peers in scan list */
            if (s_sel >= s_scroll + vis) s_scroll = s_sel - vis + 1;
            app->need_redraw = true;
        }
        return true;

    case M1SceneEventOk:
        if (s_session.peer_count > 0) {
            espnow_session_select_peer(&s_session, s_sel);
            /* Send pair request */
            uint8_t pair_msg[1] = { ESPNOW_MSG_PAIR_REQUEST };
            m1_espnow_send(s_session.peers[s_sel].mac, pair_msg, 1);
            espnow_session_pair_request_sent(&s_session);
            m1_scene_push(app, EspnowScenePair);
        }
        return true;

    case M1SceneEventBack:
        espnow_session_stop(&s_session);
        m1_scene_pop(app);
        return true;

    default:
        return false;
    }
}

/*==========================================================================*/
/* Scan scene — draw                                                        */
/*==========================================================================*/

static void scan_draw(M1SceneApp *app)
{
    (void)app;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Scanning...", TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, u8g2_font_NokiaSmallPlain_tf);

    if (s_session.peer_count == 0) {
        u8g2_DrawStr(&m1_u8g2, 20, 36, "No peers found");
    } else {
        uint8_t vis = 4;
        uint8_t y = 22;
        for (uint8_t i = s_scroll; i < s_session.peer_count && i < s_scroll + vis; i++) {
            bool highlight = (i == s_sel);
            char line[32];
            snprintf(line, sizeof(line), "%s  %ddBm",
                     s_session.peers[i].name[0] ? s_session.peers[i].name : "M1",
                     s_session.peers[i].rssi);
            if (highlight) {
                u8g2_DrawRBox(&m1_u8g2, 1, y - 8, 122, 10, 2);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            }
            u8g2_DrawStr(&m1_u8g2, 4, y, line);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            y += 12;
        }
    }
    m1_u8g2_nextpage();
}

static void scan_on_exit(M1SceneApp *app) { (void)app; }

const M1SceneHandlers espnow_scene_scan_handlers = {
    .on_enter = scan_on_enter,
    .on_event = scan_on_event,
    .on_exit  = scan_on_exit,
    .draw     = scan_draw,
};

/*==========================================================================*/
/* Pair scene — waiting for PAIR_ACCEPT / PAIR_REJECT                       */
/*==========================================================================*/

static uint32_t s_pair_start_tick;
#define PAIR_TIMEOUT_MS  5000

static void pair_on_enter(M1SceneApp *app)
{
    s_pair_start_tick = HAL_GetTick();
    app->need_redraw = true;
}

static bool pair_on_event(M1SceneApp *app, M1SceneEvent event)
{
    /* Check for incoming pair response */
    uint8_t msg_buf[8];
    uint8_t msg_len = 0;
    uint8_t from_mac[ESPNOW_MAC_LEN];
    if (m1_espnow_recv_msg(from_mac, msg_buf, sizeof(msg_buf), &msg_len)) {
        if (msg_len >= 1 && msg_buf[0] == ESPNOW_MSG_PAIR_ACCEPT) {
            espnow_session_pair_accepted(&s_session);
            if (s_session.selected_peer_idx < s_session.peer_count) {
                uint8_t local_mac[ESPNOW_MAC_LEN];
                const espnow_peer_info_t *peer =
                    &s_session.peers[s_session.selected_peer_idx];
                m1_espnow_get_mac(local_mac);
                m1_espnow_scene_ctx_set_peer(
                    peer->mac, peer->name);
                (void)m1_espnow_secure_link_configure(
                    local_mac, peer->mac, s_session.confirm_code);
            }
            app->need_redraw = true;
            /* Stay on pair screen showing confirm code briefly,
             * then pop back.  For now just pop. */
            m1_scene_pop(app);
            return true;
        } else if (msg_len >= 1 && msg_buf[0] == ESPNOW_MSG_PAIR_REJECT) {
            espnow_session_pair_rejected(&s_session);
            espnow_session_ack_rejection(&s_session);
            m1_scene_pop(app);
            return true;
        }
    }

    /* Timeout check */
    if ((HAL_GetTick() - s_pair_start_tick) >= PAIR_TIMEOUT_MS) {
        espnow_session_stop(&s_session);
        espnow_session_init(&s_session, "M1", s_session.our_mac,
                            s_session.our_channel);
        espnow_session_start_scan(&s_session);
        m1_scene_pop(app);
        return true;
    }

    if (event == M1SceneEventBack) {
        espnow_session_stop(&s_session);
        espnow_session_init(&s_session, "M1", s_session.our_mac,
                            s_session.our_channel);
        espnow_session_start_scan(&s_session);
        m1_scene_pop(app);
        return true;
    }

    return false;
}

static void pair_draw(M1SceneApp *app)
{
    (void)app;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Pairing...", TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, u8g2_font_NokiaSmallPlain_tf);
    const char *peer_name = s_session.peers[s_session.selected_peer_idx].name;
    char line[32];
    snprintf(line, sizeof(line), "-> %s",
             peer_name[0] ? peer_name : "M1");
    m1_draw_text(&m1_u8g2, 2, 28, 120, line, TEXT_ALIGN_CENTER);
    m1_draw_text(&m1_u8g2, 2, 44, 120, "Waiting for response", TEXT_ALIGN_CENTER);
    m1_u8g2_nextpage();
}

static void pair_on_exit(M1SceneApp *app) { (void)app; }

const M1SceneHandlers espnow_scene_pair_handlers = {
    .on_enter = pair_on_enter,
    .on_event = pair_on_event,
    .on_exit  = pair_on_exit,
    .draw     = pair_draw,
};
