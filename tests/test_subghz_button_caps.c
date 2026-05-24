/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_button_caps.c
 * @brief  Host-side unit tests for the button-cycling capability lookup.
 *
 * Phase 4a — pins the protocol coverage list against accidental edits
 * and exercises the name-canonicalization rules (case, whitespace).
 */

#include "unity.h"
#include "subghz_button_caps.h"

#include <string.h>

void setUp(void)    { }
void tearDown(void) { }

/*============================================================================*/
/* NULL / empty / unknown                                                     */
/*============================================================================*/

static void test_null_returns_unsupported(void)
{
    subghz_button_caps_t c = subghz_button_caps_for_protocol(NULL);
    TEST_ASSERT_FALSE(c.supports_cycling);
    TEST_ASSERT_EQUAL_UINT8(0, c.button_count);
}

static void test_empty_returns_unsupported(void)
{
    subghz_button_caps_t c = subghz_button_caps_for_protocol("");
    TEST_ASSERT_FALSE(c.supports_cycling);
    TEST_ASSERT_EQUAL_UINT8(0, c.button_count);
}

static void test_unknown_returns_unsupported(void)
{
    subghz_button_caps_t c =
        subghz_button_caps_for_protocol("Definitely Not A Real Protocol");
    TEST_ASSERT_FALSE(c.supports_cycling);
    TEST_ASSERT_EQUAL_UINT8(0, c.button_count);
}

/*============================================================================*/
/* Static OOK / non-replayable / AES — must NOT support cycling               */
/*============================================================================*/

static void test_static_protocols_unsupported(void)
{
    /* These are deliberately excluded — they are not rolling-code or
     * cannot be replayed OTA on M1. */
    const char *names[] = {
        "Princeton",
        "CAME",          /* static CAME (not Atomo/TWEE) */
        "Nice FLO",
        "Holtek",
        "Linear",
        "Linear Delta-3",
        "Hormann HSM",
        "FAAC SLH",      /* Manchester, no replay */
        "Somfy Telis",
        "Security+ 2.0", /* FSK bidirectional */
        "KIA Seed",
    };
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        subghz_button_caps_t c =
            subghz_button_caps_for_protocol(names[i]);
        TEST_ASSERT_FALSE_MESSAGE(c.supports_cycling, names[i]);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, c.button_count, names[i]);
    }
}

/*============================================================================*/
/* Supported protocols — exact registry name                                  */
/*============================================================================*/

static void check_supported(const char *name, uint8_t expected_count)
{
    subghz_button_caps_t c = subghz_button_caps_for_protocol(name);
    TEST_ASSERT_TRUE_MESSAGE(c.supports_cycling, name);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected_count, c.button_count, name);
}

static void test_keeloq_family(void)
{
    check_supported("KeeLoq",    4);
    check_supported("Jarolift",  4);
    check_supported("Star Line", 4);
}

static void test_nice_flor_s_has_sixteen_buttons(void)
{
    /* Nice FloR-S has a 4-bit button field (16 codes). */
    check_supported("Nice FloR-S", 16);
}

static void test_came_family(void)
{
    check_supported("CAME Atomo", 4);
    check_supported("CAME TWEE",  4);
}

static void test_alutech_and_kinggates(void)
{
    check_supported("Alutech AT-4N",     4);
    check_supported("KingGates Stylo4k", 4);
}

static void test_ditec_gol4(void)
{
    check_supported("DITEC_GOL4", 4);
}

static void test_scher_khan(void)
{
    check_supported("Scher-Khan", 4);
}

static void test_toyota(void)
{
    check_supported("Toyota", 4);
}

/*============================================================================*/
/* Name canonicalisation                                                      */
/*============================================================================*/

