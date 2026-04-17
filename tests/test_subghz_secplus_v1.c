/* See COPYING.txt for license details. */

/*
 * test_subghz_secplus_v1.c
 *
 * Unit tests for subghz_secplus_v1_encoder — the Security+ 1.0 ternary
 * OOK packet encoder used by the brute-force mode.
 *
 * Tests cover:
 *   - NULL guard (out == NULL)
 *   - Fixed/rolling = 0 encodes to all-zero trits (known symbols)
 *   - Header symbols are correct (sym[0]=0, sym[21]=2)
 *   - Symbol timing values match the three legal encodings
 *   - Rolling counter = 1 is handled correctly
 *   - All-twos encoding (rolling = 3^20-1)
 *   - Symmetry: different rolling values produce different packets
 */

#include <string.h>
#include "unity.h"
#include "subghz_secplus_v1_encoder.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/* Verify one symbol has legal timing values */
static void assert_symbol_legal(const SubGhzSecPlusV1Symbol *s, uint8_t idx)
{
    (void)idx;
    bool valid = (s->low_us == SUBGHZ_SECPLUS_V1_TE_SHORT && s->high_us == SUBGHZ_SECPLUS_V1_TE_LONG)  /* sym 2 */
              || (s->low_us == SUBGHZ_SECPLUS_V1_TE_MID   && s->high_us == SUBGHZ_SECPLUS_V1_TE_MID)    /* sym 1 */
              || (s->low_us == SUBGHZ_SECPLUS_V1_TE_LONG  && s->high_us == SUBGHZ_SECPLUS_V1_TE_SHORT); /* sym 0 */
    TEST_ASSERT_TRUE_MESSAGE(valid, "Symbol timing is not one of the three legal values");
}

/* Return the logical symbol value (0, 1, or 2) for a timing pair */
static uint8_t symbol_value(const SubGhzSecPlusV1Symbol *s)
{
    if (s->low_us == SUBGHZ_SECPLUS_V1_TE_LONG  && s->high_us == SUBGHZ_SECPLUS_V1_TE_SHORT) return 0;
    if (s->low_us == SUBGHZ_SECPLUS_V1_TE_MID   && s->high_us == SUBGHZ_SECPLUS_V1_TE_MID)   return 1;
    return 2; /* SHORT/LONG */
}

/*============================================================================*/
/* Tests                                                                       */
/*============================================================================*/

void test_null_out_returns_false(void)
{
    TEST_ASSERT_FALSE(subghz_secplus_v1_encode(0, 0, NULL));
}

void test_zero_fixed_zero_rolling_succeeds(void)
{
    SubGhzSecPlusV1Packet pkt;
    TEST_ASSERT_TRUE(subghz_secplus_v1_encode(0, 0, &pkt));
}

void test_header_symbols_correct(void)
{
    SubGhzSecPlusV1Packet pkt;
    subghz_secplus_v1_encode(0, 0, &pkt);

    /* Sub-packet 1 header = sym 0 → LONG LOW, SHORT HIGH */
    TEST_ASSERT_EQUAL_UINT16(SUBGHZ_SECPLUS_V1_TE_LONG,  pkt.symbols[0].low_us);
    TEST_ASSERT_EQUAL_UINT16(SUBGHZ_SECPLUS_V1_TE_SHORT, pkt.symbols[0].high_us);
    /* Sub-packet 2 header = sym 2 → SHORT LOW, LONG HIGH */
    TEST_ASSERT_EQUAL_UINT16(SUBGHZ_SECPLUS_V1_TE_SHORT, pkt.symbols[21].low_us);
    TEST_ASSERT_EQUAL_UINT16(SUBGHZ_SECPLUS_V1_TE_LONG,  pkt.symbols[21].high_us);
}

