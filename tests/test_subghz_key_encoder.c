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
    /* Unknown protocol with no TE should still be rejected */
    SubGhzKeyParams params = make_params("TotallyUnknownProtocol", 0, 24, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_ERR_UNSUPPORTED, ret);
}

void test_timing_unknown_with_te_fallback(void)
{
    /* Unknown protocol BUT .sub file provides TE — should use generic 1:3 */
    SubGhzKeyParams params = make_params("TotallyUnknownProtocol", 0, 24, 400);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_OK, ret);

    TEST_ASSERT_EQUAL_UINT32(400, timing.te_short);
    TEST_ASSERT_EQUAL_UINT32(1200, timing.te_long);   /* 400 * 3 */
    TEST_ASSERT_EQUAL_UINT32(12000, timing.gap_low);   /* 400 * 30 */
}

void test_timing_case_insensitive_registry(void)
{
    /* "princeton" (lowercase) should resolve via case-insensitive registry lookup */
    SubGhzKeyParams params = make_params("princeton", 0, 24, 0);
    SubGhzKeyTiming timing;

    uint8_t ret = subghz_key_resolve_timing(&params, &timing);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_KEY_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(370, timing.te_short);
    TEST_ASSERT_EQUAL_UINT32(1140, timing.te_long);
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
 * Magellan custom encoder
 * =================================================================== */

void test_magellan_has_custom_encoder(void)
{
    TEST_ASSERT_TRUE(subghz_key_has_custom_encoder("Magellan"));
    TEST_ASSERT_TRUE(subghz_key_has_custom_encoder("magellan"));   /* case-insensitive */
    TEST_ASSERT_TRUE(subghz_key_has_custom_encoder("MAGELLAN"));
    TEST_ASSERT_FALSE(subghz_key_has_custom_encoder("Princeton"));
    TEST_ASSERT_FALSE(subghz_key_has_custom_encoder(NULL));
}

void test_magellan_encode_structure(void)
{
    /* Verify Magellan waveform structure: header + start bit + 32 data bits + stop bit */
    SubGhzKeyParams params = make_params("Magellan", 0x77D1883F, 32, 0);

    SubGhzRawPair out[60];
    uint32_t count = subghz_key_encode_custom(&params, out, 60, 1);

    /* 1 header burst + 12 header toggles + 1 header end + 1 start bit + 32 data + 1 stop = 48 */
    TEST_ASSERT_EQUAL_UINT32(SUBGHZ_MAGELLAN_PAIRS_PER_REP, count);

    /* Header burst: 800µs HIGH + 200µs LOW */
    TEST_ASSERT_EQUAL_UINT32(800, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(200, out[0].low_us);

    /* Header toggle pairs (indices 1-12): 200µs HIGH + 200µs LOW */
    for (int i = 1; i <= 12; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(200, out[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(200, out[i].low_us);
    }

    /* Header end (index 13): 200µs HIGH + 400µs LOW */
    TEST_ASSERT_EQUAL_UINT32(200, out[13].high_us);
    TEST_ASSERT_EQUAL_UINT32(400, out[13].low_us);

    /* Start bit (index 14): 1200µs HIGH + 400µs LOW */
    TEST_ASSERT_EQUAL_UINT32(1200, out[14].high_us);
    TEST_ASSERT_EQUAL_UINT32(400,  out[14].low_us);

    /* Data starts at index 15, ends at 46 (32 bits) */

    /* Stop bit (index 47): 200µs HIGH + 40000µs LOW */
    TEST_ASSERT_EQUAL_UINT32(200,   out[47].high_us);
    TEST_ASSERT_EQUAL_UINT32(40000, out[47].low_us);
}

void test_magellan_encode_bit_polarity(void)
{
    /* Magellan uses INVERTED polarity:
     * Bit 1: SHORT HIGH (200) + LONG LOW (400)
     * Bit 0: LONG HIGH (400) + SHORT LOW (200) */
    SubGhzKeyParams params = make_params("Magellan", 0xA0000000ULL, 32, 0);

    SubGhzRawPair out[60];
    subghz_key_encode_custom(&params, out, 60, 1);

    /* Data starts at index 15 (after header + start bit)
     * 0xA0000000 = 1010 0000 0000 0000 0000 0000 0000 0000
     * First 4 bits: 1, 0, 1, 0 */

    /* Bit 31 (MSB) = 1 → SHORT HIGH + LONG LOW */
    TEST_ASSERT_EQUAL_UINT32(200, out[15].high_us);
    TEST_ASSERT_EQUAL_UINT32(400, out[15].low_us);

    /* Bit 30 = 0 → LONG HIGH + SHORT LOW */
    TEST_ASSERT_EQUAL_UINT32(400, out[16].high_us);
    TEST_ASSERT_EQUAL_UINT32(200, out[16].low_us);

    /* Bit 29 = 1 → SHORT HIGH + LONG LOW */
    TEST_ASSERT_EQUAL_UINT32(200, out[17].high_us);
    TEST_ASSERT_EQUAL_UINT32(400, out[17].low_us);

    /* Bit 28 = 0 → LONG HIGH + SHORT LOW */
    TEST_ASSERT_EQUAL_UINT32(400, out[18].high_us);
    TEST_ASSERT_EQUAL_UINT32(200, out[18].low_us);
}

void test_magellan_encode_repetitions(void)
{
    SubGhzKeyParams params = make_params("Magellan", 0x12345678, 32, 0);

    SubGhzRawPair out[200];
    uint32_t count = subghz_key_encode_custom(&params, out, 200, 3);

    /* 3 reps × 48 pairs = 144 */
    TEST_ASSERT_EQUAL_UINT32(144, count);

    /* Each repetition should start with the same header burst */
    TEST_ASSERT_EQUAL_UINT32(800, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(800, out[48].high_us);
    TEST_ASSERT_EQUAL_UINT32(800, out[96].high_us);

    /* Each repetition should end with the stop bit */
    TEST_ASSERT_EQUAL_UINT32(40000, out[47].low_us);
    TEST_ASSERT_EQUAL_UINT32(40000, out[95].low_us);
    TEST_ASSERT_EQUAL_UINT32(40000, out[143].low_us);
}

void test_magellan_encode_gulf_star_door_alarm_1(void)
{
    /* Real signal from Gulf Star Marina issue #188:
     * Door Alarm 1: Key: 00 00 00 00 47 AE 88 40
     * → key_value = 0x47AE8840, bit_count = 32 */
    SubGhzKeyParams params = make_params("Magellan", 0x47AE8840ULL, 32, 0);

    SubGhzRawPair out[60];
    uint32_t count = subghz_key_encode_custom(&params, out, 60, 1);
    TEST_ASSERT_EQUAL_UINT32(48, count);

    /* Verify first few data bits: 0x47 = 0100 0111
     * Bit 31 = 0 → LONG HIGH + SHORT LOW
     * Bit 30 = 1 → SHORT HIGH + LONG LOW
     * Bit 29 = 0 → LONG HIGH + SHORT LOW
     * Bit 28 = 0 → LONG HIGH + SHORT LOW
     * Bit 27 = 0 → LONG HIGH + SHORT LOW
     * Bit 26 = 1 → SHORT HIGH + LONG LOW
     * Bit 25 = 1 → SHORT HIGH + LONG LOW
     * Bit 24 = 1 → SHORT HIGH + LONG LOW */
    TEST_ASSERT_EQUAL_UINT32(400, out[15].high_us);  /* bit 31 = 0 */
    TEST_ASSERT_EQUAL_UINT32(200, out[16].high_us);  /* bit 30 = 1 */
    TEST_ASSERT_EQUAL_UINT32(400, out[17].high_us);  /* bit 29 = 0 */
    TEST_ASSERT_EQUAL_UINT32(400, out[18].high_us);  /* bit 28 = 0 */
    TEST_ASSERT_EQUAL_UINT32(400, out[19].high_us);  /* bit 27 = 0 */
    TEST_ASSERT_EQUAL_UINT32(200, out[20].high_us);  /* bit 26 = 1 */
    TEST_ASSERT_EQUAL_UINT32(200, out[21].high_us);  /* bit 25 = 1 */
    TEST_ASSERT_EQUAL_UINT32(200, out[22].high_us);  /* bit 24 = 1 */
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

    /* TE fallback and case-insensitive */
    RUN_TEST(test_timing_unknown_with_te_fallback);
    RUN_TEST(test_timing_case_insensitive_registry);

    /* Magellan custom encoder */
    RUN_TEST(test_magellan_has_custom_encoder);
    RUN_TEST(test_magellan_encode_structure);
    RUN_TEST(test_magellan_encode_bit_polarity);
    RUN_TEST(test_magellan_encode_repetitions);
    RUN_TEST(test_magellan_encode_gulf_star_door_alarm_1);

    return UNITY_END();
}
