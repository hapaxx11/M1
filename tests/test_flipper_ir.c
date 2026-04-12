/* See COPYING.txt for license details. */

/*
 * test_flipper_ir.c
 *
 * Unit tests for flipper_ir.c — IR protocol mapping and hex conversion.
 *
 * Tests the pure-logic functions:
 *   - flipper_ir_proto_to_irmp()  — Flipper name → IRMP protocol ID
 *   - flipper_ir_irmp_to_proto()  — IRMP ID → Flipper name (reverse)
 *
 * These validate that all supported Flipper IR protocol names are correctly
 * recognized during .ir file import, and that round-trip conversion works.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "flipper_ir.h"

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * flipper_ir_proto_to_irmp — forward mapping (name → ID)
 * =================================================================== */

void test_ir_nec(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL,
		flipper_ir_proto_to_irmp("NEC"));
}

void test_ir_necext(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL,
		flipper_ir_proto_to_irmp("NECext"));
}

void test_ir_nec42(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC42_PROTOCOL,
		flipper_ir_proto_to_irmp("NEC42"));
}

void test_ir_nec42ext(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC42_PROTOCOL,
		flipper_ir_proto_to_irmp("NEC42ext"));
}

void test_ir_nec16(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC16_PROTOCOL,
		flipper_ir_proto_to_irmp("NEC16"));
}

void test_ir_samsung32(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL,
		flipper_ir_proto_to_irmp("Samsung32"));
}

void test_ir_rc5(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL,
		flipper_ir_proto_to_irmp("RC5"));
}

void test_ir_rc5x(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL,
		flipper_ir_proto_to_irmp("RC5X"));
}

void test_ir_rc6(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC6_PROTOCOL,
		flipper_ir_proto_to_irmp("RC6"));
}

void test_ir_sirc(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL,
		flipper_ir_proto_to_irmp("SIRC"));
}

void test_ir_sirc15(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL,
		flipper_ir_proto_to_irmp("SIRC15"));
}

void test_ir_sirc20(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL,
		flipper_ir_proto_to_irmp("SIRC20"));
}

void test_ir_kaseikyo(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_KASEIKYO_PROTOCOL,
		flipper_ir_proto_to_irmp("Kaseikyo"));
}

void test_ir_rca(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RCCAR_PROTOCOL,
		flipper_ir_proto_to_irmp("RCA"));
}

void test_ir_pioneer(void)
{
	/* Pioneer uses NEC encoding */
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL,
		flipper_ir_proto_to_irmp("Pioneer"));
}

void test_ir_denon(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL,
		flipper_ir_proto_to_irmp("Denon"));
}

void test_ir_jvc(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_JVC_PROTOCOL,
		flipper_ir_proto_to_irmp("JVC"));
}

void test_ir_sharp(void)
{
	/* Sharp uses same as Denon */
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL,
		flipper_ir_proto_to_irmp("Sharp"));
}

void test_ir_panasonic(void)
{
	/* Panasonic uses Kaseikyo */
	TEST_ASSERT_EQUAL_UINT8(IRMP_KASEIKYO_PROTOCOL,
		flipper_ir_proto_to_irmp("Panasonic"));
}

void test_ir_lg(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_LGAIR_PROTOCOL,
		flipper_ir_proto_to_irmp("LG"));
}

void test_ir_samsung(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL,
		flipper_ir_proto_to_irmp("Samsung"));
}

void test_ir_apple(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_APPLE_PROTOCOL,
		flipper_ir_proto_to_irmp("Apple"));
}

void test_ir_nokia(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NOKIA_PROTOCOL,
		flipper_ir_proto_to_irmp("Nokia"));
}

void test_ir_bose(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_BOSE_PROTOCOL,
		flipper_ir_proto_to_irmp("Bose"));
}

void test_ir_samsung48(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG48_PROTOCOL,
		flipper_ir_proto_to_irmp("Samsung48"));
}

void test_ir_rcmm(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RCMM32_PROTOCOL,
		flipper_ir_proto_to_irmp("RCMM"));
}

/* ===================================================================
 * Case insensitivity
 * =================================================================== */

void test_ir_case_lower(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL,
		flipper_ir_proto_to_irmp("nec"));
}

void test_ir_case_upper(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL,
		flipper_ir_proto_to_irmp("SAMSUNG32"));
}

void test_ir_case_mixed(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL,
		flipper_ir_proto_to_irmp("dEnOn"));
}

/* ===================================================================
 * Edge cases
 * =================================================================== */

void test_ir_unknown(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_UNKNOWN_PROTOCOL,
		flipper_ir_proto_to_irmp("SomeUnknownProtocol"));
}

void test_ir_null(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_UNKNOWN_PROTOCOL,
		flipper_ir_proto_to_irmp(NULL));
}

