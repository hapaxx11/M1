/* See COPYING.txt for license details. */

/*
 * test_flipper_nfc.c
 *
 * Unit tests for flipper_nfc.c — NFC device type mapping.
 *
 * Tests the pure-logic function flipper_nfc_parse_type() which maps
 * Flipper Zero .nfc device type strings to flipper_nfc_type_t enum values.
 * This validates that all supported NFC types (ISO14443, NTAG, Mifare Classic,
 * DESFire, FeliCa, ISO15693, SLIX, ST25TB) are correctly recognized.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "flipper_nfc.h"

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * ISO 14443 base types
 * =================================================================== */

void test_nfc_iso14443_3a(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_3A,
		flipper_nfc_parse_type("ISO14443-3A"));
}

void test_nfc_iso14443_3b(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_3B,
		flipper_nfc_parse_type("ISO14443-3B"));
}

void test_nfc_iso14443_4a(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_4A,
		flipper_nfc_parse_type("ISO14443-4A"));
}

void test_nfc_iso14443_4b_as_3b(void)
{
	/* ISO14443-4B treated as -3B at parser level */
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_3B,
		flipper_nfc_parse_type("ISO14443-4B"));
}

/* ===================================================================
 * NTAG series — all map to NTAG handler
 * =================================================================== */

void test_nfc_ntag(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAG"));
}

void test_nfc_ntag203(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAG203"));
}

void test_nfc_ntag210(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAG210"));
}

void test_nfc_ntag212(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAG212"));
}

void test_nfc_ntag213(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAG213"));
}

void test_nfc_ntag215(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAG215"));
}

void test_nfc_ntag216(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAG216"));
}

void test_nfc_ntag216f(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAG216F"));
}

void test_nfc_ntagi2c1k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAGI2C1K"));
}

void test_nfc_ntagi2c2k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NTAGI2C2K"));
}

void test_nfc_nt3h1101(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NT3H1101"));
}

void test_nfc_nt3h1201(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NT3H1201"));
}

void test_nfc_nt3h2111(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NT3H2111"));
}

void test_nfc_nt3h2211(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("NT3H2211"));
}

/* ===================================================================
 * Mifare Ultralight (treated as NTAG-compatible T2T)
 * =================================================================== */

void test_nfc_mifare_ultralight(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("Mifare Ultralight"));
}

void test_nfc_mifare_ultralight_c(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("Mifare Ultralight C"));
}

void test_nfc_mifare_ultralight_ev1(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("Mifare Ultralight EV1"));
}

void test_nfc_mifare_ultralight_nano(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("Mifare Ultralight Nano"));
}

void test_nfc_mf0ul11(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("MF0UL11"));
}

void test_nfc_mf0ul21(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("MF0UL21"));
}

/* ===================================================================
 * Mifare Classic
 * =================================================================== */

void test_nfc_mifare_classic(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_CLASSIC,
		flipper_nfc_parse_type("Mifare Classic"));
}

void test_nfc_mifare_classic_1k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_CLASSIC,
		flipper_nfc_parse_type("Mifare Classic 1K"));
}

void test_nfc_mifare_classic_4k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_CLASSIC,
		flipper_nfc_parse_type("Mifare Classic 4K"));
}

void test_nfc_mifare_classic_mini(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_CLASSIC,
		flipper_nfc_parse_type("Mifare Classic Mini"));
}

void test_nfc_mifare_mini(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_CLASSIC,
		flipper_nfc_parse_type("Mifare Mini"));
}

void test_nfc_mf1s50(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_CLASSIC,
		flipper_nfc_parse_type("MF1S50"));
}

void test_nfc_mf1s70(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_CLASSIC,
		flipper_nfc_parse_type("MF1S70"));
}

/* ===================================================================
 * Mifare Plus (ISO 14443-4A)
 * =================================================================== */

void test_nfc_mifare_plus(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_4A,
		flipper_nfc_parse_type("Mifare Plus"));
}

void test_nfc_mifare_plus_s(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_4A,
		flipper_nfc_parse_type("Mifare Plus S"));
}

void test_nfc_mifare_plus_x(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_4A,
		flipper_nfc_parse_type("Mifare Plus X"));
}

void test_nfc_mifare_plus_ev1(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_4A,
		flipper_nfc_parse_type("Mifare Plus EV1"));
}

void test_nfc_mifare_plus_ev2(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_4A,
		flipper_nfc_parse_type("Mifare Plus EV2"));
}

/* ===================================================================
 * Mifare DESFire
 * =================================================================== */

