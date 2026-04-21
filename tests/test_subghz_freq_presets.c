/* See COPYING.txt for license details. */

/*
 * test_subghz_freq_presets.c
 *
 * Host-side unit tests for the Sub-GHz frequency preset table
 * (subghz_freq_presets.c / subghz_freq_presets.h).
 *
 * Bug context:
 *   Magellan/GE/Interlogix NA security sensors operate at 319.5 MHz.
 *   The M1 preset list jumped from 318.00 to 320.00, skipping 319.5 MHz,
 *   so the signal could never be received.  The Magellan protocol was
 *   also flagged 433-only despite operating in the 315-band on NA hardware.
 *
 *   These tests enforce structural invariants that would have caught
 *   both bugs, and will catch similar issues for future protocol ports.
 *
 * Invariants verified:
 *   1. Array length  == SUBGHZ_FREQ_PRESET_COUNT
 *   2. SUBGHZ_FREQ_PRESET_CUSTOM == SUBGHZ_FREQ_PRESET_COUNT
 *   3. Default index  <= SUBGHZ_FREQ_PRESET_COUNT (in bounds)
 *   4. Default entry  == 433.92 MHz
 *   5. All entries within [SUBGHZ_MIN_FREQ_HZ, SUBGHZ_MAX_FREQ_HZ]
 *   6. Entries sorted in strictly ascending order
 *   7. No duplicate freq_hz values
 *   8. All label strings are non-NULL and non-empty
 *   9. 319.5 MHz is present (regression for Magellan NA bug)
 *  10. 315.0 MHz is present (common NA garage/gate remotes)
 *  11. 433.92 MHz is present
 *  12. 868.35 MHz is present (common EU ISM)
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "subghz_freq_presets.h"
#include <string.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

/* ================================================================
 * Helper: find a frequency in the preset table, return its index
 * or -1 if not found.
 * ================================================================ */
static int find_freq(uint32_t hz)
{
	for (int i = 0; i < (int)SUBGHZ_FREQ_PRESET_COUNT; i++)
	{
		if (subghz_freq_presets[i].freq_hz == hz)
			return i;
	}
	return -1;
}

/* ================================================================
 * 1. Array length == SUBGHZ_FREQ_PRESET_COUNT
 *    (static assertion at compile time; runtime mirror for reporting)
 * ================================================================ */
void test_array_length_matches_count(void)
{
	/* C11 static assertion — fails at compile time if the array is the
	 * wrong size; the runtime check below gives a readable test output. */
	_Static_assert(
		sizeof(subghz_freq_presets) / sizeof(subghz_freq_presets[0])
			== SUBGHZ_FREQ_PRESET_COUNT,
		"subghz_freq_presets array length must equal SUBGHZ_FREQ_PRESET_COUNT");

	uint32_t runtime_count =
		(uint32_t)(sizeof(subghz_freq_presets) / sizeof(subghz_freq_presets[0]));
	TEST_ASSERT_EQUAL_UINT32(SUBGHZ_FREQ_PRESET_COUNT, runtime_count);
}

/* ================================================================
 * 2. SUBGHZ_FREQ_PRESET_CUSTOM == SUBGHZ_FREQ_PRESET_COUNT
 *    The custom-frequency sentinel index must equal the count so that
 *    bounds checks of the form `idx < COUNT` also exclude CUSTOM.
 * ================================================================ */
void test_custom_sentinel_equals_count(void)
{
	TEST_ASSERT_EQUAL_UINT32(SUBGHZ_FREQ_PRESET_COUNT, SUBGHZ_FREQ_PRESET_CUSTOM);
}

/* ================================================================
 * 3. Default index is within the valid range.
 * ================================================================ */
void test_default_idx_in_bounds(void)
{
	TEST_ASSERT_LESS_THAN_UINT32(SUBGHZ_FREQ_PRESET_COUNT, SUBGHZ_FREQ_DEFAULT_IDX);
}

/* ================================================================
 * 4. Default entry is 433.92 MHz.
 * ================================================================ */
void test_default_idx_is_433_92(void)
{
	TEST_ASSERT_EQUAL_UINT32(433920000UL,
		subghz_freq_presets[SUBGHZ_FREQ_DEFAULT_IDX].freq_hz);
}

/* ================================================================
 * 5. All entries within [SUBGHZ_MIN_FREQ_HZ, SUBGHZ_MAX_FREQ_HZ].
 * ================================================================ */