void test_ir_empty(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_UNKNOWN_PROTOCOL,
		flipper_ir_proto_to_irmp(""));
}

/* ===================================================================
 * flipper_ir_irmp_to_proto — reverse mapping (ID → name)
 * =================================================================== */

void test_ir_reverse_nec(void)
{
	TEST_ASSERT_EQUAL_STRING("NEC",
		flipper_ir_irmp_to_proto(IRMP_NEC_PROTOCOL));
}

void test_ir_reverse_samsung32(void)
{
	TEST_ASSERT_EQUAL_STRING("Samsung32",
		flipper_ir_irmp_to_proto(IRMP_SAMSUNG32_PROTOCOL));
}

void test_ir_reverse_rc5(void)
{
	TEST_ASSERT_EQUAL_STRING("RC5",
		flipper_ir_irmp_to_proto(IRMP_RC5_PROTOCOL));
}

void test_ir_reverse_sirc(void)
{
	TEST_ASSERT_EQUAL_STRING("SIRC",
		flipper_ir_irmp_to_proto(IRMP_SIRCS_PROTOCOL));
}

void test_ir_reverse_unknown(void)
{
	TEST_ASSERT_EQUAL_STRING("Unknown",
		flipper_ir_irmp_to_proto(255));
}

void test_ir_reverse_lg(void)
{
	TEST_ASSERT_EQUAL_STRING("LG",
		flipper_ir_irmp_to_proto(IRMP_LGAIR_PROTOCOL));
}

void test_ir_reverse_bose(void)
{
	TEST_ASSERT_EQUAL_STRING("Bose",
		flipper_ir_irmp_to_proto(IRMP_BOSE_PROTOCOL));
}

/* ===================================================================
 * Round-trip: name → ID → name (first match wins for aliases)
 * =================================================================== */

void test_ir_roundtrip_rc6(void)
{
	uint8_t id = flipper_ir_proto_to_irmp("RC6");
	const char *name = flipper_ir_irmp_to_proto(id);
	TEST_ASSERT_EQUAL_STRING("RC6", name);
}

void test_ir_roundtrip_jvc(void)
{
	uint8_t id = flipper_ir_proto_to_irmp("JVC");
	const char *name = flipper_ir_irmp_to_proto(id);
	TEST_ASSERT_EQUAL_STRING("JVC", name);
}

void test_ir_roundtrip_apple(void)
{
	uint8_t id = flipper_ir_proto_to_irmp("Apple");
	const char *name = flipper_ir_irmp_to_proto(id);
	TEST_ASSERT_EQUAL_STRING("Apple", name);
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* Forward mapping — all protocols */
	RUN_TEST(test_ir_nec);
	RUN_TEST(test_ir_necext);
	RUN_TEST(test_ir_nec42);
	RUN_TEST(test_ir_nec42ext);
	RUN_TEST(test_ir_nec16);
	RUN_TEST(test_ir_samsung32);
	RUN_TEST(test_ir_rc5);
	RUN_TEST(test_ir_rc5x);
	RUN_TEST(test_ir_rc6);
	RUN_TEST(test_ir_sirc);
	RUN_TEST(test_ir_sirc15);
	RUN_TEST(test_ir_sirc20);
	RUN_TEST(test_ir_kaseikyo);
	RUN_TEST(test_ir_rca);
	RUN_TEST(test_ir_pioneer);
	RUN_TEST(test_ir_denon);
	RUN_TEST(test_ir_jvc);
	RUN_TEST(test_ir_sharp);
	RUN_TEST(test_ir_panasonic);
	RUN_TEST(test_ir_lg);
	RUN_TEST(test_ir_samsung);
	RUN_TEST(test_ir_apple);
	RUN_TEST(test_ir_nokia);
	RUN_TEST(test_ir_bose);
	RUN_TEST(test_ir_samsung48);
	RUN_TEST(test_ir_rcmm);

	/* Case insensitivity */
	RUN_TEST(test_ir_case_lower);
	RUN_TEST(test_ir_case_upper);
	RUN_TEST(test_ir_case_mixed);

	/* Edge cases */
	RUN_TEST(test_ir_unknown);
	RUN_TEST(test_ir_null);
	RUN_TEST(test_ir_empty);

	/* Reverse mapping */
	RUN_TEST(test_ir_reverse_nec);
	RUN_TEST(test_ir_reverse_samsung32);
	RUN_TEST(test_ir_reverse_rc5);
	RUN_TEST(test_ir_reverse_sirc);
	RUN_TEST(test_ir_reverse_unknown);
	RUN_TEST(test_ir_reverse_lg);
	RUN_TEST(test_ir_reverse_bose);

	/* Round-trip */
	RUN_TEST(test_ir_roundtrip_rc6);
	RUN_TEST(test_ir_roundtrip_jvc);
	RUN_TEST(test_ir_roundtrip_apple);

	return UNITY_END();
}