void test_nfc_mifare_desfire(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_DESFIRE,
		flipper_nfc_parse_type("Mifare DESFire"));
}

void test_nfc_mifare_desfire_ev1(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_DESFIRE,
		flipper_nfc_parse_type("Mifare DESFire EV1"));
}

void test_nfc_mifare_desfire_ev2(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_DESFIRE,
		flipper_nfc_parse_type("Mifare DESFire EV2"));
}

void test_nfc_mifare_desfire_ev3(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_DESFIRE,
		flipper_nfc_parse_type("Mifare DESFire EV3"));
}

void test_nfc_mifare_desfire_light(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_DESFIRE,
		flipper_nfc_parse_type("Mifare DESFire Light"));
}

/* ===================================================================
 * ST25 / ISO14443-3A family
 * =================================================================== */

void test_nfc_st25(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_3A,
		flipper_nfc_parse_type("ST25"));
}

void test_nfc_st25ta(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_3A,
		flipper_nfc_parse_type("ST25TA"));
}

void test_nfc_st25ta512(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_3A,
		flipper_nfc_parse_type("ST25TA512"));
}

void test_nfc_st25ta02k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO14443_3A,
		flipper_nfc_parse_type("ST25TA02K"));
}

/* ===================================================================
 * FeliCa (ISO 18092 / JIS X 6319-4)
 * =================================================================== */

void test_nfc_felica(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_FELICA,
		flipper_nfc_parse_type("FeliCa"));
}

void test_nfc_felica_lower(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_FELICA,
		flipper_nfc_parse_type("Felica"));
}

void test_nfc_suica(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_FELICA,
		flipper_nfc_parse_type("Suica"));
}

void test_nfc_pasmo(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_FELICA,
		flipper_nfc_parse_type("PASMO"));
}

/* ===================================================================
 * ISO 15693-3 (NFC-V / vicinity cards)
 * =================================================================== */

void test_nfc_iso15693_3(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO15693,
		flipper_nfc_parse_type("ISO15693-3"));
}

void test_nfc_iso15693(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ISO15693,
		flipper_nfc_parse_type("ISO15693"));
}

/* ===================================================================
 * NXP SLIX / I-Code SLI (ISO 15693 subtype)
 * =================================================================== */

void test_nfc_slix(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_SLIX,
		flipper_nfc_parse_type("SLIX"));
}

void test_nfc_slix_s(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_SLIX,
		flipper_nfc_parse_type("SLIX-S"));
}

void test_nfc_slix_l(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_SLIX,
		flipper_nfc_parse_type("SLIX-L"));
}

void test_nfc_slix2(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_SLIX,
		flipper_nfc_parse_type("SLIX2"));
}

void test_nfc_sli(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_SLIX,
		flipper_nfc_parse_type("SLI"));
}

void test_nfc_sli_s(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_SLIX,
		flipper_nfc_parse_type("SLI-S"));
}

void test_nfc_sli_l(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_SLIX,
		flipper_nfc_parse_type("SLI-L"));
}

/* ===================================================================
 * ST ST25TB / SR series (ISO 14443-2 Type B)
 * =================================================================== */

void test_nfc_st25tb(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("ST25TB"));
}

void test_nfc_st25tb04k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("ST25TB04K"));
}

void test_nfc_st25tb512_ac(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("ST25TB512-AC"));
}

void test_nfc_st25tb512_at(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("ST25TB512-AT"));
}

void test_nfc_st25tb02k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("ST25TB02K"));
}

void test_nfc_sri512_lower(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("SRi512"));
}

void test_nfc_sri512_upper(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("SRI512"));
}

void test_nfc_sri4k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("SRI4K"));
}

void test_nfc_srix4k(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("SRIX4K"));
}

void test_nfc_srix4k_mixed(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_ST25TB,
		flipper_nfc_parse_type("SRIx4K"));
}

/* ===================================================================
 * Edge cases
 * =================================================================== */

void test_nfc_unknown(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_UNKNOWN,
		flipper_nfc_parse_type("SomeUnknownType"));
}

void test_nfc_null(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_UNKNOWN,
		flipper_nfc_parse_type(NULL));
}

void test_nfc_empty(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_UNKNOWN,
		flipper_nfc_parse_type(""));
}

