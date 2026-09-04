/* See COPYING.txt for license details. */

/*
 * test_subghz_registry_filter.c
 *
 * Regression tests for the Sub-GHz registry filtering helpers
 * (subghz_protocol_mod_mask_for_registry / subghz_protocol_freq_mask_for_registry).
 *
 * These tests verify that the Config scene can restrict frequency and
 * modulation choices to combinations that are actually supported by the
 * active protocol scope (Proto Pirate subset, full registry, or none).
 */

#include "unity.h"
#include "subghz_protocol_registry.h"
#include "subghz_freq_presets.h"

void setUp(void) {}
void tearDown(void) {}

/* ================================================================
 * Full registry mask helpers
 * ================================================================ */

void test_full_registry_includes_am_and_fm_modulations(void)
{
    uint32_t mask = subghz_protocol_mod_mask_for_registry(
        subghz_protocol_registry, subghz_protocol_registry_count);
    TEST_ASSERT_BITS_HIGH_MESSAGE((1u << 0) | (1u << 1), mask,
        "Full registry must include AM270/AM650 (OOK)");
    TEST_ASSERT_BITS_HIGH_MESSAGE((1u << 2) | (1u << 3), mask,
        "Full registry must include FM238/FM476 (FSK)");
}

void test_full_registry_am_frequency_mask_includes_300_315_433_868(void)
{
    /* Use AM650 (index 1) for the frequency mask check. */
    uint64_t mask = subghz_protocol_freq_mask_for_registry(
        subghz_protocol_registry, subghz_protocol_registry_count, 1);
    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << SUBGHZ_FREQ_DEFAULT_IDX)) != 0,
        "Full-registry AM mask must include 433.92 MHz");

    int16_t idx_300 = subghz_freq_preset_find_hz(300000000UL);
    int16_t idx_315 = subghz_freq_preset_find_hz(315000000UL);
    int16_t idx_868 = subghz_freq_preset_find_hz(868350000UL);
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx_300, "300 MHz preset missing");
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx_315, "315 MHz preset missing");
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx_868, "868.35 MHz preset missing");

    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << (uint8_t)idx_300)) != 0,
        "Full-registry AM mask must include 300 MHz");
    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << (uint8_t)idx_315)) != 0,
        "Full-registry AM mask must include 315 MHz");
    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << (uint8_t)idx_868)) != 0,
        "Full-registry AM mask must include 868.35 MHz");

    /* Custom is always allowed when modulation is supported. */
    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << SUBGHZ_FREQ_PRESET_CUSTOM)) != 0,
        "Frequency mask must include Custom");
}

void test_full_registry_fm_frequency_mask_is_433_only(void)
{
    /* FM (FSK) protocols operate at 433 MHz (POCSAG/PCSG). */
    uint64_t mask = subghz_protocol_freq_mask_for_registry(
        subghz_protocol_registry, subghz_protocol_registry_count, 2);
    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << SUBGHZ_FREQ_DEFAULT_IDX)) != 0,
        "Full-registry FM mask must include 433.92 MHz");

    int16_t idx_315 = subghz_freq_preset_find_hz(315000000UL);
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx_315, "315 MHz preset missing");
    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << (uint8_t)idx_315)) == 0,
        "Full-registry FM mask must NOT include 315 MHz");
}

void test_frequency_mask_changes_with_modulation(void)
{
    uint64_t am_mask = subghz_protocol_freq_mask_for_registry(
        subghz_protocol_registry, subghz_protocol_registry_count, 0);
    uint64_t fm_mask = subghz_protocol_freq_mask_for_registry(
        subghz_protocol_registry, subghz_protocol_registry_count, 3);

    int16_t idx_315 = subghz_freq_preset_find_hz(315000000UL);
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx_315, "315 MHz preset missing");

    TEST_ASSERT_TRUE_MESSAGE((am_mask & (UINT64_C(1) << (uint8_t)idx_315)) != 0,
        "AM mask must include 315 MHz");
    TEST_ASSERT_TRUE_MESSAGE((fm_mask & (UINT64_C(1) << (uint8_t)idx_315)) == 0,
        "FM mask must NOT include 315 MHz");
}

void test_full_registry_am_mask_preserves_all_presets_in_active_bands(void)
{
    /* Every real preset within an active band's section must remain
     * selectable, not just the one nearest the nominal centre frequency.
     * Magellan (319.5 MHz, 300-350 MHz section) and Somfy Telis
     * (433.42 MHz, 387-468 MHz section) must both stay reachable. */
    uint64_t mask = subghz_protocol_freq_mask_for_registry(
        subghz_protocol_registry, subghz_protocol_registry_count, 1);

    int16_t idx_magellan = subghz_freq_preset_find_hz(319500000UL);
    int16_t idx_somfy = subghz_freq_preset_find_hz(433420000UL);
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx_magellan, "319.5 MHz preset missing");
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, idx_somfy, "433.42 MHz preset missing");

    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << (uint8_t)idx_magellan)) != 0,
        "Full-registry AM mask must include Magellan's 319.5 MHz preset");
    TEST_ASSERT_TRUE_MESSAGE((mask & (UINT64_C(1) << (uint8_t)idx_somfy)) != 0,
        "Full-registry AM mask must include Somfy Telis' 433.42 MHz preset");
}

/* ================================================================
 * Empty / invalid registry edge cases
 * ================================================================ */

void test_empty_registry_returns_zero_masks(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, subghz_protocol_mod_mask_for_registry(NULL, 0));
    TEST_ASSERT_EQUAL_UINT64(0, subghz_protocol_freq_mask_for_registry(NULL, 0, 0));
}

void test_invalid_modulation_index_returns_zero_freq_mask(void)
{
    uint64_t mask = subghz_protocol_freq_mask_for_registry(
        subghz_protocol_registry, subghz_protocol_registry_count, 255);
    TEST_ASSERT_EQUAL_UINT64(0, mask);
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_full_registry_includes_am_and_fm_modulations);
    RUN_TEST(test_full_registry_am_frequency_mask_includes_300_315_433_868);
    RUN_TEST(test_full_registry_fm_frequency_mask_is_433_only);
    RUN_TEST(test_frequency_mask_changes_with_modulation);
    RUN_TEST(test_full_registry_am_mask_preserves_all_presets_in_active_bands);
    RUN_TEST(test_empty_registry_returns_zero_masks);
    RUN_TEST(test_invalid_modulation_index_returns_zero_freq_mask);

    return UNITY_END();
}
