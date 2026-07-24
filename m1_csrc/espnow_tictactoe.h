/* See COPYING.txt for license details. */

/**
 * @file   espnow_tictactoe.h
 * @brief  ESP-NOW Tic-Tac-Toe game logic — pure logic.
 *
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * Board is a 3×3 grid stored row-major (indices 0-8):
 *   0 | 1 | 2
 *   ---------
 *   3 | 4 | 5
 *   ---------
 *   6 | 7 | 8
 *
 * M1 Project
 */

#ifndef ESPNOW_TICTACTOE_H_
#define ESPNOW_TICTACTOE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Constants
 * =========================================================================*/

#define TTT_BOARD_SIZE  9
#define TTT_GAME_ID     0x01

/** Cell values. */
typedef enum {
    TTT_CELL_EMPTY = 0,
    TTT_CELL_X     = 1,
    TTT_CELL_O     = 2,
} ttt_cell_t;

/** Game result. */
typedef enum {
    TTT_RESULT_NONE      = 0,  /**< Game in progress */
    TTT_RESULT_DRAW      = 1,
    TTT_RESULT_X_WINS    = 2,
    TTT_RESULT_O_WINS    = 3,
} ttt_result_t;

/** Player role. */
typedef enum {
    TTT_ROLE_X = 0,  /**< Initiator plays X (goes first) */
    TTT_ROLE_O = 1,  /**< Responder plays O */
} ttt_role_t;

/* =========================================================================
 * Game state
 * =========================================================================*/

typedef struct {
    uint8_t     board[TTT_BOARD_SIZE];  /**< ttt_cell_t values */
    ttt_cell_t  current_turn;           /**< Whose turn (X or O) */
    ttt_role_t  our_role;               /**< Which side we play */
    ttt_result_t result;                /**< Current result */
    uint8_t     move_count;             /**< Total moves made */
} ttt_game_t;

/* =========================================================================
 * API
 * =========================================================================*/

/**
 * @brief  Initialise a new game.
 * @param  game  Game state to initialise.
 * @param  role  Our role (X = initiator, O = responder).
 */
void ttt_game_init(ttt_game_t *game, ttt_role_t role);

/**
 * @brief  Check if a move is valid.
 * @param  game  Game state.
 * @param  cell  Cell index (0-8).
 * @return true if the cell is empty and game is in progress.
 */
bool ttt_move_valid(const ttt_game_t *game, uint8_t cell);

/**
 * @brief  Apply a move to the board.
 * @param  game    Game state.
 * @param  cell    Cell index (0-8).
 * @param  player  Who is placing (X or O).
 * @return true if the move was applied successfully.
 */
bool ttt_apply_move(ttt_game_t *game, uint8_t cell, ttt_cell_t player);

/**
 * @brief  Check if it is our turn.
 */
bool ttt_is_our_turn(const ttt_game_t *game);

/**
 * @brief  Check the board for a winner or draw.
 *         Updates game->result.
 * @return The result after checking.
 */
ttt_result_t ttt_check_result(ttt_game_t *game);

/**
 * @brief  Get the cell value our role maps to.
 */
static inline ttt_cell_t ttt_our_cell(const ttt_game_t *game)
{
    return (game->our_role == TTT_ROLE_X) ? TTT_CELL_X : TTT_CELL_O;
}

/**
 * @brief  Get the cell value for the opponent.
 */
static inline ttt_cell_t ttt_their_cell(const ttt_game_t *game)
{
    return (game->our_role == TTT_ROLE_X) ? TTT_CELL_O : TTT_CELL_X;
}

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_TICTACTOE_H_ */
