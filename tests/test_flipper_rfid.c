/* See COPYING.txt for license details. */

/*
 * test_flipper_rfid.c
 *
 * Unit tests for flipper_rfid.c — RFID protocol name mapping.
 *
 * Tests the pure-logic function flipper_rfid_parse_protocol() which maps
 * Flipper Zero .rfid protocol name strings to M1 LFRFIDProtocol enum values.
 * This validates that all supported protocol names (including aliases and
 * case variants) are correctly recognized during .rfid file import.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "flipper_rfid.h"

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * EM4100 family — the most common LF RFID protocol
 * =================================================================== */

void test_rfid_em4100(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100,
		flipper_rfid_parse_protocol("EM4100"));
}

void test_rfid_em_marin_alias(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100,
		flipper_rfid_parse_protocol("EM-Marin"));
}

void test_rfid_em4100_case_insensitive(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100,
		flipper_rfid_parse_protocol("em4100"));
}

void test_rfid_em4100_upper(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100,
		flipper_rfid_parse_protocol("EM4100"));
}

void test_rfid_em4100_32(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100_32,
		flipper_rfid_parse_protocol("EM4100_32"));
}

void test_rfid_em4100_32_dash(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100_32,
		flipper_rfid_parse_protocol("EM4100-32"));
}

void test_rfid_em410032_nosep(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100_32,
		flipper_rfid_parse_protocol("EM410032"));
}

void test_rfid_em4100_16(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100_16,
		flipper_rfid_parse_protocol("EM4100_16"));
}

void test_rfid_em4100_16_dash(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100_16,
		flipper_rfid_parse_protocol("EM4100-16"));
}

void test_rfid_em410016_nosep(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolEM4100_16,
		flipper_rfid_parse_protocol("EM410016"));
}

/* ===================================================================
 * HID Wiegand family
 * =================================================================== */

void test_rfid_h10301(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolH10301,
		flipper_rfid_parse_protocol("H10301"));
}

void test_rfid_hid26_alias(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolH10301,
		flipper_rfid_parse_protocol("HID26"));
}

void test_rfid_hidprox(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolHIDGeneric,
		flipper_rfid_parse_protocol("HIDProx"));
}

void test_rfid_hid_generic(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolHIDGeneric,
		flipper_rfid_parse_protocol("HID Generic"));
}

void test_rfid_hidext(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolHIDExGeneric,
		flipper_rfid_parse_protocol("HIDExt"));
}

void test_rfid_hid_ex_generic(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolHIDExGeneric,
		flipper_rfid_parse_protocol("HID Ex Generic"));
}

/* ===================================================================
 * PSK protocols
 * =================================================================== */

void test_rfid_indala26(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolIndala26,
		flipper_rfid_parse_protocol("Indala26"));
}

void test_rfid_keri(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolKeri,
		flipper_rfid_parse_protocol("Keri"));
}

void test_rfid_nexwatch(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolNexwatch,
		flipper_rfid_parse_protocol("Nexwatch"));
}

void test_rfid_quadrakey_alias(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolNexwatch,
		flipper_rfid_parse_protocol("Quadrakey"));
}

void test_rfid_nexkey_alias(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolNexwatch,
		flipper_rfid_parse_protocol("Nexkey"));
}

void test_rfid_idteck(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolIdteck,
		flipper_rfid_parse_protocol("Idteck"));
}

/* ===================================================================
 * FSK protocols
 * =================================================================== */

void test_rfid_awid(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolAWID,
		flipper_rfid_parse_protocol("AWID"));
}

void test_rfid_pyramid(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolPyramid,
		flipper_rfid_parse_protocol("Pyramid"));
}

void test_rfid_paradox(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolParadox,
		flipper_rfid_parse_protocol("Paradox"));
}

void test_rfid_ioproxxsf(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolIoProxXSF,
		flipper_rfid_parse_protocol("IoProxXSF"));
}

void test_rfid_fdx_a(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolFDX_A,
		flipper_rfid_parse_protocol("FDX-A"));
}

/* ===================================================================
 * Manchester / bi-phase protocols
 * =================================================================== */

void test_rfid_viking(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolViking,
		flipper_rfid_parse_protocol("Viking"));
}

void test_rfid_fdx_b(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolFDX_B,
		flipper_rfid_parse_protocol("FDX-B"));
}

