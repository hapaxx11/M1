/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_button_override.c
 * @brief  Host tests for Sub-GHz button-override key mutation (Phase 4c).
 *
 * Pure-logic coverage of subghz_button_override_apply() / _supports():
 *
 *   1. Supported protocols (KeeLoq, Jarolift, Star Line) — verify the
 *      correct bit-field is mutated for each button index 0..15.
 *   2. KeeLoq + Jarolift share the [63:60] layout — verify mutation
 *      preserves serial (bits 32..59) and hop (bits 0..31).
 *   3. Star Line — verify mutation preserves serial (bits 4..31) and
 *      hop (bits 32..63).
 *   4. Round-trip: applying button N then back to original button
 *      restores the original key.
 *   5. Cycle uniqueness: 4 distinct buttons → 4 distinct keys for
 *      KeeLoq and Star Line (the protocols actually exposed by
 *      Phase 4a's button_caps with button_count=4).
 *   6. Unsupported protocols — verify FloR-S, CAME Atomo, Alutech
 *      AT-4N, KingGates Stylo4k, DITEC_GOL4, FAAC SLH, Toyota,
 *      Scher-Khan, an unknown protocol, and NULL return false.
 *   7. NULL key_out parameter — verify safe failure.
 *   8. Case-insensitive + whitespace-tolerant matching.
 *   9. button_index > 15 — verify wrap-around to lower nibble.
 *  10. _supports() agrees with _apply() return value across the table.
 */

#include "unity.h"
#include "subghz_button_override.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* KeeLoq layout: button at bits [63:60], serial at [59:32], hop at [31:0]    */
/*============================================================================*/

static uint64_t kl_key(uint8_t button, uint32_t serial28, uint32_t hop32)
{
    return ((uint64_t)(button & 0x0FU) << 60) |
           ((uint64_t)(serial28 & 0x0FFFFFFFUL) << 32) |
           (uint64_t)hop32;
}

void test_keeloq_button_override_basic(void)
{
    uint64_t in  = kl_key(/*button=*/0u, /*serial=*/0x123456u, /*hop=*/0xDEADBEEFu);
    uint64_t out = 0;

    TEST_ASSERT_TRUE(subghz_button_override_apply("KeeLoq", in, 3u, &out));

    /* Expect button = 3 (bits 63..60), serial and hop preserved. */
    TEST_ASSERT_EQUAL_HEX64(kl_key(3u, 0x123456u, 0xDEADBEEFu), out);
}

void test_keeloq_button_override_preserves_serial_and_hop(void)
{
    const uint32_t serial = 0x0ABCDEFu;
    const uint32_t hop    = 0x12345678u;

    for (uint8_t b = 0; b < 4u; ++b)
    {
        uint64_t in  = kl_key(0u, serial, hop);
        uint64_t out = 0;
        TEST_ASSERT_TRUE(subghz_button_override_apply("KeeLoq", in, b, &out));

        TEST_ASSERT_EQUAL_HEX32(hop,    (uint32_t)(out & 0xFFFFFFFFUL));
        TEST_ASSERT_EQUAL_HEX32(serial, (uint32_t)((out >> 32) & 0x0FFFFFFFUL));
        TEST_ASSERT_EQUAL_HEX8(b,       (uint8_t)((out >> 60) & 0x0FU));
    }
}

void test_keeloq_button_override_all_16_indices(void)
{
    uint64_t in = kl_key(0xFu, 0x0u, 0x0u);
    for (uint8_t b = 0; b < 16u; ++b)
    {
        uint64_t out = 0;
        TEST_ASSERT_TRUE(subghz_button_override_apply("KeeLoq", in, b, &out));
        TEST_ASSERT_EQUAL_HEX8(b, (uint8_t)((out >> 60) & 0x0FU));
    }
}

void test_keeloq_button_override_index_wraps_above_15(void)
{
    /* Higher bits of button_index must be ignored (mask 0x0F). */
    uint64_t in = kl_key(0u, 0xAAAAAAAu, 0x55555555u);
    uint64_t out = 0;

    TEST_ASSERT_TRUE(subghz_button_override_apply("KeeLoq", in, 0xF7u, &out));
    /* 0xF7 & 0x0F = 0x7 */
    TEST_ASSERT_EQUAL_HEX8(0x7u, (uint8_t)((out >> 60) & 0x0FU));
}

void test_jarolift_button_override_uses_keeloq_layout(void)
{
    uint64_t in  = kl_key(0u, 0x07F0F0Fu, 0xCAFEBABEu);
    uint64_t out = 0;

    TEST_ASSERT_TRUE(subghz_button_override_apply("Jarolift", in, 2u, &out));
    TEST_ASSERT_EQUAL_HEX64(kl_key(2u, 0x07F0F0Fu, 0xCAFEBABEu), out);
}

/*============================================================================*/
/* Star Line layout: button at bits [3:0], serial at [31:4], hop at [63:32]   */
/*============================================================================*/

static uint64_t sl_key(uint8_t button, uint32_t serial28, uint32_t hop32)
{
    return ((uint64_t)hop32 << 32) |
           ((uint64_t)(serial28 & 0x0FFFFFFFUL) << 4) |
           (uint64_t)(button & 0x0FU);
}

void test_starline_button_override_basic(void)
{
    uint64_t in  = sl_key(/*button=*/0xFu, /*serial=*/0x1234567u, /*hop=*/0xDEADBEEFu);
    uint64_t out = 0;

    TEST_ASSERT_TRUE(subghz_button_override_apply("Star Line", in, 1u, &out));
    TEST_ASSERT_EQUAL_HEX64(sl_key(1u, 0x1234567u, 0xDEADBEEFu), out);
}

void test_starline_button_override_preserves_serial_and_hop(void)
{
    const uint32_t serial = 0x09ABCDEu;
    const uint32_t hop    = 0xFEEDFACEu;

    for (uint8_t b = 0; b < 4u; ++b)
    {
        uint64_t in  = sl_key(0xFu, serial, hop);
        uint64_t out = 0;
        TEST_ASSERT_TRUE(subghz_button_override_apply("Star Line", in, b, &out));

        TEST_ASSERT_EQUAL_HEX32(hop,    (uint32_t)(out >> 32));
        TEST_ASSERT_EQUAL_HEX32(serial, (uint32_t)((out >> 4) & 0x0FFFFFFFUL));
        TEST_ASSERT_EQUAL_HEX8(b,       (uint8_t)(out & 0x0FU));
    }
}

/*============================================================================*/
/* Round-trip & uniqueness                                                    */
/*============================================================================*/

void test_keeloq_round_trip_restores_original(void)
{
    uint64_t orig = kl_key(2u, 0x05A5A5Au, 0x11223344u);
    uint64_t mut  = 0;
    uint64_t back = 0;

    TEST_ASSERT_TRUE(subghz_button_override_apply("KeeLoq", orig, 0xFu, &mut));
    TEST_ASSERT_NOT_EQUAL(orig, mut);
    TEST_ASSERT_TRUE(subghz_button_override_apply("KeeLoq", mut, 2u, &back));
    TEST_ASSERT_EQUAL_HEX64(orig, back);
}

void test_keeloq_4_buttons_produce_4_distinct_keys(void)
{
    uint64_t in   = kl_key(0u, 0x0123456u, 0x89ABCDEFu);
    uint64_t k[4] = { 0 };

    for (uint8_t b = 0; b < 4u; ++b)
    {
        TEST_ASSERT_TRUE(subghz_button_override_apply("KeeLoq", in, b, &k[b]));
    }
    /* Pairwise distinct. */
    TEST_ASSERT_NOT_EQUAL(k[0], k[1]);
    TEST_ASSERT_NOT_EQUAL(k[0], k[2]);
    TEST_ASSERT_NOT_EQUAL(k[0], k[3]);
    TEST_ASSERT_NOT_EQUAL(k[1], k[2]);
    TEST_ASSERT_NOT_EQUAL(k[1], k[3]);
    TEST_ASSERT_NOT_EQUAL(k[2], k[3]);
}

void test_starline_4_buttons_produce_4_distinct_keys(void)
{
    uint64_t in   = sl_key(0u, 0x0123456u, 0x89ABCDEFu);
    uint64_t k[4] = { 0 };

    for (uint8_t b = 0; b < 4u; ++b)
    {
        TEST_ASSERT_TRUE(subghz_button_override_apply("Star Line", in, b, &k[b]));
    }
    TEST_ASSERT_NOT_EQUAL(k[0], k[1]);
    TEST_ASSERT_NOT_EQUAL(k[0], k[2]);
    TEST_ASSERT_NOT_EQUAL(k[0], k[3]);
    TEST_ASSERT_NOT_EQUAL(k[1], k[2]);
    TEST_ASSERT_NOT_EQUAL(k[1], k[3]);
    TEST_ASSERT_NOT_EQUAL(k[2], k[3]);
}

/*============================================================================*/
/* Unsupported protocols                                                       */
/*============================================================================*/

void test_unsupported_protocols_return_false(void)
{
    static const char *const unsupported[] = {
        "Nice FloR-S",
        "CAME Atomo",
        "CAME TWEE",
        "Alutech AT-4N",
        "KingGates Stylo4k",
        "DITEC_GOL4",
        "FAAC SLH",
        "Toyota",
        "Scher-Khan",
        "Princeton",   /* static OOK — no button cycling */
        "GarbageProto",
    };
    const uint64_t in = 0xDEADBEEFCAFEBABEull;

    for (size_t i = 0; i < sizeof(unsupported)/sizeof(unsupported[0]); ++i)
    {
        uint64_t out = 0;
        bool ok = subghz_button_override_apply(unsupported[i], in, 2u, &out);
        TEST_ASSERT_FALSE_MESSAGE(ok, unsupported[i]);
        /* On unsupported, *key_out is set to key_in so callers can use
         * a single assignment path. */
        TEST_ASSERT_EQUAL_HEX64(in, out);

        TEST_ASSERT_FALSE_MESSAGE(subghz_button_override_supports(unsupported[i]),
                                   unsupported[i]);
    }
}

void test_null_protocol_returns_false(void)
{
    uint64_t out = 0x1234ull;
    bool ok = subghz_button_override_apply(NULL, 0xAAAAull, 1u, &out);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_HEX64(0xAAAAull, out);
    TEST_ASSERT_FALSE(subghz_button_override_supports(NULL));
}

void test_null_key_out_returns_false(void)
{
    /* Must not crash; must return false. */
    bool ok = subghz_button_override_apply("KeeLoq", 0xDEADBEEFu, 1u, NULL);
    TEST_ASSERT_FALSE(ok);
}

/*============================================================================*/
/* Case / whitespace tolerance                                                */
/*============================================================================*/

void test_case_insensitive_protocol_match(void)
{
    uint64_t out = 0;
    TEST_ASSERT_TRUE(subghz_button_override_apply("keeloq",    0u, 5u, &out));
    TEST_ASSERT_EQUAL_HEX8(5u, (uint8_t)((out >> 60) & 0x0FU));

    TEST_ASSERT_TRUE(subghz_button_override_apply("KEELOQ",    0u, 6u, &out));
    TEST_ASSERT_EQUAL_HEX8(6u, (uint8_t)((out >> 60) & 0x0FU));

    TEST_ASSERT_TRUE(subghz_button_override_apply("star line", 0u, 7u, &out));
    TEST_ASSERT_EQUAL_HEX8(7u, (uint8_t)(out & 0x0FU));

    TEST_ASSERT_TRUE(subghz_button_override_apply("JaRoLiFt",  0u, 4u, &out));
    TEST_ASSERT_EQUAL_HEX8(4u, (uint8_t)((out >> 60) & 0x0FU));
}

void test_whitespace_trimmed_protocol_match(void)
{
    uint64_t out = 0;
    TEST_ASSERT_TRUE(subghz_button_override_apply("  KeeLoq  ", 0u, 1u, &out));
    TEST_ASSERT_EQUAL_HEX8(1u, (uint8_t)((out >> 60) & 0x0FU));
    TEST_ASSERT_TRUE(subghz_button_override_supports("\tStar Line "));
}

/*============================================================================*/
/* _supports() agrees with _apply()                                            */
/*============================================================================*/

void test_supports_agrees_with_apply(void)
{
    static const char *const all_protos[] = {
        /* supported */
        "KeeLoq", "Jarolift", "Star Line",
        /* unsupported */
        "Nice FloR-S", "CAME Atomo", "Alutech AT-4N", "Toyota",
        "DITEC_GOL4", "KingGates Stylo4k", "FAAC SLH", "Unknown",
    };
    for (size_t i = 0; i < sizeof(all_protos)/sizeof(all_protos[0]); ++i)
    {
        uint64_t out = 0;
        bool supports = subghz_button_override_supports(all_protos[i]);
        bool applied  = subghz_button_override_apply(all_protos[i], 0ull, 0u, &out);
        TEST_ASSERT_EQUAL_MESSAGE(supports, applied, all_protos[i]);
    }
}

/*============================================================================*/
/* Test runner                                                                */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_keeloq_button_override_basic);
    RUN_TEST(test_keeloq_button_override_preserves_serial_and_hop);
    RUN_TEST(test_keeloq_button_override_all_16_indices);
    RUN_TEST(test_keeloq_button_override_index_wraps_above_15);
    RUN_TEST(test_jarolift_button_override_uses_keeloq_layout);
    RUN_TEST(test_starline_button_override_basic);
    RUN_TEST(test_starline_button_override_preserves_serial_and_hop);
    RUN_TEST(test_keeloq_round_trip_restores_original);
    RUN_TEST(test_keeloq_4_buttons_produce_4_distinct_keys);
    RUN_TEST(test_starline_4_buttons_produce_4_distinct_keys);
    RUN_TEST(test_unsupported_protocols_return_false);
    RUN_TEST(test_null_protocol_returns_false);
    RUN_TEST(test_null_key_out_returns_false);
    RUN_TEST(test_case_insensitive_protocol_match);
    RUN_TEST(test_whitespace_trimmed_protocol_match);
    RUN_TEST(test_supports_agrees_with_apply);
    return UNITY_END();
}
