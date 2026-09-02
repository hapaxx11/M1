/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_tictactoe.c
 * @brief  ESP-NOW Tic-Tac-Toe game scene.
 *
 * Uses the espnow_tictactoe pure-logic module for game state and
 * m1_espnow_hal for sending/receiving moves over ESP-NOW.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_espnow_scene.h"
#include "m1_espnow_scene_ctx.h"
#include "m1_scene.h"
#include "m1_espnow_hal.h"
#include "m1_espnow_secure_link.h"
#include "espnow_appmsg.h"
#include "espnow_tictactoe.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Game scene state                                                         */
/*==========================================================================*/

static ttt_game_t s_game;
static uint8_t s_cursor;  /* 0-8 board position */
static uint8_t s_peer_mac[ESPNOW_MAC_LEN];
static uint32_t s_last_poll_tick;
static bool s_game_active;

#define TTT_POLL_INTERVAL_MS  100

/* Wire message: [game_id=0x01][move_cell:1] */
#define TTT_MSG_MOVE    (ESPNOW_APP_GAME_BASE + 0u)
#define TTT_MSG_RESIGN  (ESPNOW_APP_GAME_BASE + 1u)

/*==========================================================================*/
/* Tic-Tac-Toe scene — on_enter                                             */
/*==========================================================================*/

static void ttt_on_enter(M1SceneApp *app)
{
    /* Initiator plays X. */
    ttt_game_init(&s_game, TTT_ROLE_X);
    s_cursor = 4;  /* center */
    s_game_active = true;
    s_last_poll_tick = HAL_GetTick();
    if (!m1_espnow_scene_ctx_get_peer(s_peer_mac, NULL, 0))
        memset(s_peer_mac, 0xFF, ESPNOW_MAC_LEN);
    app->need_redraw = true;
}

/*==========================================================================*/
/* Tic-Tac-Toe scene — on_event                                             */
/*==========================================================================*/

static bool ttt_on_event(M1SceneApp *app, M1SceneEvent event)
{
    /* Poll for opponent moves */
    uint32_t now = HAL_GetTick();
    if ((now - s_last_poll_tick) >= TTT_POLL_INTERVAL_MS) {
        s_last_poll_tick = now;

        uint8_t from_mac[ESPNOW_MAC_LEN];
        uint8_t msg_buf[8];
        uint8_t msg_len = 0;
        if (m1_espnow_secure_link_recv(from_mac, msg_buf, sizeof(msg_buf),
                                       &msg_len)) {
            if (msg_len >= 2 && msg_buf[0] == TTT_MSG_MOVE) {
                uint8_t cell = msg_buf[1];
                ttt_cell_t their = ttt_their_cell(&s_game);
                ttt_apply_move(&s_game, cell, their);
                ttt_check_result(&s_game);
                app->need_redraw = true;
            } else if (msg_len >= 1 && msg_buf[0] == TTT_MSG_RESIGN) {
                s_game.result = (s_game.our_role == TTT_ROLE_X)
                                ? TTT_RESULT_X_WINS : TTT_RESULT_O_WINS;
                s_game_active = false;
                app->need_redraw = true;
            }
        }
    }

    /* Don't process input if game over */
    if (s_game.result != TTT_RESULT_NONE && event != M1SceneEventBack) {
        return false;
    }

    switch (event) {
    case M1SceneEventUp:
        if (s_cursor >= 3) { s_cursor -= 3; app->need_redraw = true; }
        return true;

    case M1SceneEventDown:
        if (s_cursor <= 5) { s_cursor += 3; app->need_redraw = true; }
        return true;

    case M1SceneEventLeft:
        if (s_cursor % 3 > 0) { s_cursor--; app->need_redraw = true; }
        return true;

    case M1SceneEventRight:
        if (s_cursor % 3 < 2) { s_cursor++; app->need_redraw = true; }
        return true;

    case M1SceneEventOk:
        if (ttt_is_our_turn(&s_game) && ttt_move_valid(&s_game, s_cursor)) {
            ttt_cell_t ours = ttt_our_cell(&s_game);
            ttt_apply_move(&s_game, s_cursor, ours);
            ttt_check_result(&s_game);

            /* Send move to opponent */
            uint8_t move_msg[2] = { TTT_MSG_MOVE, s_cursor };
            m1_espnow_secure_link_send(s_peer_mac, move_msg, 2);
            app->need_redraw = true;
        }
        return true;

    case M1SceneEventBack:
        if (s_game_active && s_game.result == TTT_RESULT_NONE) {
            /* Resign */
            uint8_t resign_msg[1] = { TTT_MSG_RESIGN };
            m1_espnow_secure_link_send(s_peer_mac, resign_msg, 1);
        }
        m1_scene_pop(app);
        return true;

    default:
        return false;
    }
}

