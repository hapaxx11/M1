/* See COPYING.txt for license details. */

/*
 *
 * game_2048.c
 *
 * 2048 puzzle game for M1
 *
 * 4x4 grid, D-pad to slide tiles, OK to restart, BACK to exit.
 * Score and best score are tracked for the session.
 *
 * Ported from dagnazty/M1_T-1000.
 *
 * M1 Project
 *
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "m1_games.h"

/*************************** D E F I N E S ************************************/

#define G_SIZE          4           /* 4x4 grid */
#define G_CELLS         (G_SIZE * G_SIZE)

/* Layout: the grid occupies the left portion of the 128x64 screen.
 * Each cell is 15x14 px with 1 px gaps, yielding a 63x59 px grid
 * starting at (1,4). The right panel (x=66..127) shows score/best/hints. */
#define CELL_W          15
#define CELL_H          14
#define CELL_GAP        1
#define GRID_X          1
#define GRID_Y          4
#define PANEL_X         66

//************************** S T R U C T U R E S *******************************

typedef struct {
    uint16_t tile[G_SIZE][G_SIZE]; /* 0 = empty, else power-of-2 value */
    uint32_t score;
    uint32_t best;
    bool     won;           /* hit 2048 once */
    bool     game_over;
} state_2048_t;

/***************************** V A R I A B L E S ******************************/

static state_2048_t g_state;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static void     g2048_init(state_2048_t *s);
static bool     g2048_add_random_tile(state_2048_t *s);
static bool     g2048_slide_left(state_2048_t *s);
static bool     g2048_slide_right(state_2048_t *s);
static bool     g2048_slide_up(state_2048_t *s);
static bool     g2048_slide_down(state_2048_t *s);
static bool     g2048_has_moves(const state_2048_t *s);
static void     g2048_draw(const state_2048_t *s);
static void     g2048_draw_cell(uint8_t col, uint8_t row, uint16_t val);
static void     g2048_draw_title(void);
static void     g2048_draw_game_over(const state_2048_t *s);
static void     g2048_draw_you_win(const state_2048_t *s);
static void     g2048_uint_to_str(uint32_t val, char *buf, uint8_t bufsize);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/*============================================================================*/
/*
 * @brief  Convert an unsigned integer to a decimal string (no snprintf).
 */
/*============================================================================*/
static void g2048_uint_to_str(uint32_t val, char *buf, uint8_t bufsize)
{
    if (bufsize == 0) return;
    if (val == 0)
    {
        if (bufsize < 2) { buf[0] = '\0'; return; }
        buf[0] = '0';
        if (bufsize > 1) buf[1] = '\0';
        return;
    }
    char tmp[12];
    uint8_t pos = 0;
    while (val > 0 && pos < (uint8_t)(sizeof(tmp) - 1))
    {
        tmp[pos++] = (char)('0' + (val % 10));
        val /= 10;
    }
    uint8_t out = 0;
    for (int8_t i = (int8_t)(pos - 1); i >= 0 && out < bufsize - 1; i--)
    {
        buf[out++] = tmp[i];
    }
    buf[out] = '\0';
}


/*============================================================================*/
/*
 * @brief  Initialise or reset a game state.
 */
/*============================================================================*/
static void g2048_init(state_2048_t *s)
{
    uint32_t best = s->best;        /* preserve best across restarts */
    memset(s, 0, sizeof(*s));
    s->best = best;
    game_rand_seed();
    g2048_add_random_tile(s);
    g2048_add_random_tile(s);
}


/*============================================================================*/
/*
 * @brief  Place a new tile (90% = 2, 10% = 4) in a random empty cell.
 * @retval true if a tile was placed, false if no empty cell exists.
 */
/*============================================================================*/
static bool g2048_add_random_tile(state_2048_t *s)
{
    /* Collect empty positions */
    uint8_t empty_r[G_CELLS];
    uint8_t empty_c[G_CELLS];
    uint8_t count = 0;

    for (uint8_t r = 0; r < G_SIZE; r++)
    {
        for (uint8_t c = 0; c < G_SIZE; c++)
        {
            if (s->tile[r][c] == 0)
            {
                empty_r[count] = r;
                empty_c[count] = c;
                count++;
            }
        }
    }

    if (count == 0) return false;

    uint8_t idx = (uint8_t)game_rand_range(0, count - 1);
    uint16_t val = (game_rand_range(0, 9) < 9) ? 2 : 4;
    s->tile[empty_r[idx]][empty_c[idx]] = val;
    return true;
}


