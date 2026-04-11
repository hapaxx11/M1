/* See COPYING.txt for license details. */

/*
 * test_subghz_raw_line_parser.c
 *
 * Unit tests for subghz_raw_line_parser — the RAW_Data line parser
 * extracted from sub_ghz_replay_flipper_file().
 *
 * Tests cover:
 *   - Normal positive values
 *   - Negative → absolute conversion
 *   - Zero skipping
 *   - Cross-buffer leftover digit recombination
 *   - Empty / whitespace-only lines
 *   - Output count limits
 *   - Multiple continuation calls
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>
#include "unity.h"
#include "subghz_raw_line_parser.h"

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * Basic parsing
 * =================================================================== */

void test_parse_positive_values(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];
    uint16_t count = subghz_parse_raw_data_line(
        " 350 1050 350 1050", true, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(4, count);
    TEST_ASSERT_EQUAL_UINT32(350,  out[0]);
    TEST_ASSERT_EQUAL_UINT32(1050, out[1]);
    TEST_ASSERT_EQUAL_UINT32(350,  out[2]);
    TEST_ASSERT_EQUAL_UINT32(1050, out[3]);
}

void test_parse_negative_to_absolute(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];
    uint16_t count = subghz_parse_raw_data_line(
        " -350 1050 -350 1050", true, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(4, count);
    TEST_ASSERT_EQUAL_UINT32(350,  out[0]);
    TEST_ASSERT_EQUAL_UINT32(1050, out[1]);
    TEST_ASSERT_EQUAL_UINT32(350,  out[2]);
    TEST_ASSERT_EQUAL_UINT32(1050, out[3]);
}

void test_parse_mixed_sign(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];
    uint16_t count = subghz_parse_raw_data_line(
        " 350 -1050 350 -1050", true, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(4, count);
    TEST_ASSERT_EQUAL_UINT32(350,  out[0]);
    TEST_ASSERT_EQUAL_UINT32(1050, out[1]);
    TEST_ASSERT_EQUAL_UINT32(350,  out[2]);
    TEST_ASSERT_EQUAL_UINT32(1050, out[3]);
}

/* ===================================================================
 * Zero skipping
 * =================================================================== */

void test_parse_zero_skipped(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];
    uint16_t count = subghz_parse_raw_data_line(
        " 350 0 1050 0 350", true, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(3, count);
    TEST_ASSERT_EQUAL_UINT32(350,  out[0]);
    TEST_ASSERT_EQUAL_UINT32(1050, out[1]);
    TEST_ASSERT_EQUAL_UINT32(350,  out[2]);
}

void test_parse_all_zeros(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];
    uint16_t count = subghz_parse_raw_data_line(
        " 0 0 0", true, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(0, count);
}

/* ===================================================================
 * Empty / whitespace input
 * =================================================================== */

void test_parse_empty_string(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];
    uint16_t count = subghz_parse_raw_data_line(
        "", true, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(0, count);
}

void test_parse_whitespace_only(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];
    uint16_t count = subghz_parse_raw_data_line(
        "   ", true, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(0, count);
}

/* ===================================================================
 * Output buffer limit
 * =================================================================== */

void test_parse_respects_max_samples(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[2];
    uint16_t count = subghz_parse_raw_data_line(
        " 100 200 300 400 500", true, &state, out, 2);

    TEST_ASSERT_EQUAL_UINT16(2, count);
    TEST_ASSERT_EQUAL_UINT32(100, out[0]);
    TEST_ASSERT_EQUAL_UINT32(200, out[1]);
}

/* ===================================================================
 * Cross-buffer leftover digit recombination
 * =================================================================== */

void test_leftover_digit_recombination(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];

    /* First call: line truncated, "123" at end is incomplete
     * line_complete=false → last number saved as leftover */
    uint16_t c1 = subghz_parse_raw_data_line(
        " 350 1050 123", false, &state, out, 10);

    /* "350" and "1050" are complete, "123" is saved as leftover */
    TEST_ASSERT_EQUAL_UINT16(2, c1);
    TEST_ASSERT_EQUAL_UINT32(350,  out[0]);
    TEST_ASSERT_EQUAL_UINT32(1050, out[1]);

    /* Second call: continuation with "45 678" — leftover "123" + "45" → 12345 */
    uint16_t c2 = subghz_parse_raw_data_line(
        "45 678", true, &state, &out[c1], 10 - c1);

    TEST_ASSERT_EQUAL_UINT16(2, c2);
    TEST_ASSERT_EQUAL_UINT32(12345, out[2]);
    TEST_ASSERT_EQUAL_UINT32(678,   out[3]);
}