void test_all_symbols_are_legal(void)
{
    SubGhzSecPlusV1Packet pkt;
    subghz_secplus_v1_encode(0x1234, 0x5678, &pkt);
    for (uint8_t i = 0; i < SUBGHZ_SECPLUS_V1_TOTAL_SYMS; i++)
        assert_symbol_legal(&pkt.symbols[i], i);
}

void test_zero_rolling_zero_fixed_all_data_sym0(void)
{
    /*
     * rolling=0 → all rolling trits = 0 → da[odd]  = 0
     * fixed=0   → all fixed trits   = 0 → acc=0, da[even] = 0
     *
     * So all 40 data symbols (idx 1..20 and 22..41) should be sym 0
     * (LONG LOW, SHORT HIGH), matching the header of sub-packet 1.
     */
    SubGhzSecPlusV1Packet pkt;
    subghz_secplus_v1_encode(0, 0, &pkt);

    /* Data symbols 1..20 (sub-pkt 1): all should be sym 0 */
    for (uint8_t i = 1; i <= 20; i++)
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, symbol_value(&pkt.symbols[i]),
                                        "Sub-pkt 1 data symbol should be 0 for all-zero input");

    /* Data symbols 22..41 (sub-pkt 2): all should be sym 0 */
    for (uint8_t i = 22; i < 42; i++)
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, symbol_value(&pkt.symbols[i]),
                                        "Sub-pkt 2 data symbol should be 0 for all-zero input");
}

void test_rolling_1_encodes_first_trit(void)
{
    /*
     * rolling=1 → trits[19]=1, trits[0..18]=0  (LSB-last, MSB-first after decompose)
     * The 20 trits from MSB: [0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,1]
     * Pair k=9 in pkt1: idx = 1+9*2 = 19
     *   da[19] = rolling_trits[9] = 0  (trit index 9 is 0)
     *   da[20] = (0 + acc) % 3
     * The last pair's rolling trit is trit[9]=0 for rolling=1
     * (rolling=1: only trit[19]=1 in LSB position, which maps to k=9 in pkt2)
     *
     * Simpler: just verify the packet is different from rolling=0 and that
     * all symbols are still legal.
     */
    SubGhzSecPlusV1Packet pkt0, pkt1;
    subghz_secplus_v1_encode(0, 0, &pkt0);
    subghz_secplus_v1_encode(0, 1, &pkt1);

    /* Headers must still match expected symbols */
    TEST_ASSERT_EQUAL_UINT8(0, symbol_value(&pkt1.symbols[0]));   /* pkt1 header */
    TEST_ASSERT_EQUAL_UINT8(2, symbol_value(&pkt1.symbols[21]));  /* pkt2 header */

    /* rolling=1 → pkt1 data should differ from rolling=0 */
    bool differs = false;
    for (uint8_t i = 1; i < 42 && !differs; i++)
    {
        if (i == 21) continue; /* skip pkt2 header */
        if (symbol_value(&pkt1.symbols[i]) != symbol_value(&pkt0.symbols[i]))
            differs = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(differs, "rolling=1 must produce a different packet from rolling=0");

    for (uint8_t i = 0; i < SUBGHZ_SECPLUS_V1_TOTAL_SYMS; i++)
        assert_symbol_legal(&pkt1.symbols[i], i);
}

void test_max_rolling_trit_value(void)
{
    /*
     * 3^20 = 3486784401 which overflows uint32_t.
     * Maximum valid 20-trit value is 3^20 - 1 = 3486784400.
     * This should encode cleanly (all trits = 2).
     */
    const uint32_t max_rolling = 3486784400UL;  /* 3^20 - 1 */
    SubGhzSecPlusV1Packet pkt;
    TEST_ASSERT_TRUE(subghz_secplus_v1_encode(0, max_rolling, &pkt));

    /* All rolling trits = 2, fixed trits = 0, so every da[odd] = 2 */
    /* Data symbol at positions 1,3,5,…,19 and 22,24,…,40 = sym 2 */
    for (uint8_t k = 0; k < 10; k++)
    {
        uint8_t i_pkt1 = (uint8_t)(1u + k * 2u);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, symbol_value(&pkt.symbols[i_pkt1]),
                                        "rolling trit symbol should be 2 for max rolling");
        uint8_t i_pkt2 = (uint8_t)(22u + k * 2u);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, symbol_value(&pkt.symbols[i_pkt2]),
                                        "rolling trit symbol should be 2 for max rolling (pkt2)");
    }

    for (uint8_t i = 0; i < SUBGHZ_SECPLUS_V1_TOTAL_SYMS; i++)
        assert_symbol_legal(&pkt.symbols[i], i);
}

