/* See COPYING.txt for license details. */

/**
 * @file   test_queens_logic.c
 * @brief  Host-side unit tests for the Queens puzzle game pure logic
 *         (m1_csrc/queens_logic.c).  Covers RNG, procedural generation
 *         (validity, region contiguity, pattern distinctness, solution
 *         uniqueness), the solver, conflict/win/auto-exclude evaluation,
 *         and the high-score table with serialize/parse round-trips.
 */

#include <string.h>
#include "unity.h"
#include "queens_logic.h"

void setUp(void) { }
void tearDown(void) { }

/*--- Helpers ----------------------------------------------------------------*/

static void solution_to_cells(const queens_puzzle_t *pz,
                              uint8_t cells[][QUEENS_MAX_N])
{
    memset(cells, 0, sizeof(uint8_t) * QUEENS_MAX_N * QUEENS_MAX_N);
    for (uint8_t r = 0; r < pz->n; r++)
        cells[r][pz->solution_col[r]] = QUEENS_CELL_QUEEN;
}

/* Assert the stored solution satisfies all Queens rules. */
static void assert_solution_valid(const queens_puzzle_t *pz)
{
    uint8_t n = pz->n;
    uint16_t cols = 0, regs = 0;

    for (uint8_t r = 0; r < n; r++)
    {
        uint8_t c = pz->solution_col[r];
        TEST_ASSERT_LESS_THAN(n, c);
        TEST_ASSERT_FALSE_MESSAGE(cols & (1u << c), "duplicate column");
        cols |= (uint16_t)(1u << c);

        uint8_t reg = pz->region[r][c];
        TEST_ASSERT_LESS_THAN(n, reg);
        TEST_ASSERT_FALSE_MESSAGE(regs & (1u << reg), "duplicate region");
        regs |= (uint16_t)(1u << reg);

        if (r > 0)
        {
            int d = (int)c - (int)pz->solution_col[r - 1];
            TEST_ASSERT_TRUE_MESSAGE(d < -1 || d > 1, "adjacent queens");
        }
    }
}

/* Assert every region id 0..n-1 is present and 4-connected. */
static void assert_regions_valid(const queens_puzzle_t *pz)
{
    uint8_t n = pz->n;

    for (uint8_t want = 0; want < n; want++)
    {
        /* BFS from the first cell of region `want` */
        bool seen[QUEENS_MAX_N][QUEENS_MAX_N];
        uint8_t qr[QUEENS_MAX_N * QUEENS_MAX_N];
        uint8_t qc[QUEENS_MAX_N * QUEENS_MAX_N];
        uint8_t head = 0, tail = 0;
        unsigned region_cells = 0, reached = 0;

        memset(seen, 0, sizeof(seen));

        for (uint8_t r = 0; r < n; r++)
            for (uint8_t c = 0; c < n; c++)
            {
                TEST_ASSERT_LESS_THAN_MESSAGE(n, pz->region[r][c],
                                              "cell unassigned");
                if (pz->region[r][c] == want)
                {
                    region_cells++;
                    if (tail == 0)
                    {
                        qr[tail] = r;
                        qc[tail] = c;
                        tail++;
                        seen[r][c] = true;
                    }
                }
            }

        TEST_ASSERT_GREATER_THAN_MESSAGE(0, region_cells, "region missing");

        while (head < tail)
        {
            uint8_t r = qr[head], c = qc[head];
            head++;
            reached++;
            static const int8_t dr[4] = { -1, 1, 0, 0 };
            static const int8_t dc[4] = { 0, 0, -1, 1 };
            for (uint8_t k = 0; k < 4; k++)
            {
                int rr = r + dr[k], cc = c + dc[k];
                if (rr < 0 || rr >= n || cc < 0 || cc >= n)
                    continue;
                if (seen[rr][cc] || pz->region[rr][cc] != want)
                    continue;
                seen[rr][cc] = true;
                qr[tail] = (uint8_t)rr;
                qc[tail] = (uint8_t)cc;
                tail++;
            }
        }

        TEST_ASSERT_EQUAL_UINT_MESSAGE(region_cells, reached,
                                       "region not contiguous");
    }
}

