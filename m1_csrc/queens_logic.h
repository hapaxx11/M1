/* See COPYING.txt for license details. */

/**
 * @file   queens_logic.h
 * @brief  Pure game logic for the Queens puzzle game (hardware-independent).
 *
 * Queens is a placement puzzle played on an NxN grid divided into N regions
 * (shown as monochrome fill patterns): place exactly one queen per row, per
 * column and per region, with no two queens touching — not even diagonally.
 *
 * This module contains everything that can be computed without the display,
 * buttons, RTC or SD card: RNG, procedural puzzle generation (with a
 * uniqueness check), conflict/win evaluation, auto-exclude computation and
 * the persistent high-score table.  It is host-testable (no HAL/RTOS/u8g2
 * includes).
 *
 * M1 Project
 */

#ifndef QUEENS_LOGIC_H_
#define QUEENS_LOGIC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QUEENS_MAX_N         9   /* largest board (Expert) */
#define QUEENS_DIFF_COUNT    4   /* Easy/Medium/Hard/Expert */
#define QUEENS_HS_PER_DIFF   3   /* top-3 times per difficulty */
#define QUEENS_PATTERN_COUNT 9   /* distinct monochrome fill patterns */

/* Board size per difficulty level (Easy..Expert). */
extern const uint8_t queens_diff_size[QUEENS_DIFF_COUNT];

/* Cell states held by the UI layer. */
#define QUEENS_CELL_EMPTY    0u
#define QUEENS_CELL_MARK     1u   /* user-ruled-out ("X") */
#define QUEENS_CELL_QUEEN    2u

typedef struct {
    uint8_t n;                                       /* board size */
    uint8_t region[QUEENS_MAX_N][QUEENS_MAX_N];      /* region id 0..n-1 */
    uint8_t pattern[QUEENS_MAX_N];                   /* fill pattern per region id */
    uint8_t solution_col[QUEENS_MAX_N];              /* queen column per row */
    bool    unique;                                  /* solution proven unique */
} queens_puzzle_t;

/*--- RNG (xorshift32) ------------------------------------------------------*/

/* Seed the RNG state.  Any non-zero seed is used as-is; zero is remapped. */
void     queens_rng_seed(uint32_t *state, uint32_t seed);
/* Next random value; state must have been seeded. */
uint32_t queens_rng_next(uint32_t *state);
/* Random value in [min, max] inclusive. */
uint8_t  queens_rng_range(uint32_t *state, uint8_t min, uint8_t max);

/*--- Puzzle generation -----------------------------------------------------*/

/**
 * @brief  Generate a random solvable puzzle of size n (4..QUEENS_MAX_N).
 *
 * Places N non-attacking queens (one per row/column, no 8-way adjacency),
 * grows one region from each queen cell by random flood fill, assigns
 * visually distinct patterns to adjacent regions, then verifies with the
 * solver that the solution is unique (retrying with fresh random layouts
 * up to a bounded number of attempts).
 *
 * @retval true  Puzzle generated (pz->unique reports uniqueness).
 * @retval false n out of range; pz contents undefined.
 */
bool queens_generate(queens_puzzle_t *pz, uint8_t n, uint32_t *rng);

/**
 * @brief  Count solutions of a puzzle, stopping at @p limit.
 * @retval Number of solutions found, capped at limit.
 */
uint8_t queens_count_solutions(const queens_puzzle_t *pz, uint8_t limit);

/*--- Board evaluation ------------------------------------------------------*/

/**
 * @brief  Find queens that directly invalidate each other.
 *
 * A queen conflicts when it shares a row, column or region with another
 * queen, or touches another queen (8-way adjacency).  Only cells holding
 * QUEENS_CELL_QUEEN participate; marked/empty cells never conflict.
 *
 * @param conflict  Output: true for every queen cell in conflict (may be NULL).
 * @retval Number of queens in conflict (0 = none).
 */
uint8_t queens_find_conflicts(const queens_puzzle_t *pz,
                              const uint8_t cells[][QUEENS_MAX_N],
                              bool conflict[][QUEENS_MAX_N]);

/**
 * @brief  Winning condition: N queens placed with zero conflicts.
 */
bool queens_is_solved(const queens_puzzle_t *pz,
                      const uint8_t cells[][QUEENS_MAX_N]);

/**
 * @brief  Compute the auto-exclude layer: every empty cell ruled out by at
 *         least one placed queen (same row, column, region, or 8-way
 *         surrounding).  Queen cells themselves are never excluded.
 */
void queens_auto_exclude(const queens_puzzle_t *pz,
                         const uint8_t cells[][QUEENS_MAX_N],
                         bool excl[][QUEENS_MAX_N]);

/**
 * @brief  True when every cell is decided: holds a queen, is user-marked,
 *         or is auto-excluded.  Used to detect "board full but not solved".
 */
bool queens_all_decided(const queens_puzzle_t *pz,
                        const uint8_t cells[][QUEENS_MAX_N],
                        const bool excl[][QUEENS_MAX_N]);

/*--- High-score table ------------------------------------------------------*/

typedef struct {
    char     initials[4];   /* 3 chars + NUL, uppercase A-Z */
    uint32_t seconds;       /* solve time */
    uint32_t date;          /* yyyymmdd, 0 = date unknown */
    uint8_t  no_mistakes;   /* solved without any conflict alert */
    uint8_t  used;          /* slot occupied */
} queens_hs_entry_t;

typedef struct {
    queens_hs_entry_t e[QUEENS_DIFF_COUNT][QUEENS_HS_PER_DIFF];
} queens_hs_table_t;

void queens_hs_init(queens_hs_table_t *t);

/**
 * @brief  Insert an entry into one difficulty's top-3 (ascending by time).
 * @retval Rank 0..2 when the entry made the table, -1 otherwise.
 */
int  queens_hs_insert(queens_hs_table_t *t, uint8_t diff,
                      const queens_hs_entry_t *entry);

/**
 * @brief  Format one entry as "INI,seconds,yyyymmdd,nomistakes".
 *         Always NUL-terminates when bufsize > 0.
 */
void queens_hs_format(const queens_hs_entry_t *e, char *buf, size_t bufsize);

/**
 * @brief  Parse a line produced by queens_hs_format().  Tolerant of a
 *         missing trailing field (older files) and of a zero date.
 * @retval true when at least initials+seconds were parsed.
 */
bool queens_hs_parse(const char *line, queens_hs_entry_t *e);

#ifdef __cplusplus
}
#endif

#endif /* QUEENS_LOGIC_H_ */
