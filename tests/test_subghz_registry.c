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

/* Case insensitivity: registry uses strcasecmp for Flipper .sub compatibility */
void test_find_case_insensitive(void)
{
	/* "princeton" should now match "Princeton" */
	int16_t idx = subghz_protocol_find_by_name("princeton");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL_STRING("Princeton", proto->name);
}

void test_find_case_insensitive_mixed(void)
{
	/* "FAAC SLH" / "faac slh" / "Faac SLH" should all match */
	int16_t idx1 = subghz_protocol_find_by_name("FAAC SLH");
	int16_t idx2 = subghz_protocol_find_by_name("faac slh");
	int16_t idx3 = subghz_protocol_find_by_name("Faac SLH");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx1);
	TEST_ASSERT_EQUAL_INT16(idx1, idx2);
	TEST_ASSERT_EQUAL_INT16(idx1, idx3);
}

void test_find_case_insensitive_keeloq(void)
{
	/* "keeloq" should match "KeeLoq" */
	int16_t idx = subghz_protocol_find_by_name("keeloq");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeDynamic,
		subghz_protocol_get((uint16_t)idx)->type);
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

/* ===================================================================
 * SubGhzProtocolFlag_300 — 300 MHz protocol flag regression
 *
 * Bug context (300 MHz implementation, PR #171 follow-up):
 *   SubGhzProtocolFlag_300 was missing from the flag enum entirely.
 *   LINEAR_10BIT and LINEAR_DELTA3 were tagged with F_STATIC_MULTI
 *   (which implied 315 + 433 + 868 MHz) rather than being correctly
 *   identified as 300 MHz-only protocols.  This caused the brute force
 *   to always init the radio at 433.92 MHz even when "Linear 300" was
 *   selected, producing no signal at the correct frequency.
 * =================================================================== */

void test_linear_has_flag_300(void)
{
	int16_t idx = subghz_protocol_find_by_name("Linear");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);

	/* Linear operates at 300 MHz — flag must be set */
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_300, proto->flags);
}

void test_linear_has_no_433_flag(void)
{
	int16_t idx = subghz_protocol_find_by_name("Linear");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);

	/* Linear is a 300 MHz protocol — it must NOT claim 433 MHz capability */
	TEST_ASSERT_BITS_LOW(SubGhzProtocolFlag_433, proto->flags);
}

void test_linear_delta3_has_flag_300(void)
{
	int16_t idx = subghz_protocol_find_by_name("LinearDelta3");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);

	/* LinearDelta3 operates at 300 MHz — flag must be set */
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_300, proto->flags);
}

void test_princeton_has_flag_300(void)
{
	int16_t idx = subghz_protocol_find_by_name("Princeton");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);

	/* Princeton is multi-band including 300 MHz */
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_300, proto->flags);
	/* and still includes 433 MHz */
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_433, proto->flags);
}

void test_came_has_no_flag_300(void)
{
	int16_t idx = subghz_protocol_find_by_name("CAME");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);

	/* CAME is 433/868 MHz only — no 300 MHz or 315 MHz operation */
	TEST_ASSERT_BITS_LOW(SubGhzProtocolFlag_300, proto->flags);
	TEST_ASSERT_BITS_LOW(SubGhzProtocolFlag_315, proto->flags);
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_433, proto->flags);
}

/* ===================================================================
 * Specific Momentum-phase protocol presence
 *
 * Verify that protocols added across Momentum porting phases are
 * registered and correctly categorised.
 * =================================================================== */

void test_find_magellan(void)
{
	int16_t idx = subghz_protocol_find_by_name("Magellan");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_433, proto->flags);
}

