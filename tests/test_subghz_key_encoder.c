/* See COPYING.txt for license details. */

/*
 * test_subghz_key_encoder.c
 *
 * Unit tests for subghz_key_encoder — the KEY→RAW OOK PWM encoding
 * engine extracted from sub_ghz_replay_flipper_file().
 *
 * Tests cover:
 *   - Registry-based timing resolution (Princeton, CAME, etc.)
 *   - Legacy strstr() fallback paths
 *   - Rolling-code / weather / TPMS rejection
 *   - PWM bit encoding correctness (1:3 and 1:2 ratios)
 *   - TE override behaviour
 *   - Bit ordering (MSB first)
 *   - Sync gap generation
 *   - Multi-repetition output
 *   - Edge cases: zero bits, overflow, 64-bit clamp
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>
#include "unity.h"
#include "subghz_key_encoder.h"

void setUp(void) {}
void tearDown(void) {}

/* Helper — make a key params struct */
static SubGhzKeyParams make_params(const char *proto, uint64_t key,
                                    uint32_t bits, uint32_t te)
{
    SubGhzKeyParams p;
    memset(&p, 0, sizeof(p));
    strncpy(p.protocol, proto, sizeof(p.protocol) - 1);
    p.key_value = key;
    p.bit_count = bits;
    p.te        = te;
    return p;
}

/* ===================================================================
 * Timing resolution — registry-based
 * =================================================================== */

void test_timing_princeton_registry(void)
{
    SubGhzKeyParams params = make_params("Princeton", 0, 24, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_OK, ret);

    /* Princeton registry: te_short=370, te_long=1140 */
    TEST_ASSERT_EQUAL_UINT32(370, timing.te_short);
    TEST_ASSERT_EQUAL_UINT32(1140, timing.te_long);
    TEST_ASSERT_EQUAL_UINT32(370 * 30, timing.gap_low);
}

void test_timing_came_registry(void)
{
    SubGhzKeyParams params = make_params("CAME", 0, 12, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_OK, ret);

    /* CAME: te_short=320, te_long=640 (1:2 ratio) */
    TEST_ASSERT_EQUAL_UINT32(320, timing.te_short);
    TEST_ASSERT_EQUAL_UINT32(640, timing.te_long);
}

void test_timing_te_override(void)
{
    /* Override TE=400 for Princeton (registry te_short=370, te_long=1140) */
    SubGhzKeyParams params = make_params("Princeton", 0, 24, 400);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_OK, ret);

    TEST_ASSERT_EQUAL_UINT32(400, timing.te_short);
    /* te_long = te_override * registry_te_long / registry_te_short
     *         = 400 * 1140 / 370 = 1232 (integer division) */
    TEST_ASSERT_EQUAL_UINT32(1232, timing.te_long);
    TEST_ASSERT_EQUAL_UINT32(400 * 30, timing.gap_low);
}

/* ===================================================================
 * Timing resolution — legacy fallback
 * =================================================================== */

void test_timing_gate_tx_fallback(void)
{
    /* Gate_TX should hit registry first, but if protocol name is
     * "GateTX_Custom" (not in registry), falls to strstr("Gate") */
    SubGhzKeyParams params = make_params("GateTX_Custom", 0, 24, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_OK, ret);

    /* strstr("Gate") → 1:3 ratio, default TE=350 */
    TEST_ASSERT_EQUAL_UINT32(350, timing.te_short);
    TEST_ASSERT_EQUAL_UINT32(1050, timing.te_long);
}

void test_timing_nice_fallback(void)
{
    SubGhzKeyParams params = make_params("Nice_Custom", 0, 12, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_OK, ret);

    /* strstr("Nice") → 1:2 ratio, default TE=320 */
    TEST_ASSERT_EQUAL_UINT32(320, timing.te_short);
    TEST_ASSERT_EQUAL_UINT32(640, timing.te_long);
    TEST_ASSERT_EQUAL_UINT32(320 * 36, timing.gap_low);
}

/* ===================================================================
 * Timing resolution — rejection
 * =================================================================== */

void test_timing_dynamic_rejected(void)
{
    SubGhzKeyParams params = make_params("KeeLoq", 0, 66, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_ERR_DYNAMIC, ret);
}

void test_timing_weather_rejected(void)
{
    /* Acurite_592TXR is a weather protocol */
    SubGhzKeyParams params = make_params("Acurite_592TXR", 0, 56, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_ERR_DYNAMIC, ret);
}