/*============================================================================*/
/*
 * @brief  Slide one row left, merging equal adjacent tiles.
 *         Returns true if any tile moved or merged.
 */
/*============================================================================*/
static bool g2048_slide_row_left(uint16_t row[G_SIZE], uint32_t *score_add)
{
    bool moved = false;
    uint16_t tmp[G_SIZE];
    uint8_t pos = 0;

    /* Pack non-zero tiles to the left */
    for (uint8_t c = 0; c < G_SIZE; c++)
    {
        if (row[c] != 0)
        {
            tmp[pos++] = row[c];
        }
    }
    while (pos < G_SIZE) tmp[pos++] = 0;

    /* Merge adjacent equal tiles */
    for (uint8_t c = 0; c < G_SIZE - 1; c++)
    {
        if (tmp[c] != 0 && tmp[c] == tmp[c + 1])
        {
            tmp[c] = (uint16_t)(tmp[c] * 2);
            *score_add += tmp[c];
            tmp[c + 1] = 0;
            moved = true;
        }
    }

    /* Re-pack after merge */
    pos = 0;
    uint16_t out[G_SIZE] = {0};
    for (uint8_t c = 0; c < G_SIZE; c++)
    {
        if (tmp[c] != 0) out[pos++] = tmp[c];
    }

    /* Detect movement */
    for (uint8_t c = 0; c < G_SIZE; c++)
    {
        if (row[c] != out[c]) moved = true;
        row[c] = out[c];
    }
    return moved;
}


/*============================================================================*/
static bool g2048_slide_left(state_2048_t *s)
{
    bool moved = false;
    for (uint8_t r = 0; r < G_SIZE; r++)
    {
        uint32_t add = 0;
        if (g2048_slide_row_left(s->tile[r], &add)) moved = true;
        s->score += add;
    }
    return moved;
}


/*============================================================================*/
static bool g2048_slide_right(state_2048_t *s)
{
    /* Mirror, slide left, mirror back */
    for (uint8_t r = 0; r < G_SIZE; r++)
    {
        uint16_t rev[G_SIZE];
        for (uint8_t c = 0; c < G_SIZE; c++) rev[c] = s->tile[r][G_SIZE - 1 - c];
        memcpy(s->tile[r], rev, sizeof(rev));
    }
    bool moved = g2048_slide_left(s);
    for (uint8_t r = 0; r < G_SIZE; r++)
    {
        uint16_t rev[G_SIZE];
        for (uint8_t c = 0; c < G_SIZE; c++) rev[c] = s->tile[r][G_SIZE - 1 - c];
        memcpy(s->tile[r], rev, sizeof(rev));
    }
    return moved;
}


/*============================================================================*/
static bool g2048_slide_up(state_2048_t *s)
{
    /* Transpose, slide left, transpose back */
    for (uint8_t r = 0; r < G_SIZE; r++)
        for (uint8_t c = r + 1; c < G_SIZE; c++)
        {
            uint16_t t = s->tile[r][c];
            s->tile[r][c] = s->tile[c][r];
            s->tile[c][r] = t;
        }
    bool moved = g2048_slide_left(s);
    for (uint8_t r = 0; r < G_SIZE; r++)
        for (uint8_t c = r + 1; c < G_SIZE; c++)
        {
            uint16_t t = s->tile[r][c];
            s->tile[r][c] = s->tile[c][r];
            s->tile[c][r] = t;
        }
    return moved;
}


/*============================================================================*/
static bool g2048_slide_down(state_2048_t *s)
{
    /* Transpose, slide right, transpose back */
    for (uint8_t r = 0; r < G_SIZE; r++)
        for (uint8_t c = r + 1; c < G_SIZE; c++)
        {
            uint16_t t = s->tile[r][c];
            s->tile[r][c] = s->tile[c][r];
            s->tile[c][r] = t;
        }
    bool moved = g2048_slide_right(s);
    for (uint8_t r = 0; r < G_SIZE; r++)
        for (uint8_t c = r + 1; c < G_SIZE; c++)
        {
            uint16_t t = s->tile[r][c];
            s->tile[r][c] = s->tile[c][r];
            s->tile[c][r] = t;
        }
    return moved;
}


/*============================================================================*/
/*
 * @brief  Check whether any legal move remains.
 */
/*============================================================================*/
static bool g2048_has_moves(const state_2048_t *s)
{
    for (uint8_t r = 0; r < G_SIZE; r++)
    {
        for (uint8_t c = 0; c < G_SIZE; c++)
        {
            if (s->tile[r][c] == 0) return true;
            if (c + 1 < G_SIZE && s->tile[r][c] == s->tile[r][c + 1]) return true;
            if (r + 1 < G_SIZE && s->tile[r][c] == s->tile[r + 1][c]) return true;
        }
    }
    return false;
}


