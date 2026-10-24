/* See COPYING.txt for license details. */

/**
 * @file   game_queens.c
 * @brief  Queens puzzle game for M1 (LinkedIn "Queens" style).
 *
 * Place one queen per row, column and pattern region on an NxN board;
 * queens may not touch, not even diagonally.  Puzzles are procedurally
 * generated (unique solution) in four sizes: Easy 6x6, Medium 7x7,
 * Hard 8x8, Expert 9x9.
 *
 * Controls:
 *  - D-pad:  move cursor (start screen: navigate; LEFT/RIGHT change option)
 *  - OK:     cycle cell empty -> X mark -> queen -> empty
 *  - BACK:   no game active -> exit; game active -> pause (resume/give up)
 *
 * Each game is timed; the top 3 times per difficulty are stored on SD
 * (0:/apps/queens.dat) with 3 initials, the date, and a no-mistakes flag
 * (earned when the board is solved without any conflict alert).  An
 * auto-exclude assist (default on) dots every cell ruled out by a placed
 * queen.  All options persist and default to the last used.
 *
 * Pure game logic lives in queens_logic.c (host-tested); this file is the
 * UI, input, timing and persistence layer.
 *
 * M1 Project -- Hapax fork
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "ff.h"
#include "m1_games.h"
#include "queens_logic.h"

/*************************** D E F I N E S ************************************/

#define QUEENS_SAVE_PATH    "0:/apps/queens.dat"
#define QUEENS_SAVE_MAGIC   "QUEENS1"

#define Q_FONT_SMALL    M1_DISP_SUB_MENU_FONT_N     /* NokiaSmallPlain */
#define Q_FONT_MONO     M1_DISP_FUNC_MENU_FONT_N    /* profont12 */
#define Q_FONT_BIG      M1_DISP_LARGE_FONT_2B       /* VCR_OSD (uppercase) */
#define Q_FONT_HUGE     M1_DISP_LARGE_FONT_1B       /* profont17 */

static const char *const queens_diff_name[QUEENS_DIFF_COUNT] = {
    "Easy", "Medium", "Hard", "Expert"
};

/* Crown glyphs (XBM, LSB-first per row) */
static const uint8_t crown_5x5[] = { 0x15, 0x1F, 0x0E, 0x0E, 0x1F };
static const uint8_t crown_7x6[] = { 0x49, 0x55, 0x7F, 0x3E, 0x3E, 0x7F };

//************************** S T R U C T U R E S *******************************/

typedef enum {
    QS_START = 0,
    QS_PLAY,
    QS_EXIT
} queens_screen_t;

typedef struct {
    queens_puzzle_t  puzzle;
    uint8_t          cells[QUEENS_MAX_N][QUEENS_MAX_N];
    bool             excl[QUEENS_MAX_N][QUEENS_MAX_N];
    bool             conflict[QUEENS_MAX_N][QUEENS_MAX_N];
    queens_hs_table_t hs;
    uint32_t         rng;
    uint32_t         elapsed_ms;    /* accumulated while paused */
    uint32_t         resume_tick;   /* tick when the timer last resumed */
    uint32_t         win_seconds;
    char             initials[4];   /* last used initials */
    uint8_t          difficulty;
    uint8_t          auto_exclude;
    uint8_t          cursor_r, cursor_c;
    uint8_t          sel;           /* start-screen selection */
    uint8_t          n_conflicts;
    bool             mistakes;
    bool             gave_up;
    bool             stuck;         /* all cells decided, not solved */
    bool             stuck_beeped;
    int              new_rank;      /* high-score rank of this game, -1 */
} queens_ctx_t;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static void             queens_load(queens_ctx_t *ctx);
static void             queens_save(const queens_ctx_t *ctx);
static uint32_t         queens_today(void);
static queens_screen_t  scr_start(queens_ctx_t *ctx);
static queens_screen_t  play_game(queens_ctx_t *ctx);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/* Small helpers                                                              */
/*============================================================================*/