void test_leftover_negative_recombination(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];

    /* Line truncated at "-12" → saved as absolute "12" */
    uint16_t c1 = subghz_parse_raw_data_line(
        " 350 -12", false, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(1, c1);
    TEST_ASSERT_EQUAL_UINT32(350, out[0]);

    /* Continuation: "34 500" → leftover "12" + "34" → 1234 */
    uint16_t c2 = subghz_parse_raw_data_line(
        "34 500", true, &state, &out[c1], 10 - c1);

    TEST_ASSERT_EQUAL_UINT16(2, c2);
    TEST_ASSERT_EQUAL_UINT32(1234, out[1]);
    TEST_ASSERT_EQUAL_UINT32(500,  out[2]);
}

void test_no_leftover_on_complete_line(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[10];

    /* Complete line — no leftover */
    uint16_t c1 = subghz_parse_raw_data_line(
        " 350 1050", true, &state, out, 10);

    TEST_ASSERT_EQUAL_UINT16(2, c1);

    /* Next call — no leftover to recombine */
    uint16_t c2 = subghz_parse_raw_data_line(
        " 200 400", true, &state, &out[c1], 10 - c1);

    TEST_ASSERT_EQUAL_UINT16(2, c2);
    TEST_ASSERT_EQUAL_UINT32(200, out[2]);
    TEST_ASSERT_EQUAL_UINT32(400, out[3]);
}

/* ===================================================================
 * Multiple continuation calls
 * =================================================================== */

void test_three_continuations(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[20];
    uint16_t total = 0;

    /* Chunk 1: truncated at "1" */
    total += subghz_parse_raw_data_line(
        " 100 200 1", false, &state, &out[total], 20 - total);
    TEST_ASSERT_EQUAL_UINT16(2, total);

    /* Chunk 2: "23 300 4" → "123" recombined, truncated at "4" */
    total += subghz_parse_raw_data_line(
        "23 300 4", false, &state, &out[total], 20 - total);
    TEST_ASSERT_EQUAL_UINT16(4, total);
    TEST_ASSERT_EQUAL_UINT32(123, out[2]);
    TEST_ASSERT_EQUAL_UINT32(300, out[3]);

    /* Chunk 3 (final): "56" → "456" recombined */
    total += subghz_parse_raw_data_line(
        "56", true, &state, &out[total], 20 - total);
    TEST_ASSERT_EQUAL_UINT16(5, total);
    TEST_ASSERT_EQUAL_UINT32(456, out[4]);
}

/* ===================================================================
 * Null safety
 * =================================================================== */

void test_null_text_returns_zero(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);
    uint32_t out[10];

    TEST_ASSERT_EQUAL_UINT16(0,
        subghz_parse_raw_data_line(NULL, true, &state, out, 10));
}

void test_null_state_returns_zero(void)
{
    uint32_t out[10];
    TEST_ASSERT_EQUAL_UINT16(0,
        subghz_parse_raw_data_line(" 350", true, NULL, out, 10));
}

/* ===================================================================
 * Large values (Flipper RAW can have values > 65535)
 * =================================================================== */

void test_parse_large_values(void)
{
    SubGhzRawLineState state;
    subghz_raw_line_state_init(&state);

    uint32_t out[5];
    uint16_t count = subghz_parse_raw_data_line(
        " 350 -100000 350", true, &state, out, 5);

    TEST_ASSERT_EQUAL_UINT16(3, count);
    TEST_ASSERT_EQUAL_UINT32(350,    out[0]);
    TEST_ASSERT_EQUAL_UINT32(100000, out[1]);
    TEST_ASSERT_EQUAL_UINT32(350,    out[2]);
}

/* ===================================================================
 * State init
 * =================================================================== */

void test_state_init_clears_leftover(void)
{
    SubGhzRawLineState state;
    memset(state.leftover, 'X', sizeof(state.leftover));
    subghz_raw_line_state_init(&state);
    TEST_ASSERT_EQUAL_CHAR('\0', state.leftover[0]);
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Basic parsing */
    RUN_TEST(test_parse_positive_values);
    RUN_TEST(test_parse_negative_to_absolute);
    RUN_TEST(test_parse_mixed_sign);

    /* Zero skipping */
    RUN_TEST(test_parse_zero_skipped);
    RUN_TEST(test_parse_all_zeros);

    /* Empty input */
    RUN_TEST(test_parse_empty_string);
    RUN_TEST(test_parse_whitespace_only);

    /* Buffer limit */
    RUN_TEST(test_parse_respects_max_samples);

    /* Leftover recombination */
    RUN_TEST(test_leftover_digit_recombination);
    RUN_TEST(test_leftover_negative_recombination);
    RUN_TEST(test_no_leftover_on_complete_line);

    /* Multiple continuations */
    RUN_TEST(test_three_continuations);

    /* Null safety */
    RUN_TEST(test_null_text_returns_zero);
    RUN_TEST(test_null_state_returns_zero);

    /* Large values */
    RUN_TEST(test_parse_large_values);

    /* State init */
    RUN_TEST(test_state_init_clears_leftover);

    return UNITY_END();
}