/* Assert adjacent regions never share a fill pattern. */
static void assert_patterns_valid(const queens_puzzle_t *pz)
{
    uint8_t n = pz->n;

    for (uint8_t r = 0; r < n; r++)
    {
        for (uint8_t c = 0; c < n; c++)
        {
            uint8_t a = pz->region[r][c];
            TEST_ASSERT_LESS_THAN(QUEENS_PATTERN_COUNT, pz->pattern[a]);
            if (c + 1 < n && pz->region[r][c + 1] != a)
                TEST_ASSERT_NOT_EQUAL_MESSAGE(pz->pattern[a],
                                              pz->pattern[pz->region[r][c + 1]],
                                              "adjacent regions share pattern");
            if (r + 1 < n && pz->region[r + 1][c] != a)
                TEST_ASSERT_NOT_EQUAL_MESSAGE(pz->pattern[a],
                                              pz->pattern[pz->region[r + 1][c]],
                                              "adjacent regions share pattern");
        }
    }
}

/*--- Pattern visibility and dynamic assignment -------------------------------*/

void test_pattern_visibility_follows_shaded_density(void)
{
    static const uint8_t expected[QUEENS_PATTERN_COUNT] = {
        0, 2, 5, 6, 3, 4, 8, 1, 7
    };
    unsigned previous_density = 0;

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, queens_pattern_preference,
                                  QUEENS_PATTERN_COUNT);

    for (uint8_t rank = 0; rank < QUEENS_PATTERN_COUNT; rank++)
    {
        uint8_t pattern = queens_pattern_preference[rank];
        unsigned density = 0;
        for (uint8_t dy = 0; dy < 12; dy++)
            for (uint8_t dx = 0; dx < 12; dx++)
                if (queens_pattern_pixel(pattern, dx, dy))
                    density++;

        TEST_ASSERT_GREATER_OR_EQUAL_UINT(previous_density, density);
        previous_density = density;
    }

    TEST_ASSERT_FALSE(queens_pattern_pixel(0, 0, 0));
    TEST_ASSERT_TRUE(queens_pattern_pixel(QUEENS_PATTERN_SOLID, 0, 0));
}

void test_dynamic_patterns_fill_queen_regions_and_reuse_best_remaining(void)
{
    queens_puzzle_t pz;
    uint8_t cells[QUEENS_MAX_N][QUEENS_MAX_N];
    memset(&pz, 0, sizeof(pz));
    memset(cells, 0, sizeof(cells));
    pz.n = 6;
    for (uint8_t r = 0; r < pz.n; r++)
        for (uint8_t c = 0; c < pz.n; c++)
            pz.region[r][c] = c;

    queens_assign_patterns(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells);
    for (uint8_t reg = 0; reg < pz.n; reg++)
        TEST_ASSERT_EQUAL_UINT8(queens_pattern_preference[reg], pz.pattern[reg]);

    cells[0][0] = QUEENS_CELL_QUEEN;
    queens_assign_patterns(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells);
    TEST_ASSERT_TRUE(queens_region_has_queen(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, 0));
    TEST_ASSERT_EQUAL_UINT8(QUEENS_PATTERN_SOLID, pz.pattern[0]);
    for (uint8_t reg = 1; reg < pz.n; reg++)
        TEST_ASSERT_EQUAL_UINT8(queens_pattern_preference[reg - 1], pz.pattern[reg]);

    cells[1][1] = QUEENS_CELL_QUEEN;
    queens_assign_patterns(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells);
    TEST_ASSERT_EQUAL_UINT8(QUEENS_PATTERN_SOLID, pz.pattern[0]);
    TEST_ASSERT_EQUAL_UINT8(QUEENS_PATTERN_SOLID, pz.pattern[1]);
    for (uint8_t reg = 2; reg < pz.n; reg++)
        TEST_ASSERT_EQUAL_UINT8(queens_pattern_preference[reg - 2], pz.pattern[reg]);

    cells[0][0] = QUEENS_CELL_EMPTY;
    queens_assign_patterns(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells);
    TEST_ASSERT_FALSE(queens_region_has_queen(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, 0));
    TEST_ASSERT_NOT_EQUAL(QUEENS_PATTERN_SOLID, pz.pattern[0]);
}

/*--- RNG --------------------------------------------------------------------*/

void test_rng_seed_zero_is_remapped(void)
{
    uint32_t st = 0;
    queens_rng_seed(&st, 0);
    TEST_ASSERT_NOT_EQUAL(0, st);
}