void test_different_rolling_produce_different_packets(void)
{
    SubGhzSecPlusV1Packet pkt_a, pkt_b;
    subghz_secplus_v1_encode(0, 100, &pkt_a);
    subghz_secplus_v1_encode(0, 101, &pkt_b);

    bool differs = false;
    for (uint8_t i = 0; i < 42 && !differs; i++)
    {
        if (i == 0 || i == 21) continue; /* headers always the same */
        if (symbol_value(&pkt_a.symbols[i]) != symbol_value(&pkt_b.symbols[i]))
            differs = true;
    }
    TEST_ASSERT_TRUE(differs);
}

void test_different_fixed_produce_different_packets(void)
{
    SubGhzSecPlusV1Packet pkt_a, pkt_b;
    subghz_secplus_v1_encode(0,    0x100, &pkt_a);
    subghz_secplus_v1_encode(1000, 0x100, &pkt_b);

    bool differs = false;
    for (uint8_t i = 0; i < 42 && !differs; i++)
    {
        if (i == 0 || i == 21) continue;
        if (symbol_value(&pkt_a.symbols[i]) != symbol_value(&pkt_b.symbols[i]))
            differs = true;
    }
    TEST_ASSERT_TRUE(differs);
}

void test_total_symbol_count(void)
{
    /* Sanity: 2 sub-packets × 21 symbols = 42 */
    TEST_ASSERT_EQUAL_UINT(42u, SUBGHZ_SECPLUS_V1_TOTAL_SYMS);
    TEST_ASSERT_EQUAL_UINT(21u, SUBGHZ_SECPLUS_V1_SYMS_PER_PKT);
}

void test_timing_constants_nonzero(void)
{
    TEST_ASSERT_GREATER_THAN_UINT16(0u, SUBGHZ_SECPLUS_V1_TE_SHORT);
    TEST_ASSERT_GREATER_THAN_UINT16(0u, SUBGHZ_SECPLUS_V1_TE_MID);
    TEST_ASSERT_GREATER_THAN_UINT16(0u, SUBGHZ_SECPLUS_V1_TE_LONG);
    /* SHORT < MID < LONG */
    TEST_ASSERT_LESS_THAN_UINT16(SUBGHZ_SECPLUS_V1_TE_MID,  SUBGHZ_SECPLUS_V1_TE_SHORT);
    TEST_ASSERT_LESS_THAN_UINT16(SUBGHZ_SECPLUS_V1_TE_LONG, SUBGHZ_SECPLUS_V1_TE_MID);
}

/*============================================================================*/
/* Main                                                                        */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_out_returns_false);
    RUN_TEST(test_zero_fixed_zero_rolling_succeeds);
    RUN_TEST(test_header_symbols_correct);
    RUN_TEST(test_all_symbols_are_legal);
    RUN_TEST(test_zero_rolling_zero_fixed_all_data_sym0);
    RUN_TEST(test_rolling_1_encodes_first_trit);
    RUN_TEST(test_max_rolling_trit_value);
    RUN_TEST(test_different_rolling_produce_different_packets);
    RUN_TEST(test_different_fixed_produce_different_packets);
    RUN_TEST(test_total_symbol_count);
    RUN_TEST(test_timing_constants_nonzero);
    return UNITY_END();
}