void test_nfc_case_insensitive(void)
{
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_NTAG,
		flipper_nfc_parse_type("ntag215"));
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_MIFARE_CLASSIC,
		flipper_nfc_parse_type("MIFARE CLASSIC 1K"));
	TEST_ASSERT_EQUAL(FLIPPER_NFC_TYPE_FELICA,
		flipper_nfc_parse_type("felica"));
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* ISO 14443 base types */
	RUN_TEST(test_nfc_iso14443_3a);
	RUN_TEST(test_nfc_iso14443_3b);
	RUN_TEST(test_nfc_iso14443_4a);
	RUN_TEST(test_nfc_iso14443_4b_as_3b);

	/* NTAG series */
	RUN_TEST(test_nfc_ntag);
	RUN_TEST(test_nfc_ntag203);
	RUN_TEST(test_nfc_ntag210);
	RUN_TEST(test_nfc_ntag212);
	RUN_TEST(test_nfc_ntag213);
	RUN_TEST(test_nfc_ntag215);
	RUN_TEST(test_nfc_ntag216);
	RUN_TEST(test_nfc_ntag216f);
	RUN_TEST(test_nfc_ntagi2c1k);
	RUN_TEST(test_nfc_ntagi2c2k);
	RUN_TEST(test_nfc_nt3h1101);
	RUN_TEST(test_nfc_nt3h1201);
	RUN_TEST(test_nfc_nt3h2111);
	RUN_TEST(test_nfc_nt3h2211);

	/* Mifare Ultralight (NTAG-compatible) */
	RUN_TEST(test_nfc_mifare_ultralight);
	RUN_TEST(test_nfc_mifare_ultralight_c);
	RUN_TEST(test_nfc_mifare_ultralight_ev1);
	RUN_TEST(test_nfc_mifare_ultralight_nano);
	RUN_TEST(test_nfc_mf0ul11);
	RUN_TEST(test_nfc_mf0ul21);

	/* Mifare Classic */
	RUN_TEST(test_nfc_mifare_classic);
	RUN_TEST(test_nfc_mifare_classic_1k);
	RUN_TEST(test_nfc_mifare_classic_4k);
	RUN_TEST(test_nfc_mifare_classic_mini);
	RUN_TEST(test_nfc_mifare_mini);
	RUN_TEST(test_nfc_mf1s50);
	RUN_TEST(test_nfc_mf1s70);

	/* Mifare Plus */
	RUN_TEST(test_nfc_mifare_plus);
	RUN_TEST(test_nfc_mifare_plus_s);
	RUN_TEST(test_nfc_mifare_plus_x);
	RUN_TEST(test_nfc_mifare_plus_ev1);
	RUN_TEST(test_nfc_mifare_plus_ev2);

	/* Mifare DESFire */
	RUN_TEST(test_nfc_mifare_desfire);
	RUN_TEST(test_nfc_mifare_desfire_ev1);
	RUN_TEST(test_nfc_mifare_desfire_ev2);
	RUN_TEST(test_nfc_mifare_desfire_ev3);
	RUN_TEST(test_nfc_mifare_desfire_light);

	/* ST25 / ISO14443-3A */
	RUN_TEST(test_nfc_st25);
	RUN_TEST(test_nfc_st25ta);
	RUN_TEST(test_nfc_st25ta512);
	RUN_TEST(test_nfc_st25ta02k);

	/* FeliCa */
	RUN_TEST(test_nfc_felica);
	RUN_TEST(test_nfc_felica_lower);
	RUN_TEST(test_nfc_suica);
	RUN_TEST(test_nfc_pasmo);

	/* ISO 15693 */
	RUN_TEST(test_nfc_iso15693_3);
	RUN_TEST(test_nfc_iso15693);

	/* SLIX / SLI */
	RUN_TEST(test_nfc_slix);
	RUN_TEST(test_nfc_slix_s);
	RUN_TEST(test_nfc_slix_l);
	RUN_TEST(test_nfc_slix2);
	RUN_TEST(test_nfc_sli);
	RUN_TEST(test_nfc_sli_s);
	RUN_TEST(test_nfc_sli_l);

	/* ST25TB / SR */
	RUN_TEST(test_nfc_st25tb);
	RUN_TEST(test_nfc_st25tb04k);
	RUN_TEST(test_nfc_st25tb512_ac);
	RUN_TEST(test_nfc_st25tb512_at);
	RUN_TEST(test_nfc_st25tb02k);
	RUN_TEST(test_nfc_sri512_lower);
	RUN_TEST(test_nfc_sri512_upper);
	RUN_TEST(test_nfc_sri4k);
	RUN_TEST(test_nfc_srix4k);
	RUN_TEST(test_nfc_srix4k_mixed);

	/* Edge cases */
	RUN_TEST(test_nfc_unknown);
	RUN_TEST(test_nfc_null);
	RUN_TEST(test_nfc_empty);
	RUN_TEST(test_nfc_case_insensitive);

	return UNITY_END();
}
