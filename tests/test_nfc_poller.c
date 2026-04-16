/*
 * Host-side unit tests for NFC poller pure-logic helpers.
 *
 * Tests mfc_is_classic_sak() — the SAK-to-MIFARE-Classic classifier used by
 * nfc_poller_is_classic_sak() in production.
 *
 * Uses a standalone copy in tests/stubs/nfc_poller_impl.c because the
 * production nfc_poller.c has deep HAL/RTOS/RFAL dependencies.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* Standalone copy of the function under test */
extern bool mfc_is_classic_sak(uint8_t sak);

void setUp(void) { }
void tearDown(void) { }

/* --- Positive cases: all known Classic SAK values --- */

void test_classic_1k_tnp3xxx(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x01));
}

void test_classic_1k(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x08));
}

void test_mifare_mini(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x09));
}

void test_plus_2k_sl2(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x10));
}

void test_plus_4k_sl2(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x11));
}

void test_classic_4k(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x18));
}

void test_classic_2k(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x19));
}

void test_classic_ev1_1k(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x28));
}

void test_classic_ev1_4k(void)
{
    TEST_ASSERT_TRUE(mfc_is_classic_sak(0x38));
}

/* --- Negative cases: SAK values that are NOT Classic --- */

void test_ultralight_t2t(void)
{
    TEST_ASSERT_FALSE(mfc_is_classic_sak(0x00));
}

void test_type4_desfire(void)
{
    TEST_ASSERT_FALSE(mfc_is_classic_sak(0x20));
}

void test_random_value_0xff(void)
{
    TEST_ASSERT_FALSE(mfc_is_classic_sak(0xFF));
}

void test_random_value_0x04(void)
{
    TEST_ASSERT_FALSE(mfc_is_classic_sak(0x04));
}

void test_random_value_0x44(void)
{
    TEST_ASSERT_FALSE(mfc_is_classic_sak(0x44));
}

/* --- Boundary: all 256 SAK values produce correct result --- */

void test_exhaustive_sak_range(void)
{
    const uint8_t classic_saks[] = {0x01, 0x08, 0x09, 0x10, 0x11, 0x18, 0x19, 0x28, 0x38};
    const size_t  num_classic    = sizeof(classic_saks) / sizeof(classic_saks[0]);

    for (unsigned sak = 0; sak <= 0xFF; sak++) {
        bool expected = false;
        for (size_t i = 0; i < num_classic; i++) {
            if (classic_saks[i] == (uint8_t)sak) {
                expected = true;
                break;
            }
        }
        char msg[64];
        snprintf(msg, sizeof(msg), "SAK 0x%02X", sak);
        TEST_ASSERT_EQUAL_MESSAGE(expected, mfc_is_classic_sak((uint8_t)sak), msg);
    }
}

int main(void)
{
    UNITY_BEGIN();

    /* Positive cases */
    RUN_TEST(test_classic_1k_tnp3xxx);
    RUN_TEST(test_classic_1k);
    RUN_TEST(test_mifare_mini);
    RUN_TEST(test_plus_2k_sl2);
    RUN_TEST(test_plus_4k_sl2);
    RUN_TEST(test_classic_4k);
    RUN_TEST(test_classic_2k);
    RUN_TEST(test_classic_ev1_1k);
    RUN_TEST(test_classic_ev1_4k);

    /* Negative cases */
    RUN_TEST(test_ultralight_t2t);
    RUN_TEST(test_type4_desfire);
    RUN_TEST(test_random_value_0xff);
    RUN_TEST(test_random_value_0x04);
    RUN_TEST(test_random_value_0x44);

    /* Exhaustive */
    RUN_TEST(test_exhaustive_sak_range);

    return UNITY_END();
}