static void fmt_time(char *buf, size_t size, uint32_t seconds)
{
    uint32_t m = seconds / 60u;
    uint32_t s = seconds % 60u;
    if (m > 99u) { m = 99u; s = 59u; }
    snprintf(buf, size, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

static void fmt_date(char *buf, size_t size, uint32_t yyyymmdd, bool with_year)
{
    if (yyyymmdd == 0u)
    {
        snprintf(buf, size, with_year ? "--/--/--" : "--/--");
        return;
    }
    uint32_t y = (yyyymmdd / 10000u) % 100u;
    uint32_t m = (yyyymmdd / 100u) % 100u;
    uint32_t d = yyyymmdd % 100u;
    if (with_year)
        snprintf(buf, size, "%02lu/%02lu/%02lu",
                 (unsigned long)m, (unsigned long)d, (unsigned long)y);
    else
        snprintf(buf, size, "%02lu/%02lu", (unsigned long)m, (unsigned long)d);
}

static uint32_t queens_today(void)
{
    m1_time_t dt;
    m1_get_datetime(&dt);
    if (dt.year < 2024 || dt.year > 2100 || dt.month < 1 || dt.month > 12 ||
        dt.day < 1 || dt.day > 31)
        return 0u;                       /* clock not set */
    return (uint32_t)dt.year * 10000u + (uint32_t)dt.month * 100u + dt.day;
}

static void sanitize_initials(char ini[4])
{
    uint8_t len = 0;
    for (uint8_t i = 0; i < 3 && ini[i]; i++)
    {
        char c = ini[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c < 'A' || c > 'Z')   c = 'A';
        ini[len++] = c;
    }
    while (len < 3)
        ini[len++] = 'A';
    ini[3] = '\0';
}

/*============================================================================*/
/* Persistence: 0:/apps/queens.dat                                            */
/*============================================================================*/

static void queens_load(queens_ctx_t *ctx)
{
    FIL fp;
    char buf[512];
    UINT br = 0;

    if (f_open(&fp, QUEENS_SAVE_PATH, FA_READ) != FR_OK)
        return;                          /* no SD / no file: keep defaults */
    if (f_read(&fp, buf, sizeof(buf) - 1u, &br) != FR_OK)
    {
        f_close(&fp);
        return;
    }
    f_close(&fp);
    buf[br] = '\0';

    char *saveptr = NULL;
    for (char *line = strtok_r(buf, "\r\n", &saveptr);
         line != NULL;
         line = strtok_r(NULL, "\r\n", &saveptr))
    {
        if (strncmp(line, "diff=", 5) == 0)
        {
            uint8_t d = (uint8_t)atoi(line + 5);
            if (d < QUEENS_DIFF_COUNT)
                ctx->difficulty = d;
        }
        else if (strncmp(line, "ax=", 3) == 0)
        {
            ctx->auto_exclude = (line[3] == '1') ? 1u : 0u;
        }
        else if (strncmp(line, "ini=", 4) == 0)
        {
            strncpy(ctx->initials, line + 4, 3);
            ctx->initials[3] = '\0';
            sanitize_initials(ctx->initials);
        }
        else
        {
            uint8_t d, r;
            const char *val;
            if (queens_hs_parse_key(line, &d, &r, &val))
            {
                queens_hs_entry_t e;
                if (queens_hs_parse(val, &e))
                {
                    sanitize_initials(e.initials);
                    ctx->hs.e[d][r] = e;
                }
            }
        }
    }
}

static void queens_save(const queens_ctx_t *ctx)
{
    FIL fp;
    char line[40];
    UINT bw;

    f_mkdir("0:/apps");     /* fresh card may not have it yet; ignore result */
    if (f_open(&fp, QUEENS_SAVE_PATH, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;                          /* no SD: scores stay session-only */

    snprintf(line, sizeof(line), "%s\n", QUEENS_SAVE_MAGIC);
    f_write(&fp, line, strlen(line), &bw);
    snprintf(line, sizeof(line), "diff=%u\nax=%u\nini=%.3s\n",
             ctx->difficulty, ctx->auto_exclude, ctx->initials);
    f_write(&fp, line, strlen(line), &bw);

    for (uint8_t d = 0; d < QUEENS_DIFF_COUNT; d++)
    {
        for (uint8_t r = 0; r < QUEENS_HS_PER_DIFF; r++)
        {
            const queens_hs_entry_t *e = &ctx->hs.e[d][r];
            char ent[24];
            if (!e->used)
                continue;
            queens_hs_format(e, ent, sizeof(ent));
            snprintf(line, sizeof(line), "s%u.%u=%s\n", d, r, ent);
            f_write(&fp, line, strlen(line), &bw);
        }
    }

    f_close(&fp);
}

/*============================================================================*/
/* Board rendering                                                            */
/*============================================================================*/

static uint8_t cell_px(uint8_t n) { return (uint8_t)(GAME_SCREEN_H / n); }

/* Monochrome fill patterns, drawn per pixel inside a cell.  Pattern indices
 * must match QUEENS_PATTERN_COUNT and the preference order in queens_logic. */
static bool pattern_pixel(uint8_t pattern, uint8_t dx, uint8_t dy)
{
    switch (pattern)
    {
        case 1:  return ((dx + dy) & 1u) != 0u;                    /* checker      */
        case 2:  return (dx % 4u == 1u) && (dy % 4u == 1u);        /* dots         */
        case 3:  return (dy % 3u) == 1u;                           /* horizontal   */
        case 4:  return (dx % 3u) == 1u;                           /* vertical     */
        case 5:  return ((dx + dy) % 4u) == 1u;                    /* diagonal \   */
        case 6:  return (((int)dx - (int)dy + 64) % 4) == 1;       /* diagonal /   */
        case 7:  return (((dx >> 1) + (dy >> 1)) & 1u) != 0u;      /* 2x2 checker  */
        case 8:  return (dx % 4u == 0u) || (dy % 4u == 0u);        /* grid         */
        default: return false;                                     /* 0: white     */
    }
}

static void draw_cell_pattern(const queens_ctx_t *ctx, uint8_t r, uint8_t c,
                              uint8_t cs, uint8_t y0)
{
    uint8_t pat = ctx->puzzle.pattern[ctx->puzzle.region[r][c]];
    if (pat == 0)
        return;
    uint8_t x0 = (uint8_t)(c * cs);
    uint8_t cy = (uint8_t)(y0 + r * cs);
    for (uint8_t dy = 0; dy < cs; dy++)
        for (uint8_t dx = 0; dx < cs; dx++)
            if (pattern_pixel(pat, dx, dy))
                u8g2_DrawPixel(&m1_u8g2, (u8g2_uint_t)(x0 + dx),
                               (u8g2_uint_t)(cy + dy));
}

static void draw_region_borders(const queens_ctx_t *ctx, uint8_t n, uint8_t cs,
                                uint8_t y0)
{
    for (uint8_t r = 0; r < n; r++)
    {
        for (uint8_t c = 0; c < n; c++)
        {
            uint8_t reg = ctx->puzzle.region[r][c];
            if (c + 1 < n && ctx->puzzle.region[r][c + 1] != reg)
                u8g2_DrawVLine(&m1_u8g2, (u8g2_uint_t)((c + 1) * cs - 1),
                               (u8g2_uint_t)(y0 + r * cs), cs);
            if (r + 1 < n && ctx->puzzle.region[r + 1][c] != reg)
                u8g2_DrawHLine(&m1_u8g2, (u8g2_uint_t)(c * cs),
                               (u8g2_uint_t)(y0 + (r + 1) * cs - 1), cs);
        }
    }
    u8g2_DrawFrame(&m1_u8g2, 0, y0, (u8g2_uint_t)(n * cs), (u8g2_uint_t)(n * cs));
}

static void draw_queen(uint8_t px, uint8_t py, uint8_t cs)
{
    if (cs >= 8)
        u8g2_DrawXBMP(&m1_u8g2, (u8g2_uint_t)(px + (cs - 7) / 2),
                      (u8g2_uint_t)(py + (cs - 6) / 2), 7, 6, crown_7x6);
    else
        u8g2_DrawXBMP(&m1_u8g2, (u8g2_uint_t)(px + (cs - 5) / 2),
                      (u8g2_uint_t)(py + (cs - 5) / 2), 5, 5, crown_5x5);
}

static void draw_mark_x(uint8_t px, uint8_t py, uint8_t cs)
{
    u8g2_DrawLine(&m1_u8g2, (u8g2_uint_t)(px + 2), (u8g2_uint_t)(py + 2),
                  (u8g2_uint_t)(px + cs - 3), (u8g2_uint_t)(py + cs - 3));
    u8g2_DrawLine(&m1_u8g2, (u8g2_uint_t)(px + cs - 3), (u8g2_uint_t)(py + 2),
                  (u8g2_uint_t)(px + 2), (u8g2_uint_t)(py + cs - 3));
}

static void draw_board(const queens_ctx_t *ctx)
{
    uint8_t n  = ctx->puzzle.n;
    uint8_t cs = cell_px(n);
    uint8_t y0 = (uint8_t)((GAME_SCREEN_H - n * cs) / 2u);

    for (uint8_t r = 0; r < n; r++)
        for (uint8_t c = 0; c < n; c++)
            draw_cell_pattern(ctx, r, c, cs, y0);

    draw_region_borders(ctx, n, cs, y0);

    /* Marks and queens */
    for (uint8_t r = 0; r < n; r++)
    {
        for (uint8_t c = 0; c < n; c++)
        {
            uint8_t px = (uint8_t)(c * cs);
            uint8_t py = (uint8_t)(y0 + r * cs);
            if (ctx->cells[r][c] == QUEENS_CELL_QUEEN)
                draw_queen(px, py, cs);
            else if (ctx->cells[r][c] == QUEENS_CELL_MARK)
                draw_mark_x(px, py, cs);
            else if (ctx->auto_exclude && ctx->excl[r][c])
                u8g2_DrawBox(&m1_u8g2, (u8g2_uint_t)(px + cs / 2 - 1),
                             (u8g2_uint_t)(py + cs / 2 - 1), 2, 2);
        }
    }

    /* Conflicting queens: invert their cells */
    u8g2_SetDrawColor(&m1_u8g2, 2);            /* XOR */
    for (uint8_t r = 0; r < n; r++)
        for (uint8_t c = 0; c < n; c++)
            if (ctx->conflict[r][c])
                u8g2_DrawBox(&m1_u8g2, (u8g2_uint_t)(c * cs + 1),
                             (u8g2_uint_t)(y0 + r * cs + 1),
                             (u8g2_uint_t)(cs - 1), (u8g2_uint_t)(cs - 1));

    /* Cursor: double XOR frame, visible over any pattern */
    u8g2_DrawFrame(&m1_u8g2, (u8g2_uint_t)(ctx->cursor_c * cs),
                   (u8g2_uint_t)(y0 + ctx->cursor_r * cs), cs, cs);
    u8g2_DrawFrame(&m1_u8g2, (u8g2_uint_t)(ctx->cursor_c * cs + 1),
                   (u8g2_uint_t)(y0 + ctx->cursor_r * cs + 1),
                   (u8g2_uint_t)(cs - 2), (u8g2_uint_t)(cs - 2));
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
}

static uint32_t cur_elapsed_s(const queens_ctx_t *ctx)
{
    return (ctx->elapsed_ms + (HAL_GetTick() - ctx->resume_tick)) / 1000u;
}

static void draw_play_panel(const queens_ctx_t *ctx, uint32_t secs)
{
    uint8_t n  = ctx->puzzle.n;
    uint8_t cs = cell_px(n);
    uint8_t px = (uint8_t)(n * cs + 2);
    char buf[12];
    uint8_t nq = 0;

    for (uint8_t r = 0; r < n; r++)
        for (uint8_t c = 0; c < n; c++)
            if (ctx->cells[r][c] == QUEENS_CELL_QUEEN)
                nq++;

    u8g2_DrawVLine(&m1_u8g2, (u8g2_uint_t)(px - 1), 0, GAME_SCREEN_H);

    u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
    u8g2_DrawStr(&m1_u8g2, px, 8, queens_diff_name[ctx->difficulty]);

    u8g2_SetFont(&m1_u8g2, Q_FONT_MONO);
    fmt_time(buf, sizeof(buf), secs);
    u8g2_DrawStr(&m1_u8g2, px, 21, buf);

    u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
    snprintf(buf, sizeof(buf), "Q %u/%u", nq, n);
    u8g2_DrawStr(&m1_u8g2, px, 33, buf);

    if (ctx->stuck)
        u8g2_DrawStr(&m1_u8g2, px, 45, "STUCK!");
    else if (ctx->n_conflicts > 0)
        u8g2_DrawStr(&m1_u8g2, px, 45, "CLASH!");

    u8g2_DrawStr(&m1_u8g2, px, 62, "OK:cycle");
}

static void draw_play(const queens_ctx_t *ctx, uint32_t secs)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    draw_board(ctx);
    draw_play_panel(ctx, secs);
    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Score-list rendering (shared by start and end screens)                     */
/*============================================================================*/

static void draw_score_rows(const queens_ctx_t *ctx, uint8_t diff, uint8_t x,
                            uint8_t y, uint8_t row_h, bool with_year,
                            int highlight_rank)
{
    char line[28], tbuf[8], dbuf[10];

    u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
    for (uint8_t i = 0; i < QUEENS_HS_PER_DIFF; i++)
    {
        const queens_hs_entry_t *e = &ctx->hs.e[diff][i];
        uint8_t ry = (uint8_t)(y + i * row_h);

        if (!e->used)
        {
            snprintf(line, sizeof(line), "%u. --- --:--", i + 1u);
            u8g2_DrawStr(&m1_u8g2, x, ry, line);
            continue;
        }

        fmt_time(tbuf, sizeof(tbuf), e->seconds);
        fmt_date(dbuf, sizeof(dbuf), e->date, with_year);
        snprintf(line, sizeof(line), "%u.%.3s %s %s", i + 1u, e->initials,
                 tbuf, dbuf);

        if ((int)i == highlight_rank)
        {
            u8g2_DrawRBox(&m1_u8g2, x, (u8g2_uint_t)(ry - 7),
                          (u8g2_uint_t)(GAME_SCREEN_W - x - (e->no_mistakes ? 12 : 0)),
                          9, 1);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            u8g2_DrawStr(&m1_u8g2, (u8g2_uint_t)(x + 2), ry, line);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
        else
        {
            u8g2_DrawStr(&m1_u8g2, x, ry, line);
        }

        if (e->no_mistakes)
            u8g2_DrawXBMP(&m1_u8g2, (u8g2_uint_t)(GAME_SCREEN_W - 10),
                          (u8g2_uint_t)(ry - 8), 10, 10, check_10x10);
    }
}

/*============================================================================*/
/* Start screen                                                               */
/*============================================================================*/

#define START_SEL_PLAY   0u
#define START_SEL_DIFF   1u
#define START_SEL_AUTO   2u
#define START_SEL_COUNT  3u

static void draw_start(const queens_ctx_t *ctx)
{
    static const char *const item_label[START_SEL_COUNT] = {
        "Play", "Difficulty", "Auto-Excl"
    };
    char val[12];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, Q_FONT_BIG);
    uint8_t tw = u8g2_GetStrWidth(&m1_u8g2, "QUEENS");
    u8g2_DrawStr(&m1_u8g2, (u8g2_uint_t)((GAME_SCREEN_W - tw) / 2u), 13, "QUEENS");
    u8g2_DrawHLine(&m1_u8g2, 0, 16, GAME_SCREEN_W);

    u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
    for (uint8_t i = 0; i < START_SEL_COUNT; i++)
    {
        uint8_t ry = (uint8_t)(25 + i * 9u);
        char row[24];

        val[0] = '\0';
        if (i == START_SEL_DIFF)
            snprintf(val, sizeof(val), ": %s", queens_diff_name[ctx->difficulty]);
        else if (i == START_SEL_AUTO)
            snprintf(val, sizeof(val), ": %s", ctx->auto_exclude ? "On" : "Off");
        snprintf(row, sizeof(row), "%s%s", item_label[i], val);

        if (i == ctx->sel)
        {
            u8g2_DrawRBox(&m1_u8g2, 0, (u8g2_uint_t)(ry - 7), 70, 9, 2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            u8g2_DrawStr(&m1_u8g2, 3, ry, row);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
        else
        {
            u8g2_DrawStr(&m1_u8g2, 3, ry, row);
        }
    }

    /* High scores for the selected difficulty, right column */
    u8g2_DrawStr(&m1_u8g2, 74, 25, "Best:");
    u8g2_DrawStr(&m1_u8g2, 74, 33, queens_diff_name[ctx->difficulty]);
    for (uint8_t i = 0; i < QUEENS_HS_PER_DIFF; i++)
    {
        const queens_hs_entry_t *e = &ctx->hs.e[ctx->difficulty][i];
        uint8_t ry = (uint8_t)(42 + i * 7u);
        char row[16], tbuf[8];
        if (e->used)
        {
            fmt_time(tbuf, sizeof(tbuf), e->seconds);
            snprintf(row, sizeof(row), "%.3s %s", e->initials, tbuf);
        }
        else
        {
            snprintf(row, sizeof(row), "--- --:--");
        }
        u8g2_DrawStr(&m1_u8g2, 74, ry, row);
        if (e->used && e->no_mistakes)
            u8g2_DrawXBMP(&m1_u8g2, 118, (u8g2_uint_t)(ry - 7), 10, 10,
                          check_10x10);
    }

    u8g2_DrawStr(&m1_u8g2, 3, 63, "LR:chg");
    m1_u8g2_nextpage();
}

static queens_screen_t scr_start(queens_ctx_t *ctx)
{
    draw_start(ctx);

    for (;;)
    {
        game_button_t btn = game_poll_button(60000);
        bool dirty = false;

        switch (btn)
        {
            case GAME_BTN_UP:
                ctx->sel = (ctx->sel + START_SEL_COUNT - 1u) % START_SEL_COUNT;
                dirty = true;
                break;
            case GAME_BTN_DOWN:
                ctx->sel = (uint8_t)((ctx->sel + 1u) % START_SEL_COUNT);
                dirty = true;
                break;
            case GAME_BTN_LEFT:
            case GAME_BTN_RIGHT:
                if (ctx->sel == START_SEL_DIFF)
                {
                    uint8_t d = ctx->difficulty;
                    d = (btn == GAME_BTN_RIGHT)
                        ? (uint8_t)((d + 1u) % QUEENS_DIFF_COUNT)
                        : (uint8_t)((d + QUEENS_DIFF_COUNT - 1u) % QUEENS_DIFF_COUNT);
                    ctx->difficulty = d;
                    queens_save(ctx);
                    dirty = true;
                }
                else if (ctx->sel == START_SEL_AUTO)
                {
                    ctx->auto_exclude = ctx->auto_exclude ? 0u : 1u;
                    queens_save(ctx);
                    dirty = true;
                }
                break;
            case GAME_BTN_OK:
                if (ctx->sel == START_SEL_PLAY)
                    return QS_PLAY;
                /* OK on an option behaves like RIGHT */
                if (ctx->sel == START_SEL_DIFF)
                {
                    ctx->difficulty =
                        (uint8_t)((ctx->difficulty + 1u) % QUEENS_DIFF_COUNT);
                    queens_save(ctx);
                    dirty = true;
                }
                else if (ctx->sel == START_SEL_AUTO)
                {
                    ctx->auto_exclude = ctx->auto_exclude ? 0u : 1u;
                    queens_save(ctx);
                    dirty = true;
                }
                break;
            case GAME_BTN_BACK:
                queens_save(ctx);
                return QS_EXIT;
            default:
                break;
        }

        if (dirty)
            draw_start(ctx);
    }
}

/*============================================================================*/
/* Pause screen                                                               */
/*============================================================================*/

/* Returns true to resume, false to give up. */
static bool pause_menu(queens_ctx_t *ctx)
{
    uint8_t sel = 0;   /* 0 = Resume, 1 = Give Up */

    ctx->elapsed_ms += HAL_GetTick() - ctx->resume_tick;   /* stop timer */

    for (;;)
    {
        char tbuf[8];
        m1_u8g2_firstpage();
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

        u8g2_SetFont(&m1_u8g2, Q_FONT_BIG);
        uint8_t tw = u8g2_GetStrWidth(&m1_u8g2, "PAUSED");
        u8g2_DrawStr(&m1_u8g2, (u8g2_uint_t)((GAME_SCREEN_W - tw) / 2u), 16,
                     "PAUSED");

        u8g2_SetFont(&m1_u8g2, Q_FONT_MONO);
        fmt_time(tbuf, sizeof(tbuf), ctx->elapsed_ms / 1000u);
        tw = u8g2_GetStrWidth(&m1_u8g2, tbuf);
        u8g2_DrawStr(&m1_u8g2, (u8g2_uint_t)((GAME_SCREEN_W - tw) / 2u), 30,
                     tbuf);

        u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
        static const char *const opt[2] = { "Resume", "Give Up" };
        for (uint8_t i = 0; i < 2; i++)
        {
            uint8_t ry = (uint8_t)(44 + i * 10u);
            uint8_t w = (uint8_t)(u8g2_GetStrWidth(&m1_u8g2, opt[i]) + 8u);
            uint8_t x = (uint8_t)((GAME_SCREEN_W - w) / 2u);
            if (i == sel)
            {
                u8g2_DrawRBox(&m1_u8g2, x, (u8g2_uint_t)(ry - 7), w, 9, 2);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
                u8g2_DrawStr(&m1_u8g2, (u8g2_uint_t)(x + 4), ry, opt[i]);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            }
            else
            {
                u8g2_DrawStr(&m1_u8g2, (u8g2_uint_t)(x + 4), ry, opt[i]);
            }
        }
        m1_u8g2_nextpage();

        game_button_t btn = game_poll_button(60000);
        if (btn == GAME_BTN_UP || btn == GAME_BTN_DOWN)
            sel ^= 1u;
        else if (btn == GAME_BTN_OK)
        {
            if (sel == 0)
            {
                ctx->resume_tick = HAL_GetTick();          /* resume timer */
                return true;
            }
            return false;                                   /* give up */
        }
        else if (btn == GAME_BTN_BACK)
        {
            ctx->resume_tick = HAL_GetTick();
            return true;
        }
    }
}

/*============================================================================*/
/* Initials entry (3-letter spinner)                                          */
/*============================================================================*/

static void enter_initials(queens_ctx_t *ctx)
{
    uint8_t pos = 0;

    for (;;)
    {
        m1_u8g2_firstpage();
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

        u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
        u8g2_DrawStr(&m1_u8g2, 24, 12, "New best time!");
        u8g2_DrawStr(&m1_u8g2, 20, 24, "Enter initials:");

        u8g2_SetFont(&m1_u8g2, Q_FONT_HUGE);
        for (uint8_t i = 0; i < 3; i++)
        {
            char ch[2] = { ctx->initials[i], '\0' };
            uint8_t x = (uint8_t)(44 + i * 14u);
            if (i == pos)
            {
                u8g2_DrawRBox(&m1_u8g2, (u8g2_uint_t)(x - 2), 28, 13, 17, 2);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
                u8g2_DrawStr(&m1_u8g2, x, 42, ch);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            }
            else
            {
                u8g2_DrawStr(&m1_u8g2, x, 42, ch);
            }
        }

        u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
        u8g2_DrawStr(&m1_u8g2, 8, 60, "UD:chr LR:pos OK:save");
        m1_u8g2_nextpage();

        game_button_t btn = game_poll_button(60000);
        switch (btn)
        {
            case GAME_BTN_UP:
                ctx->initials[pos] =
                    (ctx->initials[pos] == 'Z') ? 'A' : (char)(ctx->initials[pos] + 1);
                break;
            case GAME_BTN_DOWN:
                ctx->initials[pos] =
                    (ctx->initials[pos] == 'A') ? 'Z' : (char)(ctx->initials[pos] - 1);
                break;
            case GAME_BTN_LEFT:
                pos = (pos + 2u) % 3u;
                break;
            case GAME_BTN_RIGHT:
                pos = (uint8_t)((pos + 1u) % 3u);
                break;
            case GAME_BTN_OK:
                sanitize_initials(ctx->initials);
                return;
            case GAME_BTN_BACK:
                return;    /* keep current (default/last-used) initials */
            default:
                break;
        }
    }
}

/*============================================================================*/
/* End screens                                                                */
/*============================================================================*/

/* Compact solution board for the give-up screen (borders + queens only). */
static void draw_solution_mini(const queens_ctx_t *ctx, uint8_t x0, uint8_t side)
{
    uint8_t n  = ctx->puzzle.n;
    uint8_t cs = (uint8_t)(side / n);
    uint8_t y0 = (uint8_t)((GAME_SCREEN_H - n * cs) / 2u);

    for (uint8_t r = 0; r < n; r++)
    {
        for (uint8_t c = 0; c < n; c++)
        {
            uint8_t reg = ctx->puzzle.region[r][c];
            if (c + 1 < n && ctx->puzzle.region[r][c + 1] != reg)
                u8g2_DrawVLine(&m1_u8g2, (u8g2_uint_t)(x0 + (c + 1) * cs - 1),
                               (u8g2_uint_t)(y0 + r * cs), cs);
            if (r + 1 < n && ctx->puzzle.region[r + 1][c] != reg)
                u8g2_DrawHLine(&m1_u8g2, (u8g2_uint_t)(x0 + c * cs),
                               (u8g2_uint_t)(y0 + (r + 1) * cs - 1), cs);
        }
    }
    u8g2_DrawFrame(&m1_u8g2, x0, y0, (u8g2_uint_t)(n * cs), (u8g2_uint_t)(n * cs));

    for (uint8_t r = 0; r < n; r++)
    {
        uint8_t c = ctx->puzzle.solution_col[r];
        uint8_t px = (uint8_t)(x0 + c * cs);
        uint8_t py = (uint8_t)(y0 + r * cs);
        if (cs >= 6)
            draw_queen(px, py, cs);
        else
            u8g2_DrawBox(&m1_u8g2, (u8g2_uint_t)(px + 1), (u8g2_uint_t)(py + 1),
                         (u8g2_uint_t)(cs - 2), (u8g2_uint_t)(cs - 2));
    }
}

static void draw_end(const queens_ctx_t *ctx)
{
    char buf[24];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    if (ctx->gave_up)
    {
        draw_solution_mini(ctx, 2, 46);

        uint8_t px = 52;
        u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
        u8g2_DrawStr(&m1_u8g2, px, 9, "Gave Up");
        u8g2_DrawStr(&m1_u8g2, 2, 62, "Solution");

        snprintf(buf, sizeof(buf), "Best-%s", queens_diff_name[ctx->difficulty]);
        u8g2_DrawStr(&m1_u8g2, px, 21, buf);

        for (uint8_t i = 0; i < QUEENS_HS_PER_DIFF; i++)
        {
            const queens_hs_entry_t *e = &ctx->hs.e[ctx->difficulty][i];
            uint8_t ry = (uint8_t)(32 + i * 8u);
            char tbuf[8], dbuf[8];
            if (e->used)
            {
                fmt_time(tbuf, sizeof(tbuf), e->seconds);
                fmt_date(dbuf, sizeof(dbuf), e->date, false);
                snprintf(buf, sizeof(buf), "%u.%.3s %s %s", i + 1u, e->initials,
                         tbuf, dbuf);
            }
            else
            {
                snprintf(buf, sizeof(buf), "%u.--- --:--", i + 1u);
            }
            u8g2_DrawStr(&m1_u8g2, px, ry, buf);
            if (e->used && e->no_mistakes)
                u8g2_DrawXBMP(&m1_u8g2, 118, (u8g2_uint_t)(ry - 7), 10, 10,
                              check_10x10);
        }
    }
    else
    {
        u8g2_SetFont(&m1_u8g2, Q_FONT_BIG);
        u8g2_DrawStr(&m1_u8g2, 4, 14, "SOLVED!");

        u8g2_SetFont(&m1_u8g2, Q_FONT_MONO);
        fmt_time(buf, sizeof(buf), ctx->win_seconds);
        u8g2_DrawStr(&m1_u8g2, 78, 14, buf);

        u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
        if (!ctx->mistakes)
        {
            u8g2_DrawStr(&m1_u8g2, 4, 26, "No mistakes");
            u8g2_DrawXBMP(&m1_u8g2, 64, 18, 10, 10, check_10x10);
        }

        snprintf(buf, sizeof(buf), "Best - %s", queens_diff_name[ctx->difficulty]);
        u8g2_DrawStr(&m1_u8g2, 4, 36, buf);

        draw_score_rows(ctx, ctx->difficulty, 4, 45, 9, true, ctx->new_rank);
    }

    m1_u8g2_nextpage();
}

/* End screen: BACK or OK returns to the start screen. */
static void end_screen(queens_ctx_t *ctx)
{
    draw_end(ctx);
    for (;;)
    {
        game_button_t btn = game_poll_button(60000);
        if (btn == GAME_BTN_BACK || btn == GAME_BTN_OK)
            return;
    }
}

/*============================================================================*/
/* Game flow                                                                  */
/*============================================================================*/

static void recompute(queens_ctx_t *ctx, bool placed_queen)
{
    uint8_t before = ctx->n_conflicts;

    if (ctx->auto_exclude)
        queens_auto_exclude(&ctx->puzzle,
                            (const uint8_t (*)[QUEENS_MAX_N])ctx->cells,
                            ctx->excl);
    else
        memset(ctx->excl, 0, sizeof(ctx->excl));

    ctx->n_conflicts = queens_find_conflicts(&ctx->puzzle,
                                             (const uint8_t (*)[QUEENS_MAX_N])ctx->cells,
                                             ctx->conflict);

    if (placed_queen && ctx->n_conflicts > 0 && ctx->n_conflicts != before)
    {
        /* The new queen directly invalidates other placed queens */
        ctx->mistakes = true;
        m1_buzzer_set(BUZZER_FREQ_01_KHZ, 120);
    }

    bool was_stuck = ctx->stuck;
    bool now_stuck = !queens_is_solved(&ctx->puzzle,
                                       (const uint8_t (*)[QUEENS_MAX_N])ctx->cells) &&
                     queens_all_decided(&ctx->puzzle,
                                        (const uint8_t (*)[QUEENS_MAX_N])ctx->cells,
                                        (const bool (*)[QUEENS_MAX_N])ctx->excl);
    /* Defer the penalty by one cell-changing action: with auto-exclude on,
     * the final correct queen's EMPTY->MARK step already decides every
     * square, and that intermediate step is not a mistake (the MARK->QUEEN
     * step resolves it immediately). */
    if (queens_stuck_penalty(was_stuck, now_stuck) && !ctx->stuck_beeped)
    {
        /* Every square is marked but the board is not a winning one, and
         * the user's latest change did not resolve it */
        ctx->mistakes = true;
        ctx->stuck_beeped = true;
        m1_buzzer_set(BUZZER_FREQ_01_KHZ, 250);
    }
    ctx->stuck = now_stuck;
    if (!ctx->stuck)
        ctx->stuck_beeped = false;
}

static bool would_qualify(const queens_ctx_t *ctx, uint32_t seconds)
{
    const queens_hs_entry_t *row = ctx->hs.e[ctx->difficulty];
    for (uint8_t i = 0; i < QUEENS_HS_PER_DIFF; i++)
        if (!row[i].used || seconds < row[i].seconds)
            return true;
    return false;
}

/* Plays one full game: generate -> play -> (pause) -> end screen.
 * Returns to the start screen when done. */
static queens_screen_t play_game(queens_ctx_t *ctx)
{
    /* Generating can take a few hundred ms on the larger boards */
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, Q_FONT_SMALL);
    u8g2_DrawStr(&m1_u8g2, 28, 34, "Generating...");
    m1_u8g2_nextpage();

    memset(ctx->cells, 0, sizeof(ctx->cells));
    memset(ctx->excl, 0, sizeof(ctx->excl));
    memset(ctx->conflict, 0, sizeof(ctx->conflict));
    ctx->cursor_r = 0;
    ctx->cursor_c = 0;
    ctx->mistakes = false;
    ctx->gave_up = false;
    ctx->stuck = false;
    ctx->stuck_beeped = false;
    ctx->n_conflicts = 0;
    ctx->new_rank = -1;
    ctx->elapsed_ms = 0;

    queens_generate(&ctx->puzzle, queens_diff_size[ctx->difficulty], &ctx->rng);

    ctx->resume_tick = HAL_GetTick();
    uint32_t shown_s = 0xFFFFFFFFu;
    draw_play(ctx, 0);

    for (;;)
    {
        game_button_t btn = game_poll_button(200);
        bool redraw = false;

        uint32_t secs = cur_elapsed_s(ctx);
        if (secs != shown_s)
        {
            shown_s = secs;
            redraw = true;
        }

        switch (btn)
        {
            case GAME_BTN_UP:
                if (ctx->cursor_r > 0) ctx->cursor_r--;
                redraw = true;
                break;
            case GAME_BTN_DOWN:
                if (ctx->cursor_r + 1 < ctx->puzzle.n) ctx->cursor_r++;
                redraw = true;
                break;
            case GAME_BTN_LEFT:
                if (ctx->cursor_c > 0) ctx->cursor_c--;
                redraw = true;
                break;
            case GAME_BTN_RIGHT:
                if (ctx->cursor_c + 1 < ctx->puzzle.n) ctx->cursor_c++;
                redraw = true;
                break;

            case GAME_BTN_OK:
            {
                uint8_t *cell = &ctx->cells[ctx->cursor_r][ctx->cursor_c];
                bool placed = false;
                if (*cell == QUEENS_CELL_EMPTY)
                    *cell = QUEENS_CELL_MARK;
                else if (*cell == QUEENS_CELL_MARK)
                {
                    *cell = QUEENS_CELL_QUEEN;
                    placed = true;
                }
                else
                    *cell = QUEENS_CELL_EMPTY;

                recompute(ctx, placed);

                if (queens_is_solved(&ctx->puzzle,
                                     (const uint8_t (*)[QUEENS_MAX_N])ctx->cells))
                {
                    ctx->elapsed_ms += HAL_GetTick() - ctx->resume_tick;
                    ctx->win_seconds = ctx->elapsed_ms / 1000u;
                    if (ctx->win_seconds == 0)
                        ctx->win_seconds = 1;
                    m1_buzzer_notification();

                    if (would_qualify(ctx, ctx->win_seconds))
                    {
                        queens_hs_entry_t e;
                        memset(&e, 0, sizeof(e));
                        enter_initials(ctx);
                        memcpy(e.initials, ctx->initials, sizeof(e.initials));
                        e.seconds = ctx->win_seconds;
                        e.date = queens_today();
                        e.no_mistakes = ctx->mistakes ? 0u : 1u;
                        e.used = 1;
                        ctx->new_rank = queens_hs_insert(&ctx->hs,
                                                         ctx->difficulty, &e);
                        queens_save(ctx);
                    }
                    end_screen(ctx);
                    return QS_START;
                }
                redraw = true;
                break;
            }

            case GAME_BTN_BACK:
                if (!pause_menu(ctx))
                {
                    ctx->gave_up = true;
                    end_screen(ctx);
                    return QS_START;
                }
                shown_s = 0xFFFFFFFFu;    /* force panel refresh */
                redraw = true;
                break;

            default:
                break;
        }

        if (redraw)
            draw_play(ctx, cur_elapsed_s(ctx));
    }
}

/*============================================================================*/
/* Entry point                                                                */
/*============================================================================*/

void game_queens_run(void)
{
    /* RAM is critically tight: the whole game context lives on the FreeRTOS
     * heap (~700 bytes) instead of static .bss. */
    queens_ctx_t *ctx = malloc(sizeof(queens_ctx_t));
    if (ctx == NULL)
        return;

    memset(ctx, 0, sizeof(*ctx));
    ctx->difficulty = 1;          /* Medium */
    ctx->auto_exclude = 1;        /* default on */
    ctx->sel = START_SEL_PLAY;
    ctx->new_rank = -1;
    memcpy(ctx->initials, "AAA", 4);
    queens_hs_init(&ctx->hs);
    queens_rng_seed(&ctx->rng, HAL_GetTick() ^ (uint32_t)(uintptr_t)ctx);

    queens_load(ctx);
    sanitize_initials(ctx->initials);

    queens_screen_t screen = QS_START;
    while (screen != QS_EXIT)
    {
        if (screen == QS_START)
            screen = scr_start(ctx);
        else
            screen = play_game(ctx);
    }

    queens_save(ctx);
    free(ctx);
}