static void test_case_insensitive_match(void)
{
    /* Lower, upper, mixed — all canonicalise to the same entry. */
    subghz_button_caps_t a = subghz_button_caps_for_protocol("keeloq");
    subghz_button_caps_t b = subghz_button_caps_for_protocol("KEELOQ");
    subghz_button_caps_t c = subghz_button_caps_for_protocol("KeeLoq");
    subghz_button_caps_t d = subghz_button_caps_for_protocol("kEeLoQ");
    TEST_ASSERT_TRUE(a.supports_cycling);
    TEST_ASSERT_EQUAL_UINT8(4, a.button_count);
    TEST_ASSERT_EQUAL_UINT8(a.button_count, b.button_count);
    TEST_ASSERT_EQUAL_UINT8(a.button_count, c.button_count);
    TEST_ASSERT_EQUAL_UINT8(a.button_count, d.button_count);
}

static void test_whitespace_trimming(void)
{
    /* Leading + trailing whitespace must be tolerated — registry
     * strings sometimes pick up incidental whitespace from .sub file
     * parsing or playlist line breaks. */
    subghz_button_caps_t c1 = subghz_button_caps_for_protocol("  KeeLoq");
    subghz_button_caps_t c2 = subghz_button_caps_for_protocol("KeeLoq  ");
    subghz_button_caps_t c3 = subghz_button_caps_for_protocol("  KeeLoq\t");
    subghz_button_caps_t c4 = subghz_button_caps_for_protocol("\n KeeLoq\r\n");
    TEST_ASSERT_TRUE(c1.supports_cycling);
    TEST_ASSERT_TRUE(c2.supports_cycling);
    TEST_ASSERT_TRUE(c3.supports_cycling);
    TEST_ASSERT_TRUE(c4.supports_cycling);
}

static void test_internal_whitespace_not_collapsed(void)
{
    /* "KeeLoq" with internal whitespace must NOT match.  Trimming is
     * only at the ends. */
    subghz_button_caps_t c = subghz_button_caps_for_protocol("Kee Loq");
    TEST_ASSERT_FALSE(c.supports_cycling);
    TEST_ASSERT_EQUAL_UINT8(0, c.button_count);
}

static void test_partial_match_rejected(void)
{
    /* Prefix / suffix substrings must not match. */
    const char *names[] = { "Kee", "KeeLoqExtra", "ToyotaPlus", "X-Toyota" };
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        subghz_button_caps_t c =
            subghz_button_caps_for_protocol(names[i]);
        TEST_ASSERT_FALSE_MESSAGE(c.supports_cycling, names[i]);
    }
}

/*============================================================================*/
/* Invariants                                                                 */
/*============================================================================*/

static void test_supports_cycling_implies_count_ge_2(void)
{
    /* For every supported protocol, button_count must be >= 2.
     * "Supports cycling with 1 button" would be a contradiction. */
    const char *names[] = {
        "KeeLoq", "Jarolift", "Star Line", "Nice FloR-S",
        "CAME Atomo", "CAME TWEE", "Alutech AT-4N",
        "KingGates Stylo4k", "DITEC_GOL4", "Scher-Khan", "Toyota",
    };
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        subghz_button_caps_t c =
            subghz_button_caps_for_protocol(names[i]);
        TEST_ASSERT_TRUE_MESSAGE(c.supports_cycling, names[i]);
        TEST_ASSERT_TRUE_MESSAGE(c.button_count >= 2, names[i]);
    }
}

/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_returns_unsupported);
    RUN_TEST(test_empty_returns_unsupported);
    RUN_TEST(test_unknown_returns_unsupported);
    RUN_TEST(test_static_protocols_unsupported);
    RUN_TEST(test_keeloq_family);
    RUN_TEST(test_nice_flor_s_has_sixteen_buttons);
    RUN_TEST(test_came_family);
    RUN_TEST(test_alutech_and_kinggates);
    RUN_TEST(test_ditec_gol4);
    RUN_TEST(test_scher_khan);
    RUN_TEST(test_toyota);
    RUN_TEST(test_case_insensitive_match);
    RUN_TEST(test_whitespace_trimming);
    RUN_TEST(test_internal_whitespace_not_collapsed);
    RUN_TEST(test_partial_match_rejected);
    RUN_TEST(test_supports_cycling_implies_count_ge_2);
    return UNITY_END();
}
