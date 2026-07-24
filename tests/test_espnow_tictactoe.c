/* See COPYING.txt for license details. */

/**
 * @file   test_espnow_tictactoe.c
 * @brief  Host-side unit tests for the ESP-NOW Tic-Tac-Toe game logic.
 *
 * Pure logic — no stubs required.
 */

#include "unity.h"
#include "espnow_tictactoe.h"

#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

/* =========================================================================
 * Init tests
 * =========================================================================*/

void test_init_empty_board(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    for (int i = 0; i < TTT_BOARD_SIZE; i++)
        TEST_ASSERT_EQUAL_UINT8(TTT_CELL_EMPTY, game.board[i]);
}

void test_init_x_goes_first(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_O);
    TEST_ASSERT_EQUAL(TTT_CELL_X, game.current_turn);
}

void test_init_no_result(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    TEST_ASSERT_EQUAL(TTT_RESULT_NONE, game.result);
}

/* =========================================================================
 * Move validation
 * =========================================================================*/

void test_move_valid_empty_cell(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    TEST_ASSERT_TRUE(ttt_move_valid(&game, 4));
}

void test_move_invalid_occupied_cell(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    ttt_apply_move(&game, 4, TTT_CELL_X);
    TEST_ASSERT_FALSE(ttt_move_valid(&game, 4));
}

void test_move_invalid_out_of_range(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    TEST_ASSERT_FALSE(ttt_move_valid(&game, 9));
}

/* =========================================================================
 * Apply move
 * =========================================================================*/

void test_apply_move_places_piece(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    TEST_ASSERT_TRUE(ttt_apply_move(&game, 0, TTT_CELL_X));
    TEST_ASSERT_EQUAL_UINT8(TTT_CELL_X, game.board[0]);
}

void test_apply_move_switches_turn(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    ttt_apply_move(&game, 0, TTT_CELL_X);
    TEST_ASSERT_EQUAL(TTT_CELL_O, game.current_turn);
}

void test_apply_move_wrong_turn_fails(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    /* O tries to go first */
    TEST_ASSERT_FALSE(ttt_apply_move(&game, 0, TTT_CELL_O));
}

void test_apply_move_occupied_fails(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    ttt_apply_move(&game, 0, TTT_CELL_X);
    TEST_ASSERT_FALSE(ttt_apply_move(&game, 0, TTT_CELL_O));
}

/* =========================================================================
 * Win detection
 * =========================================================================*/

void test_x_wins_row(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    /* X: 0,1,2  O: 3,4 */
    ttt_apply_move(&game, 0, TTT_CELL_X);
    ttt_apply_move(&game, 3, TTT_CELL_O);
    ttt_apply_move(&game, 1, TTT_CELL_X);
    ttt_apply_move(&game, 4, TTT_CELL_O);
    ttt_apply_move(&game, 2, TTT_CELL_X);
    TEST_ASSERT_EQUAL(TTT_RESULT_X_WINS, game.result);
}

void test_o_wins_column(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_O);
    /* X: 0,1,3  O: 2,5,8 */
    ttt_apply_move(&game, 0, TTT_CELL_X);
    ttt_apply_move(&game, 2, TTT_CELL_O);
    ttt_apply_move(&game, 1, TTT_CELL_X);
    ttt_apply_move(&game, 5, TTT_CELL_O);
    ttt_apply_move(&game, 3, TTT_CELL_X);
    ttt_apply_move(&game, 8, TTT_CELL_O);
    TEST_ASSERT_EQUAL(TTT_RESULT_O_WINS, game.result);
}

void test_x_wins_diagonal(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    /* X: 0,4,8  O: 1,2 */
    ttt_apply_move(&game, 0, TTT_CELL_X);
    ttt_apply_move(&game, 1, TTT_CELL_O);
    ttt_apply_move(&game, 4, TTT_CELL_X);
    ttt_apply_move(&game, 2, TTT_CELL_O);
    ttt_apply_move(&game, 8, TTT_CELL_X);
    TEST_ASSERT_EQUAL(TTT_RESULT_X_WINS, game.result);
}