void test_rng_deterministic(void)
{
    uint32_t a, b;
    queens_rng_seed(&a, 12345);
    queens_rng_seed(&b, 12345);
    for (int i = 0; i < 32; i++)
        TEST_ASSERT_EQUAL_UINT32(queens_rng_next(&a), queens_rng_next(&b));
}

void test_rng_range_bounds(void)
{
    uint32_t st;
    queens_rng_seed(&st, 999);
    for (int i = 0; i < 500; i++)
    {
        uint8_t v = queens_rng_range(&st, 3, 7);
        TEST_ASSERT_GREATER_OR_EQUAL(3, v);
        TEST_ASSERT_LESS_OR_EQUAL(7, v);
    }
    TEST_ASSERT_EQUAL_UINT8(5, queens_rng_range(&st, 5, 5));
}

/*--- Generation -------------------------------------------------------------*/

void test_generate_all_difficulties_valid_and_unique(void)
{
    for (uint8_t d = 0; d < QUEENS_DIFF_COUNT; d++)
    {
        uint8_t n = queens_diff_size[d];
        for (uint32_t seed = 1; seed <= 20; seed++)
        {
            queens_puzzle_t pz;
            uint32_t rng;
            queens_rng_seed(&rng, seed * 2654435761u + d);

            TEST_ASSERT_TRUE_MESSAGE(queens_generate(&pz, n, &rng),
                                     "generation failed");
            TEST_ASSERT_EQUAL_UINT8(n, pz.n);
            TEST_ASSERT_TRUE_MESSAGE(pz.unique, "solution not unique");

            assert_solution_valid(&pz);
            assert_regions_valid(&pz);
            assert_patterns_valid(&pz);

            uint8_t region_size[QUEENS_MAX_N] = { 0 };
            uint8_t small_regions = 0;
            for (uint8_t r = 0; r < n; r++)
                for (uint8_t c = 0; c < n; c++)
                    region_size[pz.region[r][c]]++;
            for (uint8_t reg = 0; reg < n; reg++)
            {
                if (d >= 2)
                    TEST_ASSERT_GREATER_OR_EQUAL_UINT8_MESSAGE(2, region_size[reg],
                        "Hard/Expert singleton region");
                if (region_size[reg] <= 2)
                    small_regions++;
            }
            if (d >= 2)
                TEST_ASSERT_LESS_OR_EQUAL_UINT8_MESSAGE(n / 4u, small_regions,
                    "too many small regions for difficulty");

            TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, queens_count_solutions(&pz, 2),
                                            "solver disagrees on uniqueness");
        }
    }
}

void test_generate_rejects_bad_size(void)
{
    queens_puzzle_t pz;
    uint32_t rng;
    queens_rng_seed(&rng, 1);
    TEST_ASSERT_FALSE(queens_generate(&pz, 3, &rng));
    TEST_ASSERT_FALSE(queens_generate(&pz, QUEENS_MAX_N + 1, &rng));
    TEST_ASSERT_FALSE(queens_generate(NULL, 6, &rng));
    TEST_ASSERT_FALSE(queens_generate(&pz, 6, NULL));
}

void test_generate_varies_with_seed(void)
{
    queens_puzzle_t a, b;
    uint32_t rng;

    queens_rng_seed(&rng, 42);
    TEST_ASSERT_TRUE(queens_generate(&a, 8, &rng));
    queens_rng_seed(&rng, 43);
    TEST_ASSERT_TRUE(queens_generate(&b, 8, &rng));

    TEST_ASSERT_NOT_EQUAL(0, memcmp(a.region, b.region, sizeof(a.region)));
}

/*--- Solver -----------------------------------------------------------------*/

void test_solver_counts_two_solutions_on_handcrafted_board(void)
{
    /* 4x4, region id == column.  Exactly two placements satisfy
     * one-per-row/column with no touching: 1,3,0,2 and 2,0,3,1. */
    queens_puzzle_t pz;
    memset(&pz, 0, sizeof(pz));
    pz.n = 4;
    for (uint8_t r = 0; r < 4; r++)
        for (uint8_t c = 0; c < 4; c++)
            pz.region[r][c] = c;

    TEST_ASSERT_EQUAL_UINT8(2, queens_count_solutions(&pz, 10));
    TEST_ASSERT_EQUAL_UINT8(1, queens_count_solutions(&pz, 1)); /* limit caps */
    TEST_ASSERT_EQUAL_UINT8(0, queens_count_solutions(&pz, 0));
}

