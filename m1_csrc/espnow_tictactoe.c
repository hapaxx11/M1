/* See COPYING.txt for license details. */

/**
 * @file   espnow_tictactoe.c
 * @brief  ESP-NOW Tic-Tac-Toe game logic — pure logic.
 *
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * M1 Project
 */

#include "espnow_tictactoe.h"
#include <string.h>

/* =========================================================================
 * Win condition table
 * =========================================================================*/

static const uint8_t s_win_lines[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},  /* rows */
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},  /* columns */
    {0, 4, 8}, {2, 4, 6},             /* diagonals */
};

/* =========================================================================
 * Public API
 * =========================================================================*/

void ttt_game_init(ttt_game_t *game, ttt_role_t role)
{
    if (!game)
        return;
    memset(game, 0, sizeof(*game));
    game->our_role = role;
    game->current_turn = TTT_CELL_X;  /* X always goes first */
    game->result = TTT_RESULT_NONE;
}

bool ttt_move_valid(const ttt_game_t *game, uint8_t cell)
{
    if (!game || cell >= TTT_BOARD_SIZE)
        return false;
    if (game->result != TTT_RESULT_NONE)
        return false;
    return game->board[cell] == TTT_CELL_EMPTY;
}

bool ttt_apply_move(ttt_game_t *game, uint8_t cell, ttt_cell_t player)
{
    if (!game || cell >= TTT_BOARD_SIZE)
        return false;
    if (game->result != TTT_RESULT_NONE)
        return false;
    if (game->board[cell] != TTT_CELL_EMPTY)
        return false;
    if (player != game->current_turn)
        return false;

    game->board[cell] = (uint8_t)player;
    game->move_count++;

    /* Switch turn */
    game->current_turn = (player == TTT_CELL_X) ? TTT_CELL_O : TTT_CELL_X;

    /* Check for win/draw */
    ttt_check_result(game);
    return true;
}

bool ttt_is_our_turn(const ttt_game_t *game)
{
    if (!game || game->result != TTT_RESULT_NONE)
        return false;
    ttt_cell_t our = (game->our_role == TTT_ROLE_X) ? TTT_CELL_X : TTT_CELL_O;
    return game->current_turn == our;
}

ttt_result_t ttt_check_result(ttt_game_t *game)
{
    if (!game)
        return TTT_RESULT_NONE;

    /* Check all win lines */
    for (int i = 0; i < 8; i++) {
        uint8_t a = game->board[s_win_lines[i][0]];
        uint8_t b = game->board[s_win_lines[i][1]];
        uint8_t c = game->board[s_win_lines[i][2]];
        if (a != TTT_CELL_EMPTY && a == b && b == c) {
            game->result = (a == TTT_CELL_X)
                           ? TTT_RESULT_X_WINS : TTT_RESULT_O_WINS;
            return game->result;
        }
    }

    /* Check for draw (all cells filled) */
    if (game->move_count >= TTT_BOARD_SIZE) {
        game->result = TTT_RESULT_DRAW;
        return game->result;
    }

    game->result = TTT_RESULT_NONE;
    return TTT_RESULT_NONE;
}