void test_timing_unknown_rejected(void)
{
    SubGhzKeyParams params = make_params("TotallyUnknownProtocol", 0, 24, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_ERR_UNSUPPORTED, ret);
}

void test_timing_null_params(void)
{
    SubGhzKeyTiming timing;
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_ERR_UNSUPPORTED,
        subghz_key_resolve_timing(NULL, &timing));
}

/* ===================================================================
 * Encoding — Princeton 24-bit (1:3 ratio)
 * =================================================================== */

void test_encode_princeton_bit_pattern(void)
{
    /* Key 0xAA = 10101010 in 8 bits.
     * At 1:3 ratio (te=350):
     *   Bit 1 → HIGH=1050, LOW=350
     *   Bit 0 → HIGH=350,  LOW=1050
     */
    SubGhzKeyParams params = make_params("Princeton", 0xAA, 8, 0);
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };

    SubGhzRawPair out[20];
    uint32_t count = subghz_key_encode(&params, &timing, out, 20, 1);

    /* 8 data pairs + 1 sync = 9 */
    TEST_ASSERT_EQUAL_UINT32(9, count);

    /* Check alternating pattern (MSB first): 1,0,1,0,1,0,1,0 */
    for (int i = 0; i < 8; i++)
    {
        if (i % 2 == 0)
        {
            /* Bit 1 */
            TEST_ASSERT_EQUAL_UINT32(1050, out[i].high_us);
            TEST_ASSERT_EQUAL_UINT32(350,  out[i].low_us);
        }
        else
        {
            /* Bit 0 */
            TEST_ASSERT_EQUAL_UINT32(350,  out[i].high_us);
            TEST_ASSERT_EQUAL_UINT32(1050, out[i].low_us);
        }
    }

    /* Sync gap */
    TEST_ASSERT_EQUAL_UINT32(350,   out[8].high_us);
    TEST_ASSERT_EQUAL_UINT32(10500, out[8].low_us);
}

