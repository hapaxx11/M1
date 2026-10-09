/* See COPYING.txt for license details. */

/*
 * test_lfrfid_carrier.c
 *
 * Unit tests for lfrfid_carrier.c — the LF-RFID carrier-cycling state machine.
 *
 * Regression context: the read sweep previously only energised 125 kHz and
 * 134.2 kHz; it now also energises 128 kHz.  These tests pin the sweep order,
 * the per-carrier PWM params and the cycle-time constant used to size the read
 * timeout.  They do not exercise tag decoding.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "lfrfid_carrier.h"

void setUp(void) {}
void tearDown(void) {}

void test_carrier_cycle_ms_covers_all_dwells(void)
{
	/* Four carriers (ASK 125/128/134.2 + PSK) each get a full dwell. */
	TEST_ASSERT_EQUAL_INT(4, LFRFID_CARRIER_COUNT);
	TEST_ASSERT_EQUAL_INT(4 * LFRFID_CARRIER_SWITCH_MS, LFRFID_CARRIER_CYCLE_MS);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(8000, LFRFID_CARRIER_CYCLE_MS);
}

/* ===================================================================
 * lfrfid_carrier_next — sweep order
 * =================================================================== */

void test_carrier_cycle_full_order(void)
{
	/* ASK 125k -> ASK 128k -> ASK 134.2k -> PSK -> ASK 125k ... */
	TEST_ASSERT_EQUAL_INT(LFRFID_CARRIER_ASK_128,
	                      lfrfid_carrier_next(LFRFID_CARRIER_ASK));
	TEST_ASSERT_EQUAL_INT(LFRFID_CARRIER_ASK_134,
	                      lfrfid_carrier_next(LFRFID_CARRIER_ASK_128));
	TEST_ASSERT_EQUAL_INT(LFRFID_CARRIER_PSK,
	                      lfrfid_carrier_next(LFRFID_CARRIER_ASK_134));
	TEST_ASSERT_EQUAL_INT(LFRFID_CARRIER_ASK,
	                      lfrfid_carrier_next(LFRFID_CARRIER_PSK));
}

void test_carrier_cycle_sweeps_all_three_pet_frequencies(void)
{
	/* Starting from the read-start carrier (ASK 125k), one full cycle must
	 * visit all three US pet-chip frequencies before repeating. */
	bool seen_125 = false, seen_128 = false, seen_134 = false;
	lfrfid_carrier_t c = LFRFID_CARRIER_ASK;

	for (int i = 0; i < 4; i++) {
		uint32_t freq = 0;
		lfrfid_carrier_params(c, &freq, NULL);
		if (freq == LFRFID_CARRIER_ASK_FREQ)     seen_125 = true;
		if (freq == LFRFID_CARRIER_ASK_128_FREQ) seen_128 = true;
		if (freq == LFRFID_CARRIER_ASK_134_FREQ) seen_134 = true;
		c = lfrfid_carrier_next(c);
	}

	TEST_ASSERT_TRUE_MESSAGE(seen_125, "125 kHz not swept");
	TEST_ASSERT_TRUE_MESSAGE(seen_128, "128 kHz not swept");
	TEST_ASSERT_TRUE_MESSAGE(seen_134, "134.2 kHz not swept");
}

void test_carrier_cycle_returns_to_start_after_four_steps(void)
{
	lfrfid_carrier_t c = LFRFID_CARRIER_ASK;
	for (int i = 0; i < 4; i++)
		c = lfrfid_carrier_next(c);
	TEST_ASSERT_EQUAL_INT(LFRFID_CARRIER_ASK, c);
}

void test_carrier_next_unknown_wraps_to_ask(void)
{
	TEST_ASSERT_EQUAL_INT(LFRFID_CARRIER_ASK,
	                      lfrfid_carrier_next((lfrfid_carrier_t)999));
}

/* ===================================================================
 * lfrfid_carrier_params — per-carrier PWM parameters
 * =================================================================== */

void test_carrier_params_ask_125(void)
{
	uint32_t f = 0; float d = 0.0f;
	lfrfid_carrier_params(LFRFID_CARRIER_ASK, &f, &d);
	TEST_ASSERT_EQUAL_UINT32(125000, f);
	TEST_ASSERT_EQUAL_FLOAT(0.5f, d);
}

void test_carrier_params_ask_128(void)
{
	uint32_t f = 0; float d = 0.0f;
	lfrfid_carrier_params(LFRFID_CARRIER_ASK_128, &f, &d);
	TEST_ASSERT_EQUAL_UINT32(128000, f);
	TEST_ASSERT_EQUAL_FLOAT(0.5f, d);
}

void test_carrier_params_ask_134(void)
{
	uint32_t f = 0; float d = 0.0f;
	lfrfid_carrier_params(LFRFID_CARRIER_ASK_134, &f, &d);
	TEST_ASSERT_EQUAL_UINT32(134200, f);
	TEST_ASSERT_EQUAL_FLOAT(0.5f, d);
}

void test_carrier_params_psk(void)
{
	uint32_t f = 0; float d = 0.0f;
	lfrfid_carrier_params(LFRFID_CARRIER_PSK, &f, &d);
	TEST_ASSERT_EQUAL_UINT32(62500, f);
	TEST_ASSERT_EQUAL_FLOAT(0.25f, d);
}

void test_carrier_params_null_pointers_are_safe(void)
{
	/* Must not crash when either output pointer is NULL. */
	lfrfid_carrier_params(LFRFID_CARRIER_ASK_128, NULL, NULL);
	TEST_PASS();
}

/* ===================================================================
 * lfrfid_carrier_is_psk — decoder feature selection
 * =================================================================== */

void test_is_psk_true_only_for_psk(void)
{
	TEST_ASSERT_TRUE(lfrfid_carrier_is_psk(LFRFID_CARRIER_PSK));
	TEST_ASSERT_FALSE(lfrfid_carrier_is_psk(LFRFID_CARRIER_ASK));
	TEST_ASSERT_FALSE(lfrfid_carrier_is_psk(LFRFID_CARRIER_ASK_128));
	TEST_ASSERT_FALSE(lfrfid_carrier_is_psk(LFRFID_CARRIER_ASK_134));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_carrier_cycle_full_order);
	RUN_TEST(test_carrier_cycle_ms_covers_all_dwells);
	RUN_TEST(test_carrier_cycle_sweeps_all_three_pet_frequencies);
	RUN_TEST(test_carrier_cycle_returns_to_start_after_four_steps);
	RUN_TEST(test_carrier_next_unknown_wraps_to_ask);
	RUN_TEST(test_carrier_params_ask_125);
	RUN_TEST(test_carrier_params_ask_128);
	RUN_TEST(test_carrier_params_ask_134);
	RUN_TEST(test_carrier_params_psk);
	RUN_TEST(test_carrier_params_null_pointers_are_safe);
	RUN_TEST(test_is_psk_true_only_for_psk);
	return UNITY_END();
}