void test_rfid_electra(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolElectra,
		flipper_rfid_parse_protocol("Electra"));
}

void test_rfid_gallagher(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolGallagher,
		flipper_rfid_parse_protocol("Gallagher"));
}

void test_rfid_jablotron(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolJablotron,
		flipper_rfid_parse_protocol("Jablotron"));
}

void test_rfid_pac_stanley(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolPACStanley,
		flipper_rfid_parse_protocol("PAC/Stanley"));
}

void test_rfid_radio_key(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolSecurakey,
		flipper_rfid_parse_protocol("Radio Key"));
}

void test_rfid_securakey_alias(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolSecurakey,
		flipper_rfid_parse_protocol("Securakey"));
}

void test_rfid_gproxii(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolGProxII,
		flipper_rfid_parse_protocol("GProxII"));
}

void test_rfid_noralsy(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolNoralsy,
		flipper_rfid_parse_protocol("Noralsy"));
}

/* ===================================================================
 * Edge cases
 * =================================================================== */

void test_rfid_unknown_protocol(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolMax,
		flipper_rfid_parse_protocol("SomeUnknownProtocol"));
}

void test_rfid_null_input(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolMax,
		flipper_rfid_parse_protocol(NULL));
}

void test_rfid_empty_string(void)
{
	TEST_ASSERT_EQUAL(LFRFIDProtocolMax,
		flipper_rfid_parse_protocol(""));
}

void test_rfid_case_mixed(void)
{
	/* All entries should be matched case-insensitively */
	TEST_ASSERT_EQUAL(LFRFIDProtocolAWID,
		flipper_rfid_parse_protocol("awid"));
	TEST_ASSERT_EQUAL(LFRFIDProtocolViking,
		flipper_rfid_parse_protocol("VIKING"));
	TEST_ASSERT_EQUAL(LFRFIDProtocolKeri,
		flipper_rfid_parse_protocol("kErI"));
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* EM4100 family */
	RUN_TEST(test_rfid_em4100);
	RUN_TEST(test_rfid_em_marin_alias);
	RUN_TEST(test_rfid_em4100_case_insensitive);
	RUN_TEST(test_rfid_em4100_upper);
	RUN_TEST(test_rfid_em4100_32);
	RUN_TEST(test_rfid_em4100_32_dash);
	RUN_TEST(test_rfid_em410032_nosep);
	RUN_TEST(test_rfid_em4100_16);
	RUN_TEST(test_rfid_em4100_16_dash);
	RUN_TEST(test_rfid_em410016_nosep);

	/* HID family */
	RUN_TEST(test_rfid_h10301);
	RUN_TEST(test_rfid_hid26_alias);
	RUN_TEST(test_rfid_hidprox);
	RUN_TEST(test_rfid_hid_generic);
	RUN_TEST(test_rfid_hidext);
	RUN_TEST(test_rfid_hid_ex_generic);

	/* PSK protocols */
	RUN_TEST(test_rfid_indala26);
	RUN_TEST(test_rfid_keri);
	RUN_TEST(test_rfid_nexwatch);
	RUN_TEST(test_rfid_quadrakey_alias);
	RUN_TEST(test_rfid_nexkey_alias);
	RUN_TEST(test_rfid_idteck);

	/* FSK protocols */
	RUN_TEST(test_rfid_awid);
	RUN_TEST(test_rfid_pyramid);
	RUN_TEST(test_rfid_paradox);
	RUN_TEST(test_rfid_ioproxxsf);
	RUN_TEST(test_rfid_fdx_a);

	/* Manchester protocols */
	RUN_TEST(test_rfid_viking);
	RUN_TEST(test_rfid_fdx_b);
	RUN_TEST(test_rfid_electra);
	RUN_TEST(test_rfid_gallagher);
	RUN_TEST(test_rfid_jablotron);
	RUN_TEST(test_rfid_pac_stanley);
	RUN_TEST(test_rfid_radio_key);
	RUN_TEST(test_rfid_securakey_alias);
	RUN_TEST(test_rfid_gproxii);
	RUN_TEST(test_rfid_noralsy);

	/* Edge cases */
	RUN_TEST(test_rfid_unknown_protocol);
	RUN_TEST(test_rfid_null_input);
	RUN_TEST(test_rfid_empty_string);
	RUN_TEST(test_rfid_case_mixed);

	return UNITY_END();
}