void test_encode_all_ones(void)
{
    /* 0xFF in 8 bits — all bits 1 */
    SubGhzKeyParams params = make_params("Princeton", 0xFF, 8, 0);
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };

    SubGhzRawPair out[20];
    uint32_t count = subghz_key_encode(&params, &timing, out, 20, 1);
    TEST_ASSERT_EQUAL_UINT32(9, count);

    for (int i = 0; i < 8; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(1050, out[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(350,  out[i].low_us);
    }
}

void test_encode_all_zeros(void)
{
    /* 0x00 in 8 bits — all bits 0 */
    SubGhzKeyParams params = make_params("Princeton", 0x00, 8, 0);
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };

    SubGhzRawPair out[20];
    uint32_t count = subghz_key_encode(&params, &timing, out, 20, 1);
    TEST_ASSERT_EQUAL_UINT32(9, count);

    for (int i = 0; i < 8; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(350,  out[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(1050, out[i].low_us);
    }
}

/* ===================================================================
 * Encoding — CAME 12-bit (1:2 ratio)
 * =================================================================== */

void test_encode_came_12bit(void)
{
    /* Key 0xFFF in 12 bits, all ones */
    SubGhzKeyParams params = make_params("CAME", 0xFFF, 12, 0);
    SubGhzKeyTiming timing = { .te_short = 320, .te_long = 640, .gap_low = 11520 };

    SubGhzRawPair out[20];
    uint32_t count = subghz_key_encode(&params, &timing, out, 20, 1);

    /* 12 data + 1 sync = 13 */
    TEST_ASSERT_EQUAL_UINT32(13, count);

    /* All 1s: long HIGH, short LOW */
    for (int i = 0; i < 12; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(640, out[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(320, out[i].low_us);
    }
}

/* ===================================================================
 * Encoding — multiple repetitions
 * =================================================================== */

void test_encode_3_repetitions(void)
{
    SubGhzKeyParams params = make_params("Princeton", 0x5, 4, 0);
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };

    SubGhzRawPair out[100];
    uint32_t count = subghz_key_encode(&params, &timing, out, 100, 3);

    /* 3 reps × (4 data + 1 sync) = 15 */
    TEST_ASSERT_EQUAL_UINT32(15, count);

    /* Each repetition should be identical */
    for (int rep = 0; rep < 3; rep++)
    {
        int base = rep * 5;
        /* 0x5 = 0101 in 4 bits */
        /* Bit 0 */
        TEST_ASSERT_EQUAL_UINT32(350,  out[base + 0].high_us);
        /* Bit 1 */
        TEST_ASSERT_EQUAL_UINT32(1050, out[base + 1].high_us);
        /* Bit 0 */
        TEST_ASSERT_EQUAL_UINT32(350,  out[base + 2].high_us);
        /* Bit 1 */
        TEST_ASSERT_EQUAL_UINT32(1050, out[base + 3].high_us);
        /* Sync */
        TEST_ASSERT_EQUAL_UINT32(350,   out[base + 4].high_us);
        TEST_ASSERT_EQUAL_UINT32(10500, out[base + 4].low_us);
    }
}

/* ===================================================================
 * Edge cases
 * =================================================================== */

void test_encode_zero_bits_returns_zero(void)
{
    SubGhzKeyParams params = make_params("Princeton", 0, 0, 0);
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };

    SubGhzRawPair out[10];
    uint32_t count = subghz_key_encode(&params, &timing, out, 10, 1);
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_encode_overflow_returns_zero(void)
{
    SubGhzKeyParams params = make_params("Princeton", 0xFFFFFF, 24, 0);
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };

    /* Buffer too small: 24 data + 1 sync = 25, but only 10 slots */
    SubGhzRawPair out[10];
    uint32_t count = subghz_key_encode(&params, &timing, out, 10, 1);
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_encode_65_bit_clamped_to_64(void)
{
    SubGhzKeyParams params = make_params("Princeton", 0xFFFFFFFFFFFFFFFFULL, 65, 0);
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };

    /* 65 bits → clamped to 64 → 64 data + 1 sync = 65 pairs */
    SubGhzRawPair out[70];
    uint32_t count = subghz_key_encode(&params, &timing, out, 70, 1);
    TEST_ASSERT_EQUAL_UINT32(65, count);
}

void test_encode_null_params_returns_zero(void)
{
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };
    SubGhzRawPair out[10];

    TEST_ASSERT_EQUAL_UINT32(0, subghz_key_encode(NULL, &timing, out, 10, 1));
}

void test_encode_msb_first(void)
{
    /* 0b1100 in 4 bits → MSB first: bit3=1, bit2=1, bit1=0, bit0=0 */
    SubGhzKeyParams params = make_params("Princeton", 0xC, 4, 0);
    SubGhzKeyTiming timing = { .te_short = 350, .te_long = 1050, .gap_low = 10500 };

    SubGhzRawPair out[10];
    subghz_key_encode(&params, &timing, out, 10, 1);

    /* Bit 3 (MSB) = 1 → long HIGH */
    TEST_ASSERT_EQUAL_UINT32(1050, out[0].high_us);
    /* Bit 2 = 1 → long HIGH */
    TEST_ASSERT_EQUAL_UINT32(1050, out[1].high_us);
    /* Bit 1 = 0 → short HIGH */
    TEST_ASSERT_EQUAL_UINT32(350,  out[2].high_us);
    /* Bit 0 (LSB) = 0 → short HIGH */
    TEST_ASSERT_EQUAL_UINT32(350,  out[3].high_us);
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Timing resolution — registry */
    RUN_TEST(test_timing_princeton_registry);
    RUN_TEST(test_timing_came_registry);
    RUN_TEST(test_timing_te_override);

    /* Timing resolution — fallback */
    RUN_TEST(test_timing_gate_tx_fallback);
    RUN_TEST(test_timing_nice_fallback);

    /* Timing resolution — rejection */
    RUN_TEST(test_timing_dynamic_rejected);
    RUN_TEST(test_timing_weather_rejected);
    RUN_TEST(test_timing_unknown_rejected);
    RUN_TEST(test_timing_null_params);

    /* Encoding — Princeton */
    RUN_TEST(test_encode_princeton_bit_pattern);
    RUN_TEST(test_encode_all_ones);
    RUN_TEST(test_encode_all_zeros);

    /* Encoding — CAME */
    RUN_TEST(test_encode_came_12bit);

    /* Encoding — repetitions */
    RUN_TEST(test_encode_3_repetitions);

    /* Edge cases */
    RUN_TEST(test_encode_zero_bits_returns_zero);
    RUN_TEST(test_encode_overflow_returns_zero);
    RUN_TEST(test_encode_65_bit_clamped_to_64);
    RUN_TEST(test_encode_null_params_returns_zero);
    RUN_TEST(test_encode_msb_first);

    return UNITY_END();
}