void test_draw(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    /* X O X
       X X O
       O X O */
    ttt_apply_move(&game, 0, TTT_CELL_X);
    ttt_apply_move(&game, 1, TTT_CELL_O);
    ttt_apply_move(&game, 2, TTT_CELL_X);
    ttt_apply_move(&game, 5, TTT_CELL_O);
    ttt_apply_move(&game, 3, TTT_CELL_X);
    ttt_apply_move(&game, 6, TTT_CELL_O);
    ttt_apply_move(&game, 4, TTT_CELL_X);
    ttt_apply_move(&game, 8, TTT_CELL_O);
    ttt_apply_move(&game, 7, TTT_CELL_X);
    TEST_ASSERT_EQUAL(TTT_RESULT_DRAW, game.result);
}

/* =========================================================================
 * Is our turn
 * =========================================================================*/

void test_is_our_turn_x_first(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    TEST_ASSERT_TRUE(ttt_is_our_turn(&game));
}

void test_is_our_turn_o_not_first(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_O);
    TEST_ASSERT_FALSE(ttt_is_our_turn(&game));
}

void test_is_our_turn_after_game_over(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    /* Quick X win */
    ttt_apply_move(&game, 0, TTT_CELL_X);
    ttt_apply_move(&game, 3, TTT_CELL_O);
    ttt_apply_move(&game, 1, TTT_CELL_X);
    ttt_apply_move(&game, 4, TTT_CELL_O);
    ttt_apply_move(&game, 2, TTT_CELL_X);
    TEST_ASSERT_FALSE(ttt_is_our_turn(&game));
}

/* =========================================================================
 * Helpers
 * =========================================================================*/

void test_our_cell_x(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    TEST_ASSERT_EQUAL(TTT_CELL_X, ttt_our_cell(&game));
    TEST_ASSERT_EQUAL(TTT_CELL_O, ttt_their_cell(&game));
}

void test_our_cell_o(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_O);
    TEST_ASSERT_EQUAL(TTT_CELL_O, ttt_our_cell(&game));
    TEST_ASSERT_EQUAL(TTT_CELL_X, ttt_their_cell(&game));
}

/* =========================================================================
 * No moves after game over
 * =========================================================================*/

void test_no_move_after_win(void)
{
    ttt_game_t game;
    ttt_game_init(&game, TTT_ROLE_X);
    ttt_apply_move(&game, 0, TTT_CELL_X);
    ttt_apply_move(&game, 3, TTT_CELL_O);
    ttt_apply_move(&game, 1, TTT_CELL_X);
    ttt_apply_move(&game, 4, TTT_CELL_O);
    ttt_apply_move(&game, 2, TTT_CELL_X); /* X wins */
    TEST_ASSERT_FALSE(ttt_apply_move(&game, 6, TTT_CELL_O));
}

/* =========================================================================
 * Test runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_empty_board);
    RUN_TEST(test_init_x_goes_first);
    RUN_TEST(test_init_no_result);

    RUN_TEST(test_move_valid_empty_cell);
    RUN_TEST(test_move_invalid_occupied_cell);
    RUN_TEST(test_move_invalid_out_of_range);

    RUN_TEST(test_apply_move_places_piece);
    RUN_TEST(test_apply_move_switches_turn);
    RUN_TEST(test_apply_move_wrong_turn_fails);
    RUN_TEST(test_apply_move_occupied_fails);

    RUN_TEST(test_x_wins_row);
    RUN_TEST(test_o_wins_column);
    RUN_TEST(test_x_wins_diagonal);
    RUN_TEST(test_draw);

    RUN_TEST(test_is_our_turn_x_first);
    RUN_TEST(test_is_our_turn_o_not_first);
    RUN_TEST(test_is_our_turn_after_game_over);

    RUN_TEST(test_our_cell_x);
    RUN_TEST(test_our_cell_o);

    RUN_TEST(test_no_move_after_win);

    return UNITY_END();
}