/*============================================================================*/
/*
 * @brief  Draw a single cell at grid column col, row row with value val.
 *         val == 0 draws an empty (hollow) cell.
 */
/*============================================================================*/
static void g2048_draw_cell(uint8_t col, uint8_t row, uint16_t val)
{
    uint8_t px = (uint8_t)(GRID_X + col * (CELL_W + CELL_GAP));
    uint8_t py = (uint8_t)(GRID_Y + row * (CELL_H + CELL_GAP));

    if (val == 0)
    {
        /* Empty cell — just a thin frame */
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        u8g2_DrawFrame(&m1_u8g2, px, py, CELL_W, CELL_H);
        return;
    }

    /* Filled cell */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_DrawBox(&m1_u8g2, px, py, CELL_W, CELL_H);

    /* Draw the number in inverted colour */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);

    char buf[8];
    g2048_uint_to_str(val, buf, sizeof(buf));

    /* Choose font based on digit count: 1-2 digits → finderskeepers (~5px wide),
     * 3 digits → NokiaSmallPlain (~4px wide), 4+ digits → u8glib_4 (tiny 4px). */
    uint8_t len = 0;
    for (; buf[len]; len++) {}

    if (len <= 2)
    {
        u8g2_SetFont(&m1_u8g2, u8g2_font_finderskeepers_tf);
    }
    else if (len == 3)
    {
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    }
    else
    {
        u8g2_SetFont(&m1_u8g2, u8g2_font_u8glib_4_tf);   /* tiny, fits 4-5 digit values */
    }

    uint8_t sw = u8g2_GetStrWidth(&m1_u8g2, buf);
    uint8_t tx = (uint8_t)(px + (CELL_W - sw) / 2);
    uint8_t ty = (uint8_t)(py + CELL_H - 3);
    u8g2_DrawStr(&m1_u8g2, tx, ty, buf);

    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
}


/*============================================================================*/
/*
 * @brief  Draw the full game screen (grid + score panel).
 */
/*============================================================================*/
static void g2048_draw(const state_2048_t *s)
{
    char buf[12];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* -- Grid -- */
    for (uint8_t r = 0; r < G_SIZE; r++)
    {
        for (uint8_t c = 0; c < G_SIZE; c++)
        {
            g2048_draw_cell(c, r, s->tile[r][c]);
        }
    }

    /* -- Right panel -- */
    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, PANEL_X, 9, "2048");

    /* Score label + value */
    u8g2_SetFont(&m1_u8g2, u8g2_font_finderskeepers_tf);
    u8g2_DrawStr(&m1_u8g2, PANEL_X, 22, "SCR");
    g2048_uint_to_str(s->score, buf, sizeof(buf));
    buf[6] = '\0';  /* truncate to fit panel */
    u8g2_DrawStr(&m1_u8g2, PANEL_X, 31, buf);

    /* Best label + value */
    u8g2_DrawStr(&m1_u8g2, PANEL_X, 43, "BST");
    g2048_uint_to_str(s->best, buf, sizeof(buf));
    buf[6] = '\0';
    u8g2_DrawStr(&m1_u8g2, PANEL_X, 52, buf);

    /* Hint line */
    u8g2_DrawStr(&m1_u8g2, PANEL_X, 63, "OK=new");

    /* Thin vertical divider */
    u8g2_DrawVLine(&m1_u8g2, PANEL_X - 2, 0, GAME_SCREEN_H);

    m1_u8g2_nextpage();
}


/*============================================================================*/
/*
 * @brief  Draw the title / intro screen.
 */
/*============================================================================*/
static void g2048_draw_title(void)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_LARGE_FONT_2B);
    u8g2_DrawStr(&m1_u8g2, 22, 22, "2048");

    u8g2_SetFont(&m1_u8g2, u8g2_font_finderskeepers_tf);
    u8g2_DrawStr(&m1_u8g2, 14, 38, "Slide tiles to merge");
    u8g2_DrawStr(&m1_u8g2, 18, 50, "OK=Start  BACK=Exit");

    m1_u8g2_nextpage();
}


/*============================================================================*/
/*
 * @brief  Draw the game-over overlay (wait for OK/BACK).
 */