void test_solver_zero_solutions_when_regions_block(void)
{
    /* 4x4, single region: impossible to place 4 queens in one region. */
    queens_puzzle_t pz;
    memset(&pz, 0, sizeof(pz));
    pz.n = 4;

    TEST_ASSERT_EQUAL_UINT8(0, queens_count_solutions(&pz, 2));
}

/*--- Conflicts / win ---------------------------------------------------------*/

void test_conflicts_detect_each_rule(void)
{
    queens_puzzle_t pz;
    uint32_t rng;
    queens_rng_seed(&rng, 7);
    TEST_ASSERT_TRUE(queens_generate(&pz, 8, &rng));
    uint8_t n = pz.n;

    /* Baseline: the stored solution has zero conflicts and solves */
    uint8_t cells[QUEENS_MAX_N][QUEENS_MAX_N];
    solution_to_cells(&pz, cells);
    TEST_ASSERT_EQUAL_UINT8(0, queens_find_conflicts(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, NULL));
    TEST_ASSERT_TRUE(queens_is_solved(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells));

    /* Missing queen: not solved */
    cells[0][pz.solution_col[0]] = QUEENS_CELL_EMPTY;
    TEST_ASSERT_FALSE(queens_is_solved(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells));

    /* Same row: two queens in row 0 */
    memset(cells, 0, sizeof(cells));
    cells[0][0] = QUEENS_CELL_QUEEN;
    cells[0][2] = QUEENS_CELL_QUEEN;
    bool conf[QUEENS_MAX_N][QUEENS_MAX_N];
    TEST_ASSERT_EQUAL_UINT8(2, queens_find_conflicts(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, conf));
    TEST_ASSERT_TRUE(conf[0][0]);
    TEST_ASSERT_TRUE(conf[0][2]);

    /* Same column */
    memset(cells, 0, sizeof(cells));
    cells[0][3] = QUEENS_CELL_QUEEN;
    cells[4][3] = QUEENS_CELL_QUEEN;
    TEST_ASSERT_EQUAL_UINT8(2, queens_find_conflicts(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, NULL));

    /* Diagonal touch (no shared row/column) */
    memset(cells, 0, sizeof(cells));
    cells[2][2] = QUEENS_CELL_QUEEN;
    cells[3][3] = QUEENS_CELL_QUEEN;
    TEST_ASSERT_EQUAL_UINT8(2, queens_find_conflicts(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, NULL));

    /* Orthogonal touch */
    memset(cells, 0, sizeof(cells));
    cells[2][2] = QUEENS_CELL_QUEEN;
    cells[2][3] = QUEENS_CELL_QUEEN;
    TEST_ASSERT_EQUAL_UINT8(2, queens_find_conflicts(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, NULL));

    /* Same region, far apart: find a region with 2+ cells (single-cell
     * regions are legal and cannot host two queens) */
    memset(cells, 0, sizeof(cells));
    uint8_t placed = 0;
    for (uint8_t want = 0; want < n && placed < 2; want++)
    {
        uint8_t cnt = 0;
        for (uint8_t r = 0; r < n; r++)
            for (uint8_t c = 0; c < n; c++)
                if (pz.region[r][c] == want)
                    cnt++;
        if (cnt < 2)
            continue;
        for (uint8_t r = 0; r < n && placed < 2; r++)
            for (uint8_t c = 0; c < n && placed < 2; c++)
                if (pz.region[r][c] == want)
                {
                    cells[r][c] = QUEENS_CELL_QUEEN;
                    placed++;
                }
    }
    TEST_ASSERT_EQUAL_UINT8(2, placed);   /* a 2+-cell region must exist */
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(2, queens_find_conflicts(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, NULL));

    /* Marked cells never conflict */
    memset(cells, 0, sizeof(cells));
    cells[0][0] = QUEENS_CELL_MARK;
    cells[0][1] = QUEENS_CELL_MARK;
    TEST_ASSERT_EQUAL_UINT8(0, queens_find_conflicts(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, NULL));
}