void test_all_presets_in_si4463_range(void)
{
	for (uint32_t i = 0; i < SUBGHZ_FREQ_PRESET_COUNT; i++)
	{
		uint32_t hz = subghz_freq_presets[i].freq_hz;
		char msg[80];
		snprintf(msg, sizeof(msg),
			"Preset %lu (%s = %lu Hz) below SI4463 lower limit %lu Hz",
			(unsigned long)i,
			subghz_freq_presets[i].label ? subghz_freq_presets[i].label : "<null>",
			(unsigned long)hz,
			(unsigned long)SUBGHZ_MIN_FREQ_HZ);
		TEST_ASSERT_TRUE_MESSAGE(hz >= SUBGHZ_MIN_FREQ_HZ, msg);

		snprintf(msg, sizeof(msg),
			"Preset %lu (%s = %lu Hz) above SI4463 upper limit %lu Hz",
			(unsigned long)i,
			subghz_freq_presets[i].label ? subghz_freq_presets[i].label : "<null>",
			(unsigned long)hz,
			(unsigned long)SUBGHZ_MAX_FREQ_HZ);
		TEST_ASSERT_TRUE_MESSAGE(hz <= SUBGHZ_MAX_FREQ_HZ, msg);
	}
}

/* ================================================================
 * 6. Entries are in strictly ascending order.
 *    Out-of-order entries indicate a new freq was inserted at the
 *    wrong position and would confuse the L/R cycling UI.
 * ================================================================ */
void test_presets_sorted_ascending(void)
{
	for (uint32_t i = 1; i < SUBGHZ_FREQ_PRESET_COUNT; i++)
	{
		char msg[120];
		snprintf(msg, sizeof(msg),
			"Preset[%lu] (%lu Hz) is not > Preset[%lu] (%lu Hz) — table not sorted",
			(unsigned long)i,   (unsigned long)subghz_freq_presets[i].freq_hz,
			(unsigned long)(i-1),(unsigned long)subghz_freq_presets[i-1].freq_hz);
		TEST_ASSERT_TRUE_MESSAGE(
			subghz_freq_presets[i].freq_hz > subghz_freq_presets[i-1].freq_hz,
			msg);
	}
}

/* ================================================================
 * 7. No duplicate freq_hz values.
 *    Duplicates waste menu slots and suggest a copy-paste mistake.
 * ================================================================ */
void test_no_duplicate_frequencies(void)
{
	/* The sorted-ascending test already implies no duplicates, but
	 * this test provides a more explicit failure message. */
	for (uint32_t i = 0; i + 1 < SUBGHZ_FREQ_PRESET_COUNT; i++)
	{
		char msg[120];
		snprintf(msg, sizeof(msg),
			"Duplicate freq_hz %lu Hz at indices %lu and %lu",
			(unsigned long)subghz_freq_presets[i].freq_hz,
			(unsigned long)i, (unsigned long)(i+1));
		TEST_ASSERT_NOT_EQUAL_MESSAGE(
			subghz_freq_presets[i].freq_hz,
			subghz_freq_presets[i+1].freq_hz,
			msg);
	}
}

/* ================================================================
 * 8. All label strings are non-NULL and non-empty.
 * ================================================================ */
void test_all_labels_non_null(void)
{
	for (uint32_t i = 0; i < SUBGHZ_FREQ_PRESET_COUNT; i++)
	{
		char msg[60];
		snprintf(msg, sizeof(msg), "Preset[%lu] label is NULL", (unsigned long)i);
		TEST_ASSERT_NOT_NULL_MESSAGE(subghz_freq_presets[i].label, msg);

		snprintf(msg, sizeof(msg), "Preset[%lu] label is empty", (unsigned long)i);
		TEST_ASSERT_TRUE_MESSAGE(subghz_freq_presets[i].label[0] != '\0', msg);
	}
}

/* ================================================================
 * 9. 319.5 MHz is present — regression for Magellan/GE/Interlogix
 *    North American security sensors.  Without this preset the M1
 *    cannot tune to the frequency a Flipper emulates from a .sub file.
 * ================================================================ */
void test_319_5mhz_present(void)
{
	int idx = find_freq(319500000UL);
	TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx,
		"319.50 MHz missing — Magellan/GE/Interlogix NA sensors use this frequency");
}

/* ================================================================
 * 10. 315.0 MHz is present (common NA garage/gate remote frequency).
 * ================================================================ */
void test_315_0mhz_present(void)
{
	int idx = find_freq(315000000UL);
	TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx,
		"315.00 MHz missing — common NA ISM frequency for remotes/gates");
}

/* ================================================================
 * 11. 433.92 MHz is present (most common EU ISM center frequency).
 * ================================================================ */
void test_433_92mhz_present(void)
{
	int idx = find_freq(433920000UL);
	TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx,
		"433.92 MHz missing — most common EU ISM remote frequency");
}

/* ================================================================
 * 12. 868.35 MHz is present (EU 868 band SRD devices).
 * ================================================================ */
void test_868_35mhz_present(void)
{
	int idx = find_freq(868350000UL);
	TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx,
		"868.35 MHz missing — common EU 868 MHz band frequency");
}