/*============================================================================*/
static void g2048_draw_game_over(const state_2048_t *s)
{
    char buf[12];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_LARGE_FONT_2B);
    u8g2_DrawStr(&m1_u8g2, 4, 18, "GAME OVER");

    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    g2048_uint_to_str(s->score, buf, sizeof(buf));

    /* Build "Score: XXXXXX" without snprintf */
    char sbuf[20];
    const char *prefix = "Score: ";
    uint8_t pi = 0;
    while (prefix[pi]) sbuf[pi] = prefix[pi++];
    uint8_t bi = 0;
    while (buf[bi]) sbuf[pi++] = buf[bi++];
    sbuf[pi] = '\0';

    uint8_t sw = u8g2_GetStrWidth(&m1_u8g2, sbuf);
    u8g2_DrawStr(&m1_u8g2, (uint8_t)((GAME_SCREEN_W - sw) / 2), 34, sbuf);

    u8g2_SetFont(&m1_u8g2, u8g2_font_finderskeepers_tf);
    u8g2_DrawStr(&m1_u8g2, 8, 50, "OK=Retry   BACK=Exit");

    m1_u8g2_nextpage();
}


/*============================================================================*/
/*
 * @brief  Draw the win congratulation screen (hit 2048).
 */
/*============================================================================*/
static void g2048_draw_you_win(const state_2048_t *s)
{
    (void)s;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_LARGE_FONT_2B);
    u8g2_DrawStr(&m1_u8g2, 12, 18, "YOU WIN!");

    u8g2_SetFont(&m1_u8g2, u8g2_font_finderskeepers_tf);
    u8g2_DrawStr(&m1_u8g2, 6, 34, "You reached 2048!");
    u8g2_DrawStr(&m1_u8g2, 6, 46, "Keep going or quit.");
    u8g2_DrawStr(&m1_u8g2, 8, 60, "OK=Continue BACK=Exit");

    m1_u8g2_nextpage();
}


/*============================================================================*/
/*
 * @brief  Main 2048 game entry point. Runs own event loop; returns on BACK.
 */
/*============================================================================*/
void game_2048_run(void)
{
    game_button_t btn;

    /* Title screen */
    g2048_draw_title();
    for (;;)
    {
        btn = game_poll_button(500);
        if (btn == GAME_BTN_BACK) return;
        if (btn == GAME_BTN_OK)   break;
    }

restart:
    g2048_init(&g_state);
    g2048_draw(&g_state);

    for (;;)
    {
        btn = game_poll_button(300);

        if (btn == GAME_BTN_BACK) return;

        bool moved = false;

        switch (btn)
        {
            case GAME_BTN_LEFT:  moved = g2048_slide_left(&g_state);  break;
            case GAME_BTN_RIGHT: moved = g2048_slide_right(&g_state); break;
            case GAME_BTN_UP:    moved = g2048_slide_up(&g_state);    break;
            case GAME_BTN_DOWN:  moved = g2048_slide_down(&g_state);  break;
            case GAME_BTN_OK:
                /* OK during play = restart */
                goto restart;
            default:
                break;
        }

        if (!moved) continue;

        /* Update best score */
        if (g_state.score > g_state.best)
            g_state.best = g_state.score;

        /* Check for 2048 tile (first time) */
        if (!g_state.won)
        {
            for (uint8_t r = 0; r < G_SIZE; r++)
                for (uint8_t c = 0; c < G_SIZE; c++)
                    if (g_state.tile[r][c] >= 2048)
                        g_state.won = true;

            if (g_state.won)
            {
                m1_buzzer_notification();
                g2048_draw_you_win(&g_state);

                /* Wait for OK (continue) or BACK (exit) */
                for (;;)
                {
                    btn = game_poll_button(500);
                    if (btn == GAME_BTN_BACK) return;
                    if (btn == GAME_BTN_OK)   break;
                }
                /* Continue playing — add tile and keep going */
                g2048_add_random_tile(&g_state);
                g2048_draw(&g_state);
                continue;
            }
        }

        /* Spawn a new tile */
        g2048_add_random_tile(&g_state);

        /* Check for game over */
        if (!g2048_has_moves(&g_state))
        {
            g_state.game_over = true;
            m1_buzzer_set(BUZZER_FREQ_01_KHZ, 250);
            g2048_draw_game_over(&g_state);

            /* Wait for OK (restart) or BACK (exit) */
            for (;;)
            {
                btn = game_poll_button(500);
                if (btn == GAME_BTN_BACK) return;
                if (btn == GAME_BTN_OK)   goto restart;
            }
        }

        g2048_draw(&g_state);
    }
}
