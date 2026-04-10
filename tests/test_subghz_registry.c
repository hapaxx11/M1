/* See COPYING.txt for license details. */

/*
 * test_subghz_registry.c
 *
 * Regression tests for subghz_protocol_registry.c — protocol lookup
 * and identification functions.
 *
 * Bug context:
 *   Before the registry, .sub file replay used a fragile strstr() chain
 *   to identify protocols by name.  This caused:
 *   1. Cham_Code (Chamberlain) fell through to "unsupported".
 *   2. Marantec24 (Static) was incorrectly caught by the rolling-code
 *      check via substring match on "Marantec" (which is Dynamic).
 *   3. All static protocols added in March 2026 (Clemsa, BETT, MegaCode,
 *      Centurion, Elro, Intertechno, Firefly, etc.) were missing from
 *      both strstr branches.
 *   The fix replaced strstr matching with registry-driven lookup via
 *   subghz_protocol_find_by_name(), which does exact string match.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "subghz_protocol_registry.h"
#include <string.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * subghz_protocol_find_by_name — exact match lookup
 * =================================================================== */

void test_find_princeton(void)
{
	int16_t idx = subghz_protocol_find_by_name("Princeton");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL_STRING("Princeton", proto->name);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_keeloq(void)
{
	int16_t idx = subghz_protocol_find_by_name("KeeLoq");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL_STRING("KeeLoq", proto->name);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeDynamic, proto->type);
}

void test_find_came(void)
{
	int16_t idx = subghz_protocol_find_by_name("CAME");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_nice_flo(void)
{
	int16_t idx = subghz_protocol_find_by_name("Nice FLO");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);
}

void test_find_holtek(void)
{
	int16_t idx = subghz_protocol_find_by_name("Holtek_HT12X");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);
}

void test_find_security_plus(void)
{
	int16_t idx = subghz_protocol_find_by_name("Security+ 2.0");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeDynamic, proto->type);
}

/* Bug regression: Marantec is Static.
 * With strstr, "Marantec24" matched "Marantec" and was incorrectly
 * treated as rolling-code/dynamic. The registry uses exact match so
 * "Marantec" (Static) and "Marantec24" (Static) are distinct entries. */
void test_find_marantec_exact(void)
{
	int16_t idx_m = subghz_protocol_find_by_name("Marantec");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx_m);

	const SubGhzProtocolDef *m = subghz_protocol_get((uint16_t)idx_m);
	TEST_ASSERT_NOT_NULL(m);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, m->type);
}

void test_find_weather_protocol(void)
{
	int16_t idx = subghz_protocol_find_by_name("Oregon v2");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeWeather, proto->type);
}

/* Bug regression: "not found" case must return -1 cleanly */
void test_find_not_found(void)
{
	TEST_ASSERT_EQUAL_INT16(-1, subghz_protocol_find_by_name("NonExistentProtocol"));
}

void test_find_null(void)
{
	TEST_ASSERT_EQUAL_INT16(-1, subghz_protocol_find_by_name(NULL));
}

/* Case sensitivity: registry uses strcmp, so case must match exactly */
void test_find_case_sensitive(void)
{
	/* "princeton" != "Princeton" */
	TEST_ASSERT_EQUAL_INT16(-1, subghz_protocol_find_by_name("princeton"));
}

/* ===================================================================
 * subghz_protocol_get — index-based access
 * =================================================================== */

void test_get_valid_index(void)
{
	const SubGhzProtocolDef *proto = subghz_protocol_get(0);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_NOT_NULL(proto->name);
}

void test_get_out_of_range(void)
{
	const SubGhzProtocolDef *proto = subghz_protocol_get(9999);
	TEST_ASSERT_NULL(proto);
}

/* ===================================================================
 * subghz_protocol_get_name — name-by-index access
 * =================================================================== */

void test_get_name_valid(void)
{
	const char *name = subghz_protocol_get_name(0);
	TEST_ASSERT_NOT_NULL(name);
	TEST_ASSERT_TRUE(strlen(name) > 0);
}

void test_get_name_out_of_range(void)
{
	TEST_ASSERT_NULL(subghz_protocol_get_name(9999));
}

/* ===================================================================
 * Registry consistency — every protocol must have a name and valid type
 * =================================================================== */

void test_registry_consistency(void)
{
	TEST_ASSERT_GREATER_THAN_UINT16(0, subghz_protocol_registry_count);

	for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
	{
		const SubGhzProtocolDef *proto = subghz_protocol_get(i);
		TEST_ASSERT_NOT_NULL_MESSAGE(proto, "Protocol entry is NULL");
		TEST_ASSERT_NOT_NULL_MESSAGE(proto->name, "Protocol name is NULL");
		TEST_ASSERT_TRUE_MESSAGE(strlen(proto->name) > 0, "Protocol name is empty");

		/* Type must be a valid enum value */
		TEST_ASSERT_TRUE_MESSAGE(
			proto->type == SubGhzProtocolTypeStatic ||
			proto->type == SubGhzProtocolTypeDynamic ||
			proto->type == SubGhzProtocolTypeRAW ||
			proto->type == SubGhzProtocolTypeWeather ||
			proto->type == SubGhzProtocolTypeTPMS,
			"Invalid protocol type");
	}
}

/* Verify no unintentional duplicate names in registry.
 * Known exceptions: Cham_Code has 4 bit-width variants (7/8/9/default),
 * Scher-Khan has Magicar/Logicar, all sharing display names. */
void test_registry_no_unintentional_duplicates(void)
{
	/* Count name occurrences — only flag if count > 4 (Cham_Code max) */
	for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
	{
		const char *name_i = subghz_protocol_get_name(i);
		if (!name_i) continue;

		/* Skip known intentional duplicates */
		if (strcmp(name_i, "Cham_Code") == 0) continue;
		if (strcmp(name_i, "Scher-Khan") == 0) continue;

		for (uint16_t j = i + 1; j < subghz_protocol_registry_count; j++)
		{
			const char *name_j = subghz_protocol_get_name(j);
			if (name_j && strcmp(name_i, name_j) == 0)
			{
				char msg[128];
				snprintf(msg, sizeof(msg),
					"Unexpected duplicate protocol name: '%s' at indices %u and %u",
					name_i, (unsigned)i, (unsigned)j);
				TEST_FAIL_MESSAGE(msg);
			}
		}
	}
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* Protocol lookup by name */
	RUN_TEST(test_find_princeton);
	RUN_TEST(test_find_keeloq);
	RUN_TEST(test_find_came);
	RUN_TEST(test_find_nice_flo);
	RUN_TEST(test_find_holtek);
	RUN_TEST(test_find_security_plus);
	RUN_TEST(test_find_marantec_exact);
	RUN_TEST(test_find_weather_protocol);
	RUN_TEST(test_find_not_found);
	RUN_TEST(test_find_null);
	RUN_TEST(test_find_case_sensitive);

	/* Index-based access */
	RUN_TEST(test_get_valid_index);
	RUN_TEST(test_get_out_of_range);

	/* Name-by-index access */
	RUN_TEST(test_get_name_valid);
	RUN_TEST(test_get_name_out_of_range);

	/* Registry consistency */
	RUN_TEST(test_registry_consistency);
	RUN_TEST(test_registry_no_unintentional_duplicates);

	return UNITY_END();
}