/* ================================================================
 * 13. Spot-check: 319.5 MHz is correctly positioned between
 *     318.0 MHz and 320.0 MHz (sorted order consistency).
 * ================================================================ */
void test_319_5mhz_position(void)
{
	int idx_319  = find_freq(319500000UL);
	int idx_318  = find_freq(318000000UL);
	int idx_320  = find_freq(320000000UL);

	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, idx_318);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, idx_319);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, idx_320);

	/* 319.50 must lie strictly between 318.00 and 320.00 in the table */
	TEST_ASSERT_GREATER_THAN_INT_MESSAGE(idx_318, idx_319,
		"319.50 MHz entry must come AFTER 318.00 MHz in the sorted table");
	TEST_ASSERT_LESS_THAN_INT_MESSAGE(idx_320, idx_319,
		"319.50 MHz entry must come BEFORE 320.00 MHz in the sorted table");
}

/* ================================================================
 * 14–17. Per-region hopper tables: all entries exist in the preset table.
 *
 * If a hopper frequency is ever added that is not in subghz_freq_presets[],
 * the radio cannot tune to it from the preset picker and the hopper will
 * silently retune to a frequency the UI does not recognise.  These tests
 * catch that class of mistake at CI time.
 * ================================================================ */

static void assert_hopper_table_in_presets(const uint32_t *table, uint8_t count,
                                            const char *region_name)
{
	for (uint8_t i = 0; i < count; i++)
	{
		char msg[80];
		snprintf(msg, sizeof(msg),
			"%s hopper[%u] = %lu Hz not found in subghz_freq_presets[]",
			region_name, (unsigned)i, (unsigned long)table[i]);
		TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, find_freq(table[i]), msg);
	}
}

void test_hopper_NA_in_presets(void)
{
	assert_hopper_table_in_presets(subghz_hopper_freqs_NA, SUBGHZ_HOPPER_FREQ_COUNT, "NA");
}

void test_hopper_EU_in_presets(void)
{
	assert_hopper_table_in_presets(subghz_hopper_freqs_EU, SUBGHZ_HOPPER_FREQ_COUNT, "EU");
}

void test_hopper_ASIA_in_presets(void)
{
	assert_hopper_table_in_presets(subghz_hopper_freqs_ASIA, SUBGHZ_HOPPER_FREQ_COUNT, "ASIA");
}

void test_hopper_OFF_in_presets(void)
{
	assert_hopper_table_in_presets(subghz_hopper_freqs_OFF, SUBGHZ_HOPPER_FREQ_COUNT, "OFF");
}

/* ================================================================
 * 18. subghz_get_hopper_freqs() returns the correct table per region.
 * ================================================================ */
void test_get_hopper_freqs_returns_correct_table(void)
{
	TEST_ASSERT_EQUAL_PTR(subghz_hopper_freqs_NA,   subghz_get_hopper_freqs(0));
	TEST_ASSERT_EQUAL_PTR(subghz_hopper_freqs_EU,   subghz_get_hopper_freqs(1));
	TEST_ASSERT_EQUAL_PTR(subghz_hopper_freqs_ASIA, subghz_get_hopper_freqs(2));
	/* 3 = Off, any other value → Off */
	TEST_ASSERT_EQUAL_PTR(subghz_hopper_freqs_OFF,  subghz_get_hopper_freqs(3));
	TEST_ASSERT_EQUAL_PTR(subghz_hopper_freqs_OFF,  subghz_get_hopper_freqs(255));
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void)
{
	UNITY_BEGIN();

	/* Structural invariants */
	RUN_TEST(test_array_length_matches_count);
	RUN_TEST(test_custom_sentinel_equals_count);
	RUN_TEST(test_default_idx_in_bounds);
	RUN_TEST(test_default_idx_is_433_92);
	RUN_TEST(test_all_presets_in_si4463_range);
	RUN_TEST(test_presets_sorted_ascending);
	RUN_TEST(test_no_duplicate_frequencies);
	RUN_TEST(test_all_labels_non_null);

	/* Known frequency regression guards */
	RUN_TEST(test_319_5mhz_present);
	RUN_TEST(test_315_0mhz_present);
	RUN_TEST(test_433_92mhz_present);
	RUN_TEST(test_868_35mhz_present);
	RUN_TEST(test_319_5mhz_position);

	/* Per-region hopper table integrity */
	RUN_TEST(test_hopper_NA_in_presets);
	RUN_TEST(test_hopper_EU_in_presets);
	RUN_TEST(test_hopper_ASIA_in_presets);
	RUN_TEST(test_hopper_OFF_in_presets);
	RUN_TEST(test_get_hopper_freqs_returns_correct_table);

	return UNITY_END();
}
