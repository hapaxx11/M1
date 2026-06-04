/* See COPYING.txt for license details. */

/**
 * test_ir_button_map.c
 *
 * Unit tests for the pure-logic button-to-command mapping in
 * ir_button_map.h / ir_button_map.c:
 *   ir_map_buttons() — map button specs to command indices via
 *                      bidirectional case-insensitive substring matching
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "ir_button_map.h"
#include <string.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * Null / empty guard tests
 * =========================================================================*/

void test_null_out_map_is_safe(void)
{
	const char *cmds[] = { "Power" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Power", alts } };
	/* Must not crash */
	ir_map_buttons(specs, 1, cmds, 1, NULL, 1);
}

void test_null_specs_fills_minus_one(void)
{
	const char *cmds[] = { "Power" };
	int8_t out[2] = { 99, 99 };
	ir_map_buttons(NULL, 1, cmds, 1, out, 2);
	TEST_ASSERT_EQUAL_INT8(-1, out[0]);
	TEST_ASSERT_EQUAL_INT8(-1, out[1]);
}

void test_null_cmd_names_fills_minus_one(void)
{
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Power", alts } };
	int8_t out[2] = { 99, 99 };
	ir_map_buttons(specs, 1, NULL, 1, out, 2);
	TEST_ASSERT_EQUAL_INT8(-1, out[0]);
	TEST_ASSERT_EQUAL_INT8(-1, out[1]);
}

void test_zero_btn_count_fills_minus_one(void)
{
	const char *cmds[] = { "Power" };
	int8_t out[3] = { 5, 5, 5 };
	ir_map_buttons(NULL, 0, cmds, 1, out, 3);
	TEST_ASSERT_EQUAL_INT8(-1, out[0]);
	TEST_ASSERT_EQUAL_INT8(-1, out[1]);
	TEST_ASSERT_EQUAL_INT8(-1, out[2]);
}

void test_zero_cmd_count_fills_minus_one(void)
{
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Power", alts } };
	int8_t out[2] = { 5, 5 };
	ir_map_buttons(specs, 1, NULL, 0, out, 2);
	TEST_ASSERT_EQUAL_INT8(-1, out[0]);
	TEST_ASSERT_EQUAL_INT8(-1, out[1]);
}

/* =========================================================================
 * Exact match
 * =========================================================================*/

void test_exact_match_single(void)
{
	const char *cmds[] = { "Mute", "Power", "Vol_up" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Power", alts } };
	int8_t out[1] = { 0 };
	ir_map_buttons(specs, 1, cmds, 3, out, 1);
	TEST_ASSERT_EQUAL_INT8(1, out[0]);  /* "Power" is at index 1 */
}

void test_exact_match_first_entry(void)
{
	const char *cmds[] = { "Power", "Mute", "Vol_up" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Power", alts } };
	int8_t out[1] = { -1 };
	ir_map_buttons(specs, 1, cmds, 3, out, 1);
	TEST_ASSERT_EQUAL_INT8(0, out[0]);
}

void test_exact_match_multiple_buttons(void)
{
	const char *cmds[] = { "Mute", "Power", "Vol_up", "Vol_dn" };
	const char * const *alts_null = NULL;
	ir_button_spec_t specs[] = {
		{ "Power",  alts_null },
		{ "Mute",   alts_null },
		{ "Vol_up", alts_null },
	};
	int8_t out[3] = { 0, 0, 0 };
	ir_map_buttons(specs, 3, cmds, 4, out, 3);
	TEST_ASSERT_EQUAL_INT8(1, out[0]);  /* Power → 1 */
	TEST_ASSERT_EQUAL_INT8(0, out[1]);  /* Mute  → 0 */
	TEST_ASSERT_EQUAL_INT8(2, out[2]);  /* Vol_up → 2 */
}

/* =========================================================================
 * Case-insensitive substring match (bidirectional)
 * =========================================================================*/

void test_icase_target_in_cmd(void)
{
	/* "Vol_up" is a substring of "TV_Vol_up" */
	const char *cmds[] = { "TV_Vol_up" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Vol_up", alts } };
	int8_t out[1] = { -1 };
	ir_map_buttons(specs, 1, cmds, 1, out, 1);
	TEST_ASSERT_EQUAL_INT8(0, out[0]);
}

void test_icase_cmd_in_target(void)
{
	/* "Power" is a substring of "Power_on" (command in target) */
	const char *cmds[] = { "Power" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Power_on", alts } };
	int8_t out[1] = { -1 };
	ir_map_buttons(specs, 1, cmds, 1, out, 1);
	TEST_ASSERT_EQUAL_INT8(0, out[0]);
}

void test_icase_case_folding(void)
{
	/* Case should not matter */
	const char *cmds[] = { "POWER" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "power", alts } };
	int8_t out[1] = { -1 };
	ir_map_buttons(specs, 1, cmds, 1, out, 1);
	TEST_ASSERT_EQUAL_INT8(0, out[0]);
}

void test_icase_no_match(void)
{
	/* "Sleep" should not match "Power" */
	const char *cmds[] = { "Power", "Mute" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Sleep", alts } };
	int8_t out[1] = { -1 };
	ir_map_buttons(specs, 1, cmds, 2, out, 1);
	TEST_ASSERT_EQUAL_INT8(-1, out[0]);
}

/* =========================================================================
 * Alt fallback
 * =========================================================================*/

void test_alt_fallback_used_when_primary_fails(void)
{
	/* Primary "Sleep" fails; alt "Power" should match index 0 */
	const char *cmds[] = { "Power", "Mute" };
	const char *alts_arr[] = { "Power", NULL };
	ir_button_spec_t specs[] = { { "Sleep", alts_arr } };
	int8_t out[1] = { -1 };
	ir_map_buttons(specs, 1, cmds, 2, out, 1);
	TEST_ASSERT_EQUAL_INT8(0, out[0]);
}

void test_alt_fallback_first_matching_alt_wins(void)
{
	/* Two alts; first matching one should be picked */
	const char *cmds[] = { "Vol_up", "Volume_Up" };
	const char *alts_arr[] = { "Volume_Up", "Vol_up", NULL };
	ir_button_spec_t specs[] = { { "No_Match", alts_arr } };
	int8_t out[1] = { -1 };
	ir_map_buttons(specs, 1, cmds, 2, out, 1);
	/* "Volume_Up" is at index 1 (exact match) */
	TEST_ASSERT_EQUAL_INT8(1, out[0]);
}

void test_alt_null_list_skipped(void)
{
	/* cmd_alts == NULL means no alternates — only primary tried */
	const char *cmds[] = { "Power" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Mute", alts } };
	int8_t out[1] = { 99 };
	ir_map_buttons(specs, 1, cmds, 1, out, 1);
	TEST_ASSERT_EQUAL_INT8(-1, out[0]);
}

void test_alt_empty_list_skipped(void)
{
	/* Empty alt list (first entry NULL) is safe */
	const char *alts_arr[] = { NULL };
	ir_button_spec_t specs[] = { { "Mute", alts_arr } };
	const char *cmds[] = { "Power" };
	int8_t out[1] = { 99 };
	ir_map_buttons(specs, 1, cmds, 1, out, 1);
	TEST_ASSERT_EQUAL_INT8(-1, out[0]);
}

/* =========================================================================
 * max_btns boundary — trailing slots always set to -1
 * =========================================================================*/

void test_max_btns_clears_trailing_slots(void)
{
	const char *cmds[] = { "Power" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "Power", alts } };
	int8_t out[4] = { 5, 5, 5, 5 };
	ir_map_buttons(specs, 1, cmds, 1, out, 4);
	TEST_ASSERT_EQUAL_INT8(0,  out[0]);  /* mapped */
	TEST_ASSERT_EQUAL_INT8(-1, out[1]);  /* trailing */
	TEST_ASSERT_EQUAL_INT8(-1, out[2]);  /* trailing */
	TEST_ASSERT_EQUAL_INT8(-1, out[3]);  /* trailing */
}

void test_btn_count_exceeds_max_btns_clips(void)
{
	/* btn_count (3) > max_btns (2); only first 2 slots are filled */
	const char *cmds[] = { "Power", "Mute", "Vol_up" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = {
		{ "Power",  alts },
		{ "Mute",   alts },
		{ "Vol_up", alts },
	};
	int8_t out[2] = { -1, -1 };
	ir_map_buttons(specs, 3, cmds, 3, out, 2);
	TEST_ASSERT_EQUAL_INT8(0, out[0]);  /* Power → 0 */
	TEST_ASSERT_EQUAL_INT8(1, out[1]);  /* Mute  → 1 */
}

/* =========================================================================
 * Exact match preferred over substring
 * =========================================================================*/

void test_exact_match_wins_over_substring(void)
{
	/* Both "NEC" and "NEC16" are in the table; "NEC" exact match
	 * should be picked first, not the substring match against "NEC16". */
	const char *cmds[] = { "NEC16", "NEC" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { "NEC", alts } };
	int8_t out[1] = { -1 };
	ir_map_buttons(specs, 1, cmds, 2, out, 1);
	TEST_ASSERT_EQUAL_INT8(1, out[0]);  /* exact "NEC" at index 1 */
}

/* =========================================================================
 * Multi-button realistic mapping
 * =========================================================================*/

void test_realistic_tv_remote_mapping(void)
{
	const char *cmds[] = {
		"Power",    /* 0 */
		"Vol_up",   /* 1 */
		"Vol_dn",   /* 2 */
		"Mute",     /* 3 */
		"Ch_up",    /* 4 */
		"Ch_dn",    /* 5 */
	};
	const char * const *no_alts = NULL;
	ir_button_spec_t specs[] = {
		{ "Power",  no_alts },  /* btn 0 → cmd 0 */
		{ "Vol_up", no_alts },  /* btn 1 → cmd 1 */
		{ "Vol_dn", no_alts },  /* btn 2 → cmd 2 */
		{ "Sleep",  no_alts },  /* btn 3 → no match → -1 */
		{ "Ch_up",  no_alts },  /* btn 4 → cmd 4 */
	};
	int8_t out[6] = { 0 };
	ir_map_buttons(specs, 5, cmds, 6, out, 6);
	TEST_ASSERT_EQUAL_INT8(0,  out[0]);
	TEST_ASSERT_EQUAL_INT8(1,  out[1]);
	TEST_ASSERT_EQUAL_INT8(2,  out[2]);
	TEST_ASSERT_EQUAL_INT8(-1, out[3]);
	TEST_ASSERT_EQUAL_INT8(4,  out[4]);
	TEST_ASSERT_EQUAL_INT8(-1, out[5]);  /* trailing slot beyond btn_count */
}

/* =========================================================================
 * Primary cmd_name == NULL (treated as no match)
 * =========================================================================*/

void test_null_primary_cmd_name_no_crash(void)
{
	const char *cmds[] = { "Power" };
	const char * const *alts = NULL;
	ir_button_spec_t specs[] = { { NULL, alts } };
	int8_t out[1] = { 99 };
	ir_map_buttons(specs, 1, cmds, 1, out, 1);
	TEST_ASSERT_EQUAL_INT8(-1, out[0]);
}

/*============================================================================*/
int main(void)
{
	UNITY_BEGIN();

	/* Null / empty guards */
	RUN_TEST(test_null_out_map_is_safe);
	RUN_TEST(test_null_specs_fills_minus_one);
	RUN_TEST(test_null_cmd_names_fills_minus_one);
	RUN_TEST(test_zero_btn_count_fills_minus_one);
	RUN_TEST(test_zero_cmd_count_fills_minus_one);

	/* Exact match */
	RUN_TEST(test_exact_match_single);
	RUN_TEST(test_exact_match_first_entry);
	RUN_TEST(test_exact_match_multiple_buttons);

	/* Case-insensitive substring match */
	RUN_TEST(test_icase_target_in_cmd);
	RUN_TEST(test_icase_cmd_in_target);
	RUN_TEST(test_icase_case_folding);
	RUN_TEST(test_icase_no_match);

	/* Alt fallback */
	RUN_TEST(test_alt_fallback_used_when_primary_fails);
	RUN_TEST(test_alt_fallback_first_matching_alt_wins);
	RUN_TEST(test_alt_null_list_skipped);
	RUN_TEST(test_alt_empty_list_skipped);

	/* max_btns boundary */
	RUN_TEST(test_max_btns_clears_trailing_slots);
	RUN_TEST(test_btn_count_exceeds_max_btns_clips);

	/* Exact match preferred over substring */
	RUN_TEST(test_exact_match_wins_over_substring);

	/* Realistic multi-button mapping */
	RUN_TEST(test_realistic_tv_remote_mapping);

	/* Null primary cmd_name */
	RUN_TEST(test_null_primary_cmd_name_no_crash);

	return UNITY_END();
}
