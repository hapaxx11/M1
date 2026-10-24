/* See COPYING.txt for license details. */

/**
 * @file   queens_logic.c
 * @brief  Pure game logic for the Queens puzzle game (hardware-independent).
 *
 * See queens_logic.h for the rules and API.  No HAL/RTOS/u8g2 dependencies:
 * this unit is compiled both into the firmware and into the host test suite.
 *
 * M1 Project
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "queens_logic.h"

const uint8_t queens_diff_size[QUEENS_DIFF_COUNT] = { 6, 7, 8, 9 };

#define QUEENS_UNASSIGNED   0xFFu
#define QUEENS_GEN_ATTEMPTS 25u

/* Pattern draw order preference: strong structural differences first so
 * adjacent regions are as easy as possible to tell apart at a glance. */
static const uint8_t pattern_preference[QUEENS_PATTERN_COUNT] = {
    0, 3, 4, 5, 6, 2, 7, 1, 8
};

/*============================================================================*/
/* RNG                                                                        */
/*============================================================================*/

void queens_rng_seed(uint32_t *state, uint32_t seed)
{
    if (seed == 0u)
        seed = 0x9E3779B9u;
    *state = seed;
}

uint32_t queens_rng_next(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

uint8_t queens_rng_range(uint32_t *state, uint8_t min, uint8_t max)
{
    if (min >= max)
        return min;
    return (uint8_t)(min + (queens_rng_next(state) % (uint32_t)(max - min + 1u)));
}

/*============================================================================*/
/* Queen placement (randomized backtracking, one queen per row)               */
/*============================================================================*/

static bool place_queens(uint8_t n, uint32_t *rng, uint8_t col[QUEENS_MAX_N])
{
    uint16_t used_cols = 0;
    uint8_t  r = 0;
    uint16_t tried[QUEENS_MAX_N];      /* bitmask per row of tried columns */

    memset(tried, 0, sizeof(tried));

    while (true)
    {
        if (r == n)
            return true;

        /* Build the list of untried candidate columns for this row */
        uint8_t cand[QUEENS_MAX_N];
        uint8_t ncand = 0;
        for (uint8_t c = 0; c < n; c++)
        {
            if (used_cols & (uint16_t)(1u << c))
                continue;
            if (tried[r] & (uint16_t)(1u << c))
                continue;
            if (r > 0)
            {
                int d = (int)c - (int)col[r - 1];
                if (d >= -1 && d <= 1)
                    continue;           /* would touch the queen above */
            }
            cand[ncand++] = c;
        }

        if (ncand == 0)
        {
            /* Dead end: backtrack */
            tried[r] = 0;
            if (r == 0)
                return false;
            r--;
            used_cols &= (uint16_t)~(1u << col[r]);
            tried[r] |= (uint16_t)(1u << col[r]);
            continue;
        }

        /* Pick a random candidate */
        uint8_t pick = queens_rng_range(rng, 0, (uint8_t)(ncand - 1u));
        col[r] = cand[pick];
        used_cols |= (uint16_t)(1u << col[r]);
        r++;
    }
}

/*============================================================================*/
/* Region growth (random flood fill from the queen seeds)                     */
/*============================================================================*/

static void grow_regions(uint8_t n, uint32_t *rng,
                         const uint8_t col[QUEENS_MAX_N],
                         uint8_t region[QUEENS_MAX_N][QUEENS_MAX_N])
{
    uint8_t unassigned;

    for (uint8_t r = 0; r < n; r++)
        for (uint8_t c = 0; c < n; c++)
            region[r][c] = QUEENS_UNASSIGNED;

    /* Seed: the queen in row r starts region r */
    for (uint8_t r = 0; r < n; r++)
        region[r][col[r]] = r;

    unassigned = (uint8_t)(n * n - n);

    while (unassigned > 0)
    {
        /* Collect frontier: unassigned cells with at least one assigned
         * 4-neighbour */
        uint8_t fr[QUEENS_MAX_N * QUEENS_MAX_N];
        uint8_t fc[QUEENS_MAX_N * QUEENS_MAX_N];
        uint8_t nf = 0;

        for (uint8_t r = 0; r < n; r++)
        {
            for (uint8_t c = 0; c < n; c++)
            {
                if (region[r][c] != QUEENS_UNASSIGNED)
                    continue;
                bool frontier = false;
                if (r > 0     && region[r - 1][c] != QUEENS_UNASSIGNED) frontier = true;
                if (r + 1 < n && region[r + 1][c] != QUEENS_UNASSIGNED) frontier = true;
                if (c > 0     && region[r][c - 1] != QUEENS_UNASSIGNED) frontier = true;
                if (c + 1 < n && region[r][c + 1] != QUEENS_UNASSIGNED) frontier = true;
                if (frontier)
                {
                    fr[nf] = r;
                    fc[nf] = c;
                    nf++;
                }
            }
        }

        if (nf == 0)
            break;  /* cannot happen on a connected grid, but stay safe */

        uint8_t pick = queens_rng_range(rng, 0, (uint8_t)(nf - 1u));
        uint8_t r = fr[pick];
        uint8_t c = fc[pick];

        /* Choose a random region among the assigned neighbours */
        uint8_t nbr[4];
        uint8_t nn = 0;
        if (r > 0     && region[r - 1][c] != QUEENS_UNASSIGNED) nbr[nn++] = region[r - 1][c];
        if (r + 1 < n && region[r + 1][c] != QUEENS_UNASSIGNED) nbr[nn++] = region[r + 1][c];
        if (c > 0     && region[r][c - 1] != QUEENS_UNASSIGNED) nbr[nn++] = region[r][c - 1];
        if (c + 1 < n && region[r][c + 1] != QUEENS_UNASSIGNED) nbr[nn++] = region[r][c + 1];

        region[r][c] = nbr[queens_rng_range(rng, 0, (uint8_t)(nn - 1u))];
        unassigned--;
    }
}

/*============================================================================*/
/* Pattern assignment (adjacent regions get visually distinct patterns)       */
/*============================================================================*/

static void assign_patterns(uint8_t n, uint32_t *rng,
                            const uint8_t region[QUEENS_MAX_N][QUEENS_MAX_N],
                            uint8_t pattern[QUEENS_MAX_N])
{
    bool     adj[QUEENS_MAX_N][QUEENS_MAX_N];  /* adj[i][j]: regions i,j touch */
    uint8_t  used[QUEENS_PATTERN_COUNT];       /* how many regions use a pattern */
    uint8_t  order[QUEENS_MAX_N];

    memset(adj, 0, sizeof(adj));
    memset(used, 0, sizeof(used));

    for (uint8_t r = 0; r < n; r++)
    {
        for (uint8_t c = 0; c < n; c++)
        {
            uint8_t a = region[r][c];
            if (c + 1 < n && region[r][c + 1] != a)
            {
                adj[a][region[r][c + 1]] = true;
                adj[region[r][c + 1]][a] = true;
            }
            if (r + 1 < n && region[r + 1][c] != a)
            {
                adj[a][region[r + 1][c]] = true;
                adj[region[r + 1][c]][a] = true;
            }
        }
    }

    /* Visit regions in a random order so the preferred patterns do not
     * always land on the same region ids */
    for (uint8_t i = 0; i < n; i++)
        order[i] = i;
    for (uint8_t i = (uint8_t)(n - 1u); i > 0; i--)
    {
        uint8_t j = queens_rng_range(rng, 0, i);
        uint8_t t = order[i];
        order[i] = order[j];
        order[j] = t;
    }

    for (uint8_t oi = 0; oi < n; oi++)
    {
        uint8_t reg = order[oi];
        uint8_t best = QUEENS_PATTERN_COUNT;
        uint8_t best_used = 0xFF;

        for (uint8_t pi = 0; pi < QUEENS_PATTERN_COUNT; pi++)
        {
            uint8_t p = pattern_preference[pi];
            bool bad = false;
            for (uint8_t other = 0; other < n; other++)
            {
                if (other == reg)
                    continue;
                if (!adj[reg][other])
                    continue;
                /* Only regions assigned earlier constrain us */
                bool assigned = false;
                for (uint8_t k = 0; k < oi; k++)
                    if (order[k] == other) { assigned = true; break; }
                if (assigned && pattern[other] == p)
                {
                    bad = true;
                    break;
                }
            }
            if (!bad && used[p] < best_used)
            {
                best = p;
                best_used = used[p];
            }
        }

        if (best == QUEENS_PATTERN_COUNT)
            best = (uint8_t)(reg % QUEENS_PATTERN_COUNT);  /* unreachable with 9 patterns, 9 regions */

        pattern[reg] = best;
        used[best]++;
    }
}

/*============================================================================*/
/* Solver                                                                     */
/*============================================================================*/

static void solve_row(const queens_puzzle_t *pz, uint8_t r, uint16_t used_cols,
                      uint16_t used_regions, int8_t prev_col, uint8_t limit,
                      uint8_t *count)
{
    uint8_t n = pz->n;

    if (*count >= limit)
        return;

    if (r == n)
    {
        (*count)++;
        return;
    }

    for (uint8_t c = 0; c < n; c++)
    {
        if (used_cols & (uint16_t)(1u << c))
            continue;
        if (used_regions & (uint16_t)(1u << pz->region[r][c]))
            continue;
        if (prev_col >= 0)
        {
            int diff = (int)c - (int)prev_col;
            if (diff >= -1 && diff <= 1)
                continue;
        }
        solve_row(pz, (uint8_t)(r + 1u), (uint16_t)(used_cols | (1u << c)),
                  (uint16_t)(used_regions | (1u << pz->region[r][c])),
                  (int8_t)c, limit, count);
        if (*count >= limit)
            return;
    }
}

uint8_t queens_count_solutions(const queens_puzzle_t *pz, uint8_t limit)
{
    uint8_t count = 0;

    if (limit == 0)
        return 0;
    solve_row(pz, 0, 0, 0, -1, limit, &count);
    return count;
}

/*============================================================================*/
/* Uniqueness repair                                                          */
/*                                                                            */
/* Random flood-fill regions admit many solutions on larger boards, so after  */
/* growing the regions we search for a solution other than the seed one and   */
/* "kill" it: two of its non-seed cells are forced into the same region by    */
/* reassigning one cell to a neighbouring region.  The seed solution is never */
/* touched (its queen cells keep their regions), so the puzzle stays          */
/* solvable; region contiguity is verified after every mutation.              */
/*============================================================================*/

#define QUEENS_REPAIR_ITERS   500u

/* Find a full queen placement different from pz->solution_col. */
static bool find_alt_row(const queens_puzzle_t *pz, uint8_t r, uint16_t used_cols,
                         uint16_t used_regions, int8_t prev_col, bool deviated,
                         uint8_t out[QUEENS_MAX_N])
{
    uint8_t n = pz->n;

    if (r == n)
        return deviated;

    for (uint8_t c = 0; c < n; c++)
    {
        if (used_cols & (uint16_t)(1u << c))
            continue;
        if (used_regions & (uint16_t)(1u << pz->region[r][c]))
            continue;
        if (prev_col >= 0)
        {
            int diff = (int)c - (int)prev_col;
            if (diff >= -1 && diff <= 1)
                continue;
        }
        out[r] = c;
        if (find_alt_row(pz, (uint8_t)(r + 1u), (uint16_t)(used_cols | (1u << c)),
                         (uint16_t)(used_regions | (1u << pz->region[r][c])),
                         (int8_t)c, deviated || (c != pz->solution_col[r]), out))
            return true;
    }
    return false;
}

static bool find_alt_solution(const queens_puzzle_t *pz, uint8_t out[QUEENS_MAX_N])
{
    return find_alt_row(pz, 0, 0, 0, -1, false, out);
}

static bool cell_adjacent_to_region(const queens_puzzle_t *pz, uint8_t r, uint8_t c,
                                    uint8_t reg)
{
    uint8_t n = pz->n;
    if (r > 0     && pz->region[r - 1][c] == reg) return true;
    if (r + 1 < n && pz->region[r + 1][c] == reg) return true;
    if (c > 0     && pz->region[r][c - 1] == reg) return true;
    if (c + 1 < n && pz->region[r][c + 1] == reg) return true;
    return false;
}

static bool region_connected(const queens_puzzle_t *pz, uint8_t reg)
{
    uint8_t n = pz->n;
    bool    seen[QUEENS_MAX_N][QUEENS_MAX_N];
    uint8_t qr[QUEENS_MAX_N * QUEENS_MAX_N];
    uint8_t qc[QUEENS_MAX_N * QUEENS_MAX_N];
    uint8_t head = 0, tail = 0;
    uint8_t total = 0, reached = 0;

    memset(seen, 0, sizeof(seen));

    for (uint8_t r = 0; r < n; r++)
        for (uint8_t c = 0; c < n; c++)
            if (pz->region[r][c] == reg)
            {
                total++;
                if (tail == 0)
                {
                    qr[tail] = r;
                    qc[tail] = c;
                    tail++;
                    seen[r][c] = true;
                }
            }

    while (head < tail)
    {
        uint8_t r = qr[head], c = qc[head];
        head++;
        reached++;
        static const int8_t dr[4] = { -1, 1, 0, 0 };
        static const int8_t dc[4] = { 0, 0, -1, 1 };
        for (uint8_t k = 0; k < 4; k++)
        {
            int rr = (int)r + dr[k];
            int cc = (int)c + dc[k];
            if (rr < 0 || rr >= n || cc < 0 || cc >= n)
                continue;
            if (seen[rr][cc] || pz->region[rr][cc] != reg)
                continue;
            seen[rr][cc] = true;
            qr[tail] = (uint8_t)rr;
            qc[tail] = (uint8_t)cc;
            tail++;
        }
    }

    return reached == total;
}

/* Kill one alternative solution.
 *
 * Two strategies, both leaving the seed solution untouched:
 *  1. Evict: move a deviating cell of the alt out of its region (into any
 *     adjacent region) when it is the alt's only cell in that region — the
 *     region then holds zero alt queens, so some other region must hold two.
 *  2. Merge: move a deviating cell into the region of another deviating
 *     cell of the same alt — the alt uses that region twice.
 * Region contiguity of the source region is verified after every mutation.
 * Returns false when no legal mutation kills the alt. */
static bool kill_alt(queens_puzzle_t *pz, const uint8_t alt[QUEENS_MAX_N],
                     uint32_t *rng)
{
    uint8_t n = pz->n;
    uint8_t rows[QUEENS_MAX_N];
    uint8_t nr = 0;

    for (uint8_t r = 0; r < n; r++)
        if (alt[r] != pz->solution_col[r])
            rows[nr++] = r;

    /* Shuffle so the same cell is not always the one that moves */
    for (uint8_t i = (uint8_t)(nr - 1u); i > 0; i--)
    {
        uint8_t j = queens_rng_range(rng, 0, i);
        uint8_t t = rows[i];
        rows[i] = rows[j];
        rows[j] = t;
    }

    /* Strategy 1: evict */
    for (uint8_t i = 0; i < nr; i++)
    {
        uint8_t r1 = rows[i], c1 = alt[r1];
        uint8_t src = pz->region[r1][c1];

        /* Count the alt's cells in region src */
        uint8_t cnt = 0;
        for (uint8_t r = 0; r < n; r++)
            if (pz->region[r][alt[r]] == src)
                cnt++;
        if (cnt != 1)
            continue;

        /* Try adjacent destination regions in random order */
        uint8_t dsts[4];
        uint8_t nd = 0;
        if (r1 > 0     && pz->region[r1 - 1][c1] != src) dsts[nd++] = pz->region[r1 - 1][c1];
        if (r1 + 1 < n && pz->region[r1 + 1][c1] != src) dsts[nd++] = pz->region[r1 + 1][c1];
        if (c1 > 0     && pz->region[r1][c1 - 1] != src) dsts[nd++] = pz->region[r1][c1 - 1];
        if (c1 + 1 < n && pz->region[r1][c1 + 1] != src) dsts[nd++] = pz->region[r1][c1 + 1];

        for (uint8_t k = (uint8_t)(nd > 0 ? nd - 1u : 0u); nd > 1 && k > 0; k--)
        {
            uint8_t j = queens_rng_range(rng, 0, k);
            uint8_t t = dsts[k];
            dsts[k] = dsts[j];
            dsts[j] = t;
        }

        for (uint8_t k = 0; k < nd; k++)
        {
            pz->region[r1][c1] = dsts[k];
            if (region_connected(pz, src))
                return true;
            pz->region[r1][c1] = src;
        }
    }

    /* Strategy 2: merge */
    for (uint8_t i = 0; i < nr; i++)
    {
        for (uint8_t j = 0; j < nr; j++)
        {
            if (i == j)
                continue;
            uint8_t r1 = rows[i], c1 = alt[r1];
            uint8_t r2 = rows[j], c2 = alt[r2];
            uint8_t dst = pz->region[r2][c2];
            uint8_t src = pz->region[r1][c1];

            if (src == dst)
                continue;
            if (!cell_adjacent_to_region(pz, r1, c1, dst))
                continue;

            pz->region[r1][c1] = dst;
            if (region_connected(pz, src))
                return true;   /* alt now uses region dst twice */
            pz->region[r1][c1] = src;
        }
    }
    return false;
}

static bool repair_uniqueness(queens_puzzle_t *pz, uint32_t *rng)
{
    uint8_t alt[QUEENS_MAX_N];

    for (uint16_t k = 0; k < QUEENS_REPAIR_ITERS; k++)
    {
        if (!find_alt_solution(pz, alt))
            return true;
        if (!kill_alt(pz, alt, rng))
            return false;
    }
    return false;
}

/*============================================================================*/
/* Generation                                                                 */
/*============================================================================*/

bool queens_generate(queens_puzzle_t *pz, uint8_t n, uint32_t *rng)
{
    if (pz == NULL || rng == NULL || n < 4 || n > QUEENS_MAX_N)
        return false;

    memset(pz, 0, sizeof(*pz));
    pz->n = n;
    pz->unique = false;

    bool have_layout = false;

    for (uint8_t attempt = 0; attempt < QUEENS_GEN_ATTEMPTS; attempt++)
    {
        if (!place_queens(n, rng, pz->solution_col))
            continue;
        have_layout = true;
        grow_regions(n, rng, pz->solution_col, pz->region);

        if (repair_uniqueness(pz, rng))
        {
            pz->unique = true;
            break;
        }
        /* Repair failed: reroll a fresh layout.  The last attempt is kept
         * as a fallback (still solvable by construction, just not proven
         * unique). */
    }

    if (have_layout)
        assign_patterns(n, rng, (const uint8_t (*)[QUEENS_MAX_N])pz->region,
                        pz->pattern);

    return have_layout;
}

/*============================================================================*/
/* Board evaluation                                                           */
/*============================================================================*/

uint8_t queens_find_conflicts(const queens_puzzle_t *pz,
                              const uint8_t cells[][QUEENS_MAX_N],
                              bool conflict[][QUEENS_MAX_N])
{
    uint8_t n = pz->n;
    bool    flagged[QUEENS_MAX_N][QUEENS_MAX_N];
    uint8_t nconflict = 0;

    memset(flagged, 0, sizeof(flagged));

    for (uint8_t r1 = 0; r1 < n; r1++)
    {
        for (uint8_t c1 = 0; c1 < n; c1++)
        {
            if (cells[r1][c1] != QUEENS_CELL_QUEEN)
                continue;

            for (uint8_t r2 = r1; r2 < n; r2++)
            {
                for (uint8_t c2 = 0; c2 < n; c2++)
                {
                    if (r1 == r2 && c2 <= c1)
                        continue;
                    if (cells[r2][c2] != QUEENS_CELL_QUEEN)
                        continue;

                    bool bad = (r1 == r2) || (c1 == c2) ||
                               (pz->region[r1][c1] == pz->region[r2][c2]);
                    if (!bad)
                    {
                        uint8_t dr = (r2 > r1) ? (uint8_t)(r2 - r1) : (uint8_t)(r1 - r2);
                        uint8_t dc = (c2 > c1) ? (uint8_t)(c2 - c1) : (uint8_t)(c1 - c2);
                        bad = (dr <= 1 && dc <= 1);
                    }

                    if (bad)
                    {
                        flagged[r1][c1] = true;
                        flagged[r2][c2] = true;
                    }
                }
            }
        }
    }

    for (uint8_t r = 0; r < n; r++)
        for (uint8_t c = 0; c < n; c++)
            if (flagged[r][c])
                nconflict++;

    if (conflict != NULL)
        memcpy(conflict, flagged, sizeof(flagged));

    return nconflict;
}

bool queens_is_solved(const queens_puzzle_t *pz,
                      const uint8_t cells[][QUEENS_MAX_N])
{
    uint8_t n = pz->n;
    uint8_t nqueens = 0;

    for (uint8_t r = 0; r < n; r++)
        for (uint8_t c = 0; c < n; c++)
            if (cells[r][c] == QUEENS_CELL_QUEEN)
                nqueens++;

    if (nqueens != n)
        return false;

    return queens_find_conflicts(pz, cells, NULL) == 0;
}

void queens_auto_exclude(const queens_puzzle_t *pz,
                         const uint8_t cells[][QUEENS_MAX_N],
                         bool excl[][QUEENS_MAX_N])
{
    uint8_t n = pz->n;

    memset(excl, 0, sizeof(bool) * QUEENS_MAX_N * QUEENS_MAX_N);

    for (uint8_t r = 0; r < n; r++)
    {
        for (uint8_t c = 0; c < n; c++)
        {
            if (cells[r][c] != QUEENS_CELL_QUEEN)
                continue;

            uint8_t region_id = pz->region[r][c];

            for (uint8_t i = 0; i < n; i++)
            {
                excl[r][i] = true;                    /* row    */
                excl[i][c] = true;                    /* column */
                for (uint8_t j = 0; j < n; j++)
                    if (pz->region[i][j] == region_id)
                        excl[i][j] = true;            /* region */
            }

            for (int8_t dr = -1; dr <= 1; dr++)
            {
                for (int8_t dc = -1; dc <= 1; dc++)
                {
                    int rr = (int)r + dr;
                    int cc = (int)c + dc;
                    if (rr >= 0 && rr < n && cc >= 0 && cc < n)
                        excl[rr][cc] = true;          /* surrounding */
                }
            }
        }
    }

    /* Queen cells themselves are never excluded */
    for (uint8_t r = 0; r < n; r++)
        for (uint8_t c = 0; c < n; c++)
            if (cells[r][c] == QUEENS_CELL_QUEEN)
                excl[r][c] = false;
}

bool queens_all_decided(const queens_puzzle_t *pz,
                        const uint8_t cells[][QUEENS_MAX_N],
                        const bool excl[][QUEENS_MAX_N])
{
    uint8_t n = pz->n;

    for (uint8_t r = 0; r < n; r++)
    {
        for (uint8_t c = 0; c < n; c++)
        {
            if (cells[r][c] == QUEENS_CELL_EMPTY && !excl[r][c])
                return false;
        }
    }
    return true;
}

/*============================================================================*/
/* High-score table                                                           */
/*============================================================================*/

void queens_hs_init(queens_hs_table_t *t)
{
    memset(t, 0, sizeof(*t));
}

int queens_hs_insert(queens_hs_table_t *t, uint8_t diff,
                     const queens_hs_entry_t *entry)
{
    queens_hs_entry_t *row;
    int rank = -1;

    if (diff >= QUEENS_DIFF_COUNT || entry == NULL)
        return -1;

    row = t->e[diff];

    for (uint8_t i = 0; i < QUEENS_HS_PER_DIFF; i++)
    {
        if (!row[i].used || entry->seconds < row[i].seconds)
        {
            rank = (int)i;
            break;
        }
    }

    if (rank < 0)
        return -1;

    for (int8_t i = QUEENS_HS_PER_DIFF - 1; i > rank; i--)
        row[i] = row[i - 1];

    row[rank] = *entry;
    row[rank].initials[3] = '\0';
    row[rank].used = 1;
    return rank;
}

void queens_hs_format(const queens_hs_entry_t *e, char *buf, size_t bufsize)
{
    if (bufsize == 0)
        return;
    snprintf(buf, bufsize, "%.3s,%lu,%lu,%u", e->initials,
             (unsigned long)e->seconds, (unsigned long)e->date,
             (unsigned)e->no_mistakes);
}

bool queens_hs_parse(const char *line, queens_hs_entry_t *e)
{
    const char *p = line;
    uint8_t len = 0;

    if (line == NULL || e == NULL)
        return false;

    memset(e, 0, sizeof(*e));

    /* Initials: up to 3 chars before the first comma */
    while (*p != '\0' && *p != ',' && len < 3)
        e->initials[len++] = *p++;
    if (len == 0 || *p != ',')
        return false;
    e->initials[len] = '\0';
    p++;

    e->seconds = (uint32_t)strtoul(p, (char **)&p, 10);
    if (*p == ',')
    {
        p++;
        e->date = (uint32_t)strtoul(p, (char **)&p, 10);
        if (*p == ',')
        {
            p++;
            e->no_mistakes = (uint8_t)(strtoul(p, NULL, 10) ? 1u : 0u);
        }
    }

    e->used = 1;
    return true;
}