void test_find_marantec24(void)
{
	int16_t idx = subghz_protocol_find_by_name("Marantec24");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	/* Marantec24 is Static — must NOT be misidentified via substring match on "Marantec" */
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_clemsa(void)
{
	int16_t idx = subghz_protocol_find_by_name("Clemsa");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_centurion(void)
{
	int16_t idx = subghz_protocol_find_by_name("Centurion");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_bett(void)
{
	int16_t idx = subghz_protocol_find_by_name("BETT");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_legrand(void)
{
	int16_t idx = subghz_protocol_find_by_name("Legrand");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_linear_delta3(void)
{
	int16_t idx = subghz_protocol_find_by_name("LinearDelta3");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_300, proto->flags);
}

void test_find_came_twee(void)
{
	int16_t idx = subghz_protocol_find_by_name("CAME TWEE");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	/* CAME TWEE is rolling/dynamic */
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeDynamic, proto->type);
}

void test_find_nice_flor_s(void)
{
	int16_t idx = subghz_protocol_find_by_name("Nice FloR-S");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeDynamic, proto->type);
}

void test_find_elplast(void)
{
	int16_t idx = subghz_protocol_find_by_name("Elplast");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_keyfinder(void)
{
	int16_t idx = subghz_protocol_find_by_name("KeyFinder");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
}

void test_find_acurite_606tx(void)
{
	int16_t idx = subghz_protocol_find_by_name("Acurite_606TX");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeWeather, proto->type);
}

void test_find_firecracker(void)
{
	/* FireCracker (CM17A) — X10 home-automation RF, 40-bit packet */
	int16_t idx = subghz_protocol_find_by_name("FireCracker");
	TEST_ASSERT_GREATER_OR_EQUAL_INT16(0, idx);

	const SubGhzProtocolDef *proto = subghz_protocol_get((uint16_t)idx);
	TEST_ASSERT_NOT_NULL(proto);
	TEST_ASSERT_EQUAL(SubGhzProtocolTypeStatic, proto->type);
	TEST_ASSERT_EQUAL_UINT16(40, proto->timing.min_count_bit_for_found);
	/* Must be replayable (Static + AM) */
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_AM,   proto->flags);
	TEST_ASSERT_BITS_HIGH(SubGhzProtocolFlag_Send, proto->flags);
	/* Frequency: 315 and/or 433 MHz */
	uint32_t freq_flags = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433;
	TEST_ASSERT_TRUE_MESSAGE((proto->flags & freq_flags) != 0,
		"FireCracker must have at least one of 315/433 MHz flags");
}

/* ===================================================================
 * Protocol flag consistency rules
 *
 * Verify invariants that must hold across the entire registry:
 *  1. All Static AM protocols must have the Send flag (can be replayed).
 *  2. All Static AM protocols must have the Save flag.
 *  3. All Weather and TPMS protocols must NOT have the Send flag
 *     (sensor-only, cannot be replayed as commands).
 *  4. All Dynamic (rolling-code) protocols must NOT have the Send flag.
 *  5. All decodable protocols must have min_count_bit_for_found > 0.
 *  6. All AM protocols must declare at least one frequency band.
 * =================================================================== */

void test_static_protocols_have_send_flag(void)
{
	for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
	{
		const SubGhzProtocolDef *proto = subghz_protocol_get(i);
		if (!proto) continue;
		if (proto->type != SubGhzProtocolTypeStatic) continue;
		/* FM-only protocols (e.g. POCSAG, PCSG) are receive-only — skip */
		if (!(proto->flags & SubGhzProtocolFlag_AM)) continue;

		char msg[128];
		snprintf(msg, sizeof(msg),
			"Static AM protocol '%s' is missing SubGhzProtocolFlag_Send",
			proto->name ? proto->name : "<null>");
		TEST_ASSERT_BITS_HIGH_MESSAGE(SubGhzProtocolFlag_Send, proto->flags, msg);
	}
}

void test_weather_protocols_have_no_send_flag(void)
{
	for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
	{
		const SubGhzProtocolDef *proto = subghz_protocol_get(i);
		if (!proto) continue;
		if (proto->type != SubGhzProtocolTypeWeather &&
		    proto->type != SubGhzProtocolTypeTPMS) continue;

		char msg[128];
		snprintf(msg, sizeof(msg),
			"Weather/TPMS protocol '%s' incorrectly has SubGhzProtocolFlag_Send",
			proto->name ? proto->name : "<null>");
		TEST_ASSERT_BITS_LOW_MESSAGE(SubGhzProtocolFlag_Send, proto->flags, msg);
	}
}

void test_dynamic_protocols_have_no_send_flag(void)
{
	for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
	{
		const SubGhzProtocolDef *proto = subghz_protocol_get(i);
		if (!proto) continue;
		if (proto->type != SubGhzProtocolTypeDynamic) continue;

		char msg[128];
		snprintf(msg, sizeof(msg),
			"Dynamic protocol '%s' incorrectly has SubGhzProtocolFlag_Send",
			proto->name ? proto->name : "<null>");
		TEST_ASSERT_BITS_LOW_MESSAGE(SubGhzProtocolFlag_Send, proto->flags, msg);
	}
}

void test_decodable_protocols_have_nonzero_min_bits(void)
{
	for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
	{
		const SubGhzProtocolDef *proto = subghz_protocol_get(i);
		if (!proto) continue;
		if (!(proto->flags & SubGhzProtocolFlag_Decodable)) continue;
		if (!proto->decode) continue; /* no decoder → skip */

		char msg[128];
		snprintf(msg, sizeof(msg),
			"Decodable protocol '%s' has min_count_bit_for_found == 0",
			proto->name ? proto->name : "<null>");
		TEST_ASSERT_GREATER_THAN_UINT16_MESSAGE(0,
			proto->timing.min_count_bit_for_found, msg);
	}
}

void test_static_protocols_have_save_flag(void)
{
	for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
	{
		const SubGhzProtocolDef *proto = subghz_protocol_get(i);
		if (!proto) continue;
		if (proto->type != SubGhzProtocolTypeStatic) continue;
		/* FM-only protocols (e.g. POCSAG, PCSG) are receive-only — skip */
		if (!(proto->flags & SubGhzProtocolFlag_AM)) continue;

		char msg[128];
		snprintf(msg, sizeof(msg),
			"Static AM protocol '%s' is missing Save flag",
			proto->name ? proto->name : "<null>");
		TEST_ASSERT_BITS_HIGH_MESSAGE(SubGhzProtocolFlag_Save, proto->flags, msg);
	}
}

void test_am_protocols_have_frequency_flags(void)
{
	/* All AM protocols must specify at least one frequency band */
	const uint32_t any_freq = SubGhzProtocolFlag_300 | SubGhzProtocolFlag_315 |
	                           SubGhzProtocolFlag_433 | SubGhzProtocolFlag_868;
	for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
	{
		const SubGhzProtocolDef *proto = subghz_protocol_get(i);
		if (!proto) continue;
		if (!(proto->flags & SubGhzProtocolFlag_AM)) continue;

		char msg[128];
		snprintf(msg, sizeof(msg),
			"AM protocol '%s' has no frequency band flag (300/315/433/868)",
			proto->name ? proto->name : "<null>");
		TEST_ASSERT_TRUE_MESSAGE((proto->flags & any_freq) != 0, msg);
	}
}

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
	RUN_TEST(test_find_case_insensitive);
	RUN_TEST(test_find_case_insensitive_mixed);
	RUN_TEST(test_find_case_insensitive_keeloq);

	/* Index-based access */
	RUN_TEST(test_get_valid_index);
	RUN_TEST(test_get_out_of_range);

	/* Name-by-index access */
	RUN_TEST(test_get_name_valid);
	RUN_TEST(test_get_name_out_of_range);

	/* Registry consistency */
	RUN_TEST(test_registry_consistency);
	RUN_TEST(test_registry_no_unintentional_duplicates);

	/* SubGhzProtocolFlag_300 — 300 MHz flag regression */
	RUN_TEST(test_linear_has_flag_300);
	RUN_TEST(test_linear_has_no_433_flag);
	RUN_TEST(test_linear_delta3_has_flag_300);
	RUN_TEST(test_princeton_has_flag_300);
	RUN_TEST(test_came_has_no_flag_300);

	/* Momentum-phase protocol presence */
	RUN_TEST(test_find_magellan);
	RUN_TEST(test_find_marantec24);
	RUN_TEST(test_find_clemsa);
	RUN_TEST(test_find_centurion);
	RUN_TEST(test_find_bett);
	RUN_TEST(test_find_legrand);
	RUN_TEST(test_find_linear_delta3);
	RUN_TEST(test_find_came_twee);
	RUN_TEST(test_find_nice_flor_s);
	RUN_TEST(test_find_elplast);
	RUN_TEST(test_find_keyfinder);
	RUN_TEST(test_find_acurite_606tx);
	RUN_TEST(test_find_firecracker);

	/* Protocol flag consistency rules */
	RUN_TEST(test_static_protocols_have_send_flag);
	RUN_TEST(test_weather_protocols_have_no_send_flag);
	RUN_TEST(test_dynamic_protocols_have_no_send_flag);
	RUN_TEST(test_decodable_protocols_have_nonzero_min_bits);
	RUN_TEST(test_static_protocols_have_save_flag);
	RUN_TEST(test_am_protocols_have_frequency_flags);

	return UNITY_END();
}