void test_wrong_queen_position_alone_is_not_a_conflict(void)
{
    /* The spec: a queen on an incorrect cell (vs the stored solution) must
     * NOT be flagged as long as it does not invalidate other placed queens.
     * One lone queen anywhere never conflicts. */
    queens_puzzle_t pz;
    uint32_t rng;
    queens_rng_seed(&rng, 11);
    TEST_ASSERT_TRUE(queens_generate(&pz, 6, &rng));

    uint8_t cells[QUEENS_MAX_N][QUEENS_MAX_N];
    memset(cells, 0, sizeof(cells));
    uint8_t wrong = (uint8_t)((pz.solution_col[0] + 3) % pz.n);
    cells[0][wrong] = QUEENS_CELL_QUEEN;

    TEST_ASSERT_EQUAL_UINT8(0, queens_find_conflicts(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells, NULL));
    TEST_ASSERT_FALSE(queens_is_solved(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells));
}

/*--- Auto-exclude ------------------------------------------------------------*/

void test_auto_exclude_covers_row_col_region_surroundings(void)
{
    queens_puzzle_t pz;
    uint32_t rng;
    queens_rng_seed(&rng, 5);
    TEST_ASSERT_TRUE(queens_generate(&pz, 8, &rng));
    uint8_t n = pz.n;

    uint8_t cells[QUEENS_MAX_N][QUEENS_MAX_N];
    bool excl[QUEENS_MAX_N][QUEENS_MAX_N];
    memset(cells, 0, sizeof(cells));

    uint8_t qr = 3, qc = 3;
    cells[qr][qc] = QUEENS_CELL_QUEEN;
    queens_auto_exclude(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells, excl);

    uint8_t reg = pz.region[qr][qc];
    for (uint8_t r = 0; r < n; r++)
    {
        for (uint8_t c = 0; c < n; c++)
        {
            bool expect = (r == qr) || (c == qc) || (pz.region[r][c] == reg);
            if (!expect)
            {
                uint8_t dr = (r > qr) ? r - qr : qr - r;
                uint8_t dc = (c > qc) ? c - qc : qc - c;
                expect = (dr <= 1 && dc <= 1);
            }
            if (r == qr && c == qc)
                expect = false;   /* queen cell itself is never excluded */
            TEST_ASSERT_EQUAL_MESSAGE(expect, excl[r][c], "exclude mismatch");
        }
    }
}

void test_auto_exclude_empty_board_excludes_nothing(void)
{
    queens_puzzle_t pz;
    uint32_t rng;
    queens_rng_seed(&rng, 5);
    TEST_ASSERT_TRUE(queens_generate(&pz, 6, &rng));

    uint8_t cells[QUEENS_MAX_N][QUEENS_MAX_N];
    bool excl[QUEENS_MAX_N][QUEENS_MAX_N];
    memset(cells, 0, sizeof(cells));

    queens_auto_exclude(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells, excl);
    for (uint8_t r = 0; r < pz.n; r++)
        for (uint8_t c = 0; c < pz.n; c++)
            TEST_ASSERT_FALSE(excl[r][c]);
}

void test_all_decided(void)
{
    queens_puzzle_t pz;
    uint32_t rng;
    queens_rng_seed(&rng, 5);
    TEST_ASSERT_TRUE(queens_generate(&pz, 6, &rng));

    uint8_t cells[QUEENS_MAX_N][QUEENS_MAX_N];
    bool excl[QUEENS_MAX_N][QUEENS_MAX_N];
    memset(cells, 0, sizeof(cells));
    memset(excl, 0, sizeof(excl));

    TEST_ASSERT_FALSE(queens_all_decided(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells,
        (const bool (*)[QUEENS_MAX_N])excl));

    /* Fill everything with marks except one auto-excluded cell */
    memset(cells, QUEENS_CELL_MARK, sizeof(cells));
    cells[0][0] = QUEENS_CELL_EMPTY;
    excl[0][0] = true;
    TEST_ASSERT_TRUE(queens_all_decided(&pz,
        (const uint8_t (*)[QUEENS_MAX_N])cells,
        (const bool (*)[QUEENS_MAX_N])excl));
}

/*--- High scores --------------------------------------------------------------*/