/*==========================================================================*/
/* Tic-Tac-Toe scene — draw                                                 */
/*==========================================================================*/

static void ttt_draw(M1SceneApp *app)
{
    (void)app;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Draw grid centred on display (64x64 area) */
    /* Board origin at (36, 4), cell size 16x16 */
    #define BOARD_X  36
    #define BOARD_Y  4
    #define CELL_SZ  16

    /* Vertical lines */
    u8g2_DrawVLine(&m1_u8g2, BOARD_X + CELL_SZ,     BOARD_Y, CELL_SZ * 3);
    u8g2_DrawVLine(&m1_u8g2, BOARD_X + CELL_SZ * 2, BOARD_Y, CELL_SZ * 3);
    /* Horizontal lines */
    u8g2_DrawHLine(&m1_u8g2, BOARD_X, BOARD_Y + CELL_SZ,     CELL_SZ * 3);
    u8g2_DrawHLine(&m1_u8g2, BOARD_X, BOARD_Y + CELL_SZ * 2, CELL_SZ * 3);

    /* Draw X/O markers and cursor */
    for (uint8_t i = 0; i < TTT_BOARD_SIZE; i++) {
        uint8_t row = i / 3;
        uint8_t col = i % 3;
        uint8_t cx = BOARD_X + col * CELL_SZ + CELL_SZ / 2;
        uint8_t cy = BOARD_Y + row * CELL_SZ + CELL_SZ / 2;

        if (s_game.board[i] == TTT_CELL_X) {
            /* Draw X as two diagonals */
            u8g2_DrawLine(&m1_u8g2, cx - 4, cy - 4, cx + 4, cy + 4);
            u8g2_DrawLine(&m1_u8g2, cx - 4, cy + 4, cx + 4, cy - 4);
        } else if (s_game.board[i] == TTT_CELL_O) {
            /* Draw O as circle */
            u8g2_DrawCircle(&m1_u8g2, cx, cy, 5, U8G2_DRAW_ALL);
        }

        /* Cursor highlight */
        if (i == s_cursor && ttt_is_our_turn(&s_game) &&
            s_game.result == TTT_RESULT_NONE) {
            u8g2_DrawFrame(&m1_u8g2,
                           BOARD_X + col * CELL_SZ + 1,
                           BOARD_Y + row * CELL_SZ + 1,
                           CELL_SZ - 2, CELL_SZ - 2);
        }
    }

    /* Status line at bottom */
    const char *status;
    if (s_game.result == TTT_RESULT_X_WINS)
        status = (s_game.our_role == TTT_ROLE_X) ? "You Win!" : "You Lose";
    else if (s_game.result == TTT_RESULT_O_WINS)
        status = (s_game.our_role == TTT_ROLE_O) ? "You Win!" : "You Lose";
    else if (s_game.result == TTT_RESULT_DRAW)
        status = "Draw!";
    else if (ttt_is_our_turn(&s_game))
        status = "Your Turn";
    else
        status = "Opponent's Turn";

    u8g2_SetFont(&m1_u8g2, u8g2_font_NokiaSmallPlain_tf);
    m1_draw_text(&m1_u8g2, 2, 60, 120, status, TEXT_ALIGN_CENTER);
    m1_u8g2_nextpage();
}

static void ttt_on_exit(M1SceneApp *app) { (void)app; s_game_active = false; }

const M1SceneHandlers espnow_scene_tictactoe_handlers = {
    .on_enter = ttt_on_enter,
    .on_event = ttt_on_event,
    .on_exit  = ttt_on_exit,
    .draw     = ttt_draw,
};