void test_initials_default_to_hpx_and_preserve_recent_value(void)
{
    char initials[4];

    memset(initials, 0, sizeof(initials));
    queens_sanitize_initials(initials);
    TEST_ASSERT_EQUAL_STRING(QUEENS_DEFAULT_INITIALS, initials);

    memcpy(initials, "bob", 4);
    queens_sanitize_initials(initials);
    TEST_ASSERT_EQUAL_STRING("BOB", initials);

    memcpy(initials, "Q", 2);
    queens_sanitize_initials(initials);
    TEST_ASSERT_EQUAL_STRING("QAA", initials);
}

static queens_hs_entry_t mk_entry(const char *ini, uint32_t secs, uint32_t date,
                                  uint8_t nm)
{
    queens_hs_entry_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.initials, ini, 3);
    e.seconds = secs;
    e.date = date;
    e.no_mistakes = nm;
    e.used = 1;
    return e;
}

void test_hs_insert_ranks_and_truncates(void)
{
    queens_hs_table_t t;
    queens_hs_init(&t);

    queens_hs_entry_t a = mk_entry("AAA", 100, 20261024, 1);
    queens_hs_entry_t b = mk_entry("BOB", 80, 20261023, 0);
    queens_hs_entry_t c = mk_entry("CAT", 120, 20261022, 0);
    queens_hs_entry_t d = mk_entry("DAN", 90, 20261021, 1);
    queens_hs_entry_t e = mk_entry("EVE", 200, 20261020, 0);

    TEST_ASSERT_EQUAL_INT(0, queens_hs_insert(&t, 1, &a));
    TEST_ASSERT_EQUAL_INT(0, queens_hs_insert(&t, 1, &b));   /* faster: new #1 */
    TEST_ASSERT_EQUAL_INT(2, queens_hs_insert(&t, 1, &c));
    TEST_ASSERT_EQUAL_INT(1, queens_hs_insert(&t, 1, &d));
    TEST_ASSERT_EQUAL_INT(-1, queens_hs_insert(&t, 1, &e));  /* too slow */

    TEST_ASSERT_EQUAL_STRING("BOB", t.e[1][0].initials);
    TEST_ASSERT_EQUAL_UINT32(80, t.e[1][0].seconds);
    TEST_ASSERT_EQUAL_STRING("DAN", t.e[1][1].initials);
    TEST_ASSERT_EQUAL_STRING("AAA", t.e[1][2].initials);
    TEST_ASSERT_EQUAL_UINT8(1, t.e[1][0].used);
    TEST_ASSERT_EQUAL_UINT8(0, t.e[1][0].no_mistakes);
    TEST_ASSERT_EQUAL_UINT8(1, t.e[1][2].no_mistakes);
    TEST_ASSERT_EQUAL_UINT32(20261024, t.e[1][2].date);
}

void test_hs_tables_are_per_difficulty(void)
{
    queens_hs_table_t t;
    queens_hs_init(&t);

    queens_hs_entry_t a = mk_entry("AAA", 50, 20261024, 1);
    TEST_ASSERT_EQUAL_INT(0, queens_hs_insert(&t, 0, &a));
    TEST_ASSERT_EQUAL_INT(0, queens_hs_insert(&t, 3, &a));
    TEST_ASSERT_EQUAL_UINT8(0, t.e[1][0].used);
    TEST_ASSERT_EQUAL_UINT8(0, t.e[2][0].used);

    TEST_ASSERT_EQUAL_INT(-1, queens_hs_insert(&t, QUEENS_DIFF_COUNT, &a));
    TEST_ASSERT_EQUAL_INT(-1, queens_hs_insert(&t, 0, NULL));
}

void test_hs_format_parse_roundtrip(void)
{
    queens_hs_entry_t in = mk_entry("ZOE", 754, 20261024, 1);
    char buf[32];
    queens_hs_format(&in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("ZOE,754,20261024,1", buf);

    queens_hs_entry_t out;
    TEST_ASSERT_TRUE(queens_hs_parse(buf, &out));
    TEST_ASSERT_EQUAL_STRING("ZOE", out.initials);
    TEST_ASSERT_EQUAL_UINT32(754, out.seconds);
    TEST_ASSERT_EQUAL_UINT32(20261024, out.date);
    TEST_ASSERT_EQUAL_UINT8(1, out.no_mistakes);
    TEST_ASSERT_EQUAL_UINT8(1, out.used);
}

void test_hs_parse_tolerant_variants(void)
{
    queens_hs_entry_t out;

    /* Unknown date stored as 0 */
    TEST_ASSERT_TRUE(queens_hs_parse("AB,42,0,0", &out));
    TEST_ASSERT_EQUAL_STRING("AB", out.initials);
    TEST_ASSERT_EQUAL_UINT32(0, out.date);
    TEST_ASSERT_EQUAL_UINT8(0, out.no_mistakes);

    /* Legacy line without the no-mistakes field */
    TEST_ASSERT_TRUE(queens_hs_parse("CAT,300,20260101", &out));
    TEST_ASSERT_EQUAL_UINT32(300, out.seconds);
    TEST_ASSERT_EQUAL_UINT8(0, out.no_mistakes);

    /* Legacy line with only initials+seconds */
    TEST_ASSERT_TRUE(queens_hs_parse("Q,77", &out));
    TEST_ASSERT_EQUAL_STRING("Q", out.initials);

    /* Garbage */
    TEST_ASSERT_FALSE(queens_hs_parse("", &out));
    TEST_ASSERT_FALSE(queens_hs_parse(",10,0,0", &out));
    TEST_ASSERT_FALSE(queens_hs_parse("NO COMMA", &out));
    TEST_ASSERT_FALSE(queens_hs_parse(NULL, &out));
}

void test_hs_parse_rejects_missing_or_zero_seconds(void)
{
    queens_hs_entry_t out;

    /* Missing/non-numeric/zero seconds must not load as an unbeatable
     * 0-second high score (win_seconds is clamped to >= 1) */
    TEST_ASSERT_FALSE(queens_hs_parse("AAA,", &out));
    TEST_ASSERT_FALSE(queens_hs_parse("AAA,garbage", &out));
    TEST_ASSERT_FALSE(queens_hs_parse("AAA,0,20261024,1", &out));

    /* Smallest legitimate time still parses */
    TEST_ASSERT_TRUE(queens_hs_parse("AAA,1,20261024,1", &out));
    TEST_ASSERT_EQUAL_UINT32(1, out.seconds);
}

void test_hs_parse_key(void)
{
    uint8_t d, r;
    const char *val = NULL;

    TEST_ASSERT_TRUE(queens_hs_parse_key("s3.2=AAA,95,20261024,1", &d, &r, &val));
    TEST_ASSERT_EQUAL_UINT8(3, d);
    TEST_ASSERT_EQUAL_UINT8(2, r);
    TEST_ASSERT_EQUAL_STRING("AAA,95,20261024,1", val);

    /* Other keys are not score keys */
    TEST_ASSERT_FALSE(queens_hs_parse_key("diff=1", &d, &r, &val));
    TEST_ASSERT_FALSE(queens_hs_parse_key("ax=1", &d, &r, &val));
    TEST_ASSERT_FALSE(queens_hs_parse_key("ini=AAA", &d, &r, &val));

    /* Truncated/corrupted tokens: rejected without reading past the NUL */
    TEST_ASSERT_FALSE(queens_hs_parse_key("s", NULL, NULL, NULL));
    TEST_ASSERT_FALSE(queens_hs_parse_key("s1", NULL, NULL, NULL));
    TEST_ASSERT_FALSE(queens_hs_parse_key("s1.", NULL, NULL, NULL));
    TEST_ASSERT_FALSE(queens_hs_parse_key("s1.2", NULL, NULL, NULL));
    TEST_ASSERT_FALSE(queens_hs_parse_key("sx.0=", NULL, NULL, NULL));
    TEST_ASSERT_FALSE(queens_hs_parse_key("s1.x=", NULL, NULL, NULL));

    /* Out-of-range indices */
    TEST_ASSERT_FALSE(queens_hs_parse_key("s4.0=", NULL, NULL, NULL));
    TEST_ASSERT_FALSE(queens_hs_parse_key("s0.3=", NULL, NULL, NULL));

    TEST_ASSERT_FALSE(queens_hs_parse_key(NULL, NULL, NULL, NULL));
}

/*--- Stuck-alert deferral -----------------------------------------------------*/

void test_stuck_penalty_truth_table(void)
{
    /* Penalty only when a board that was already all-decided is still
     * all-decided (and unsolved) after the next cell change */
    TEST_ASSERT_FALSE(queens_stuck_penalty(false, false));
    TEST_ASSERT_FALSE(queens_stuck_penalty(false, true));   /* first sighting: deferred */
    TEST_ASSERT_TRUE(queens_stuck_penalty(true, true));     /* unresolved: penalize */
    TEST_ASSERT_FALSE(queens_stuck_penalty(true, false));   /* resolved */
}

/* Regression: completing a correct board through normal OK cycles (EMPTY ->
 * MARK -> QUEEN on each solution cell, auto-exclude on) must never record a
 * mistake.  With N-1 correct queens placed, auto-exclude decides every cell
 * except the final solution cell, so the final queen's intermediate MARK
 * step makes the board all-decided-but-unsolved for one step. */
void test_correct_playthrough_never_penalizes_intermediate_mark(void)
{
    queens_puzzle_t pz;
    uint32_t rng;
    queens_rng_seed(&rng, 1234);
    TEST_ASSERT_TRUE(queens_generate(&pz, 8, &rng));

    uint8_t cells[QUEENS_MAX_N][QUEENS_MAX_N];
    bool excl[QUEENS_MAX_N][QUEENS_MAX_N];
    memset(cells, 0, sizeof(cells));

    bool was_stuck = false, penalized = false, naive_penalized = false;
    bool saw_intermediate_stuck = false;

    for (uint8_t step = 0; step < pz.n * 2u; step++)
    {
        uint8_t r = step / 2u;
        uint8_t c = pz.solution_col[r];

        /* First half of each row's cycle marks, second half places */
        cells[r][c] = (step % 2u == 0u) ? QUEENS_CELL_MARK : QUEENS_CELL_QUEEN;

        queens_auto_exclude(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells, excl);
        bool solved = queens_is_solved(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells);
        bool stuck = !solved && queens_all_decided(&pz,
                                                   (const uint8_t (*)[QUEENS_MAX_N])cells,
                                                   (const bool (*)[QUEENS_MAX_N])excl);
        if (stuck)
            saw_intermediate_stuck = true;
        if (queens_stuck_penalty(was_stuck, stuck))
            penalized = true;
        if (stuck && !was_stuck)
            naive_penalized = true;   /* what the pre-fix policy would do */
        was_stuck = stuck;
    }

    TEST_ASSERT_TRUE(queens_is_solved(&pz, (const uint8_t (*)[QUEENS_MAX_N])cells));
    TEST_ASSERT_TRUE_MESSAGE(saw_intermediate_stuck,
                             "test did not exercise the risky MARK step");
    TEST_ASSERT_FALSE_MESSAGE(penalized,
                              "intermediate MARK wrongly counted as a mistake");
    TEST_ASSERT_TRUE_MESSAGE(naive_penalized,
                             "sanity: naive policy would have penalized here");
}

/*--- main ----------------------------------------------------------------------*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_pattern_visibility_follows_shaded_density);
    RUN_TEST(test_dynamic_patterns_fill_queen_regions_and_reuse_best_remaining);

    RUN_TEST(test_rng_seed_zero_is_remapped);
    RUN_TEST(test_rng_deterministic);
    RUN_TEST(test_rng_range_bounds);

    RUN_TEST(test_generate_all_difficulties_valid_and_unique);
    RUN_TEST(test_generate_rejects_bad_size);
    RUN_TEST(test_generate_varies_with_seed);

    RUN_TEST(test_solver_counts_two_solutions_on_handcrafted_board);
    RUN_TEST(test_solver_zero_solutions_when_regions_block);

    RUN_TEST(test_conflicts_detect_each_rule);
    RUN_TEST(test_wrong_queen_position_alone_is_not_a_conflict);

    RUN_TEST(test_auto_exclude_covers_row_col_region_surroundings);
    RUN_TEST(test_auto_exclude_empty_board_excludes_nothing);
    RUN_TEST(test_all_decided);

    RUN_TEST(test_initials_default_to_hpx_and_preserve_recent_value);
    RUN_TEST(test_hs_insert_ranks_and_truncates);
    RUN_TEST(test_hs_tables_are_per_difficulty);
    RUN_TEST(test_hs_format_parse_roundtrip);
    RUN_TEST(test_hs_parse_tolerant_variants);
    RUN_TEST(test_hs_parse_rejects_missing_or_zero_seconds);
    RUN_TEST(test_hs_parse_key);

    RUN_TEST(test_stuck_penalty_truth_table);
    RUN_TEST(test_correct_playthrough_never_penalizes_intermediate_mark);

    return UNITY_END();
}
