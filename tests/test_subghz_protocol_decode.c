/* See COPYING.txt for license details. */

/*
 * test_subghz_protocol_decode.c
 *
 * Encoder/decoder roundtrip tests for Sub-GHz OOK PWM protocols.
 *
 * Each test encodes a known key into pulse pairs, copies the pairs into
 * subghz_decenc_ctl.pulse_times[], calls the protocol decoder, and asserts
 * that the decoded value matches the original key.
 *
 * Protocols tested:
 *   - Magellan  (32-bit, inverted polarity + custom frame header)  ← bugfix
 *   - CAME      (12-bit, standard 1:2 OOK PWM)
 *   - GateTX    (24-bit, standard 1:2 OOK PWM)
 *   - Nice FLO  (12-bit, standard 1:2 OOK PWM)
 *
 * The Magellan tests specifically cover the bug fixed in this PR:
 *   The original decoder delegated to subghz_decode_generic_pwm() which
 *   uses standard polarity (bit 1 = LONG HIGH + SHORT LOW), but Magellan
 *   uses inverted polarity (bit 1 = SHORT HIGH + LONG LOW).  The decoder
 *   also didn't skip Magellan's unique frame header.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"

/* ======================================================================= */
/* Forward declarations for decoder functions under test                   */
/* ======================================================================= */

uint8_t subghz_decode_magellan(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_came(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_gate_tx(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_nice_flo(uint16_t p, uint16_t pulsecount);

/* ======================================================================= */
/* Test helpers                                                             */
/* ======================================================================= */

void setUp(void)
{
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
}

void tearDown(void) {}

/*
 * Populate subghz_decenc_ctl.pulse_times[] with standard OOK PWM pairs.
 *
 * Standard polarity:
 *   bit 1: te_long  HIGH + te_short LOW
 *   bit 0: te_short HIGH + te_long  LOW
 *
 * Bits are emitted MSB first.  Returns total pulse count written.
 */
static uint16_t build_ook_pwm_pulses(uint64_t key, uint8_t bits,
                                      uint16_t te_short, uint16_t te_long)
{
    uint16_t idx = 0;
    for (int b = (int)bits - 1; b >= 0 && idx + 1 < PACKET_PULSE_COUNT_MAX; b--)
    {
        if (key & (1ULL << b))
        {
            subghz_decenc_ctl.pulse_times[idx++] = te_long;  /* bit 1 HIGH */
            subghz_decenc_ctl.pulse_times[idx++] = te_short; /* bit 1 LOW  */
        }
        else
        {
            subghz_decenc_ctl.pulse_times[idx++] = te_short; /* bit 0 HIGH */
            subghz_decenc_ctl.pulse_times[idx++] = te_long;  /* bit 0 LOW  */
        }
    }
    return idx;
}

/*
 * Populate subghz_decenc_ctl.pulse_times[] with Magellan data-only pulses
 * (inverted polarity, no frame header).
 *
 * Magellan inverted polarity:
 *   bit 1: 200µs HIGH + 400µs LOW
 *   bit 0: 400µs HIGH + 200µs LOW
 *
 * Bits are emitted MSB first.  Returns total pulse count written.
 */
static uint16_t build_magellan_data_pulses(uint32_t key)
{
    uint16_t idx = 0;
    for (int b = 31; b >= 0 && idx + 1 < PACKET_PULSE_COUNT_MAX; b--)
    {
        if (key & (1u << b))
        {
            subghz_decenc_ctl.pulse_times[idx++] = 200; /* bit 1: SHORT HIGH */
            subghz_decenc_ctl.pulse_times[idx++] = 400; /* bit 1: LONG  LOW  */
        }
        else
        {
            subghz_decenc_ctl.pulse_times[idx++] = 400; /* bit 0: LONG  HIGH */
            subghz_decenc_ctl.pulse_times[idx++] = 200; /* bit 0: SHORT LOW  */
        }
    }
    return idx;
}

/*
 * Populate subghz_decenc_ctl.pulse_times[] with a full Magellan frame:
 *   header burst (800µs HIGH + 200µs LOW)
 *   12 × toggle  (200µs HIGH + 200µs LOW each)
 *   header end   (200µs HIGH + 400µs LOW)
 *   start bit    (1200µs HIGH + 400µs LOW)   ← decoder landmark
 *   32 data bits (inverted polarity)
 *   stop bit     (200µs HIGH + 40000µs LOW)
 *
 * Returns total pulse count written.
 */
static uint16_t build_magellan_full_frame(uint32_t key)
{
    uint16_t idx = 0;

    /* Header burst: 800µs HIGH + 200µs LOW */
    subghz_decenc_ctl.pulse_times[idx++] = 800;
    subghz_decenc_ctl.pulse_times[idx++] = 200;

    /* 12 × header toggles: 200µs HIGH + 200µs LOW */
    for (int i = 0; i < 12; i++)
    {
        subghz_decenc_ctl.pulse_times[idx++] = 200;
        subghz_decenc_ctl.pulse_times[idx++] = 200;
    }

    /* Header end: 200µs HIGH + 400µs LOW */
    subghz_decenc_ctl.pulse_times[idx++] = 200;
    subghz_decenc_ctl.pulse_times[idx++] = 400;

    /* Start bit: 1200µs HIGH + 400µs LOW */
    subghz_decenc_ctl.pulse_times[idx++] = 1200;
    subghz_decenc_ctl.pulse_times[idx++] = 400;

    /* 32 data bits (inverted Magellan polarity) */
    for (int b = 31; b >= 0; b--)
    {
        if (key & (1u << b))
        {
            subghz_decenc_ctl.pulse_times[idx++] = 200; /* bit 1: SHORT HIGH */
            subghz_decenc_ctl.pulse_times[idx++] = 400; /* bit 1: LONG  LOW  */
        }
        else
        {
            subghz_decenc_ctl.pulse_times[idx++] = 400; /* bit 0: LONG  HIGH */
            subghz_decenc_ctl.pulse_times[idx++] = 200; /* bit 0: SHORT LOW  */
        }
    }

    /* Stop bit: 200µs HIGH + 40000µs LOW (te_long * 100, fits in uint16_t) */
    subghz_decenc_ctl.pulse_times[idx++] = 200;
    subghz_decenc_ctl.pulse_times[idx++] = 40000;

    return idx;
}

/* ======================================================================= */
/* Magellan tests — the polarity + header-skip bug fix                     */
/* ======================================================================= */

/*
 * Basic data-only buffer: 32 Magellan bits with inverted polarity.
 * No frame header.  The fixed decoder should recognise that no start bit
 * is present and fall back to decoding from index 0.
 */
void test_magellan_bare_data_all_ones(void)
{
    const uint32_t KEY = 0xFFFFFFFFu;
    uint16_t pc = build_magellan_data_pulses(KEY);
    uint8_t ret = subghz_decode_magellan(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT16(32, subghz_decenc_ctl.ndecodedbitlength);
}

void test_magellan_bare_data_all_zeros(void)
{
    const uint32_t KEY = 0x00000000u;
    uint16_t pc = build_magellan_data_pulses(KEY);
    uint8_t ret = subghz_decode_magellan(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

void test_magellan_bare_data_alternating(void)
{
    const uint32_t KEY = 0xAAAAAAAAu;
    uint16_t pc = build_magellan_data_pulses(KEY);
    uint8_t ret = subghz_decode_magellan(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

void test_magellan_bare_data_real_key(void)
{
    /* Synthetic key representative of a real Magellan alarm sensor */
    const uint32_t KEY = 0xDEADBEEFu;
    uint16_t pc = build_magellan_data_pulses(KEY);
    uint8_t ret = subghz_decode_magellan(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

/*
 * Full Magellan frame: tests that the decoder correctly locates the start
 * bit (1200µs HIGH) and skips the header before decoding data bits.
 */
void test_magellan_full_frame_all_ones(void)
{
    const uint32_t KEY = 0xFFFFFFFFu;
    uint16_t pc = build_magellan_full_frame(KEY);
    uint8_t ret = subghz_decode_magellan(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

void test_magellan_full_frame_real_key(void)
{
    const uint32_t KEY = 0x12345678u;
    uint16_t pc = build_magellan_full_frame(KEY);
    uint8_t ret = subghz_decode_magellan(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

void test_magellan_full_frame_from_database(void)
{
    /* Key from a Magellan .sub signal in the UberGuidoZ Flipper database */
    const uint32_t KEY = 0xA3C501F2u;
    uint16_t pc = build_magellan_full_frame(KEY);
    uint8_t ret = subghz_decode_magellan(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

/*
 * Verify that the buggy generic-PWM polarity would have produced
 * inverted results.  Under the old (broken) decoder, feeding a key of
 * 0xFFFFFFFF would have decoded as 0x00000000 because every bit was
 * inverted.  The new decoder produces the correct output.
 */
void test_magellan_polarity_not_inverted(void)
{
    /* If polarity were wrong, 0xFFFFFFFF → 0x00000000 (all bits flipped) */
    const uint32_t KEY = 0xFFFFFFFFu;
    uint16_t pc = build_magellan_data_pulses(KEY);
    subghz_decode_magellan(0, pc);
    TEST_ASSERT_NOT_EQUAL(0x00000000u,
                          (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

/* ======================================================================= */
/* CAME 12-bit roundtrip tests                                             */
/* Verified correct: bit 0 = SHORT HIGH + LONG LOW;                       */
/*                   bit 1 = LONG HIGH + SHORT LOW  (standard OOK PWM)    */
/* ======================================================================= */

void test_came_roundtrip_hampton_bay(void)
{
    /* Hampton Bay doorbell: Protocol=CAME, Bit=12, Key=0x06B8 (0x6B8 lower 12 bits) */
    const uint32_t KEY = 0x6B8u;
    const uint8_t  BITS = 12;
    const uint16_t TE_S = 320, TE_L = 640;

    /* Set up protocol entry at index 0 */
    subghz_protocols_list_ptr[0].te_short    = TE_S;
    subghz_protocols_list_ptr[0].te_long     = TE_L;
    subghz_protocols_list_ptr[0].te_tolerance = 20;
    subghz_protocols_list_ptr[0].data_bits   = BITS;

    uint16_t pc = build_ook_pwm_pulses(KEY, BITS, TE_S, TE_L);
    uint8_t ret = subghz_decode_came(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT16(BITS, subghz_decenc_ctl.ndecodedbitlength);
}

void test_came_roundtrip_all_ones(void)
{
    const uint32_t KEY = 0xFFFu; /* 12-bit all-ones */
    const uint8_t  BITS = 12;
    const uint16_t TE_S = 320, TE_L = 640;

    subghz_protocols_list_ptr[0].te_short    = TE_S;
    subghz_protocols_list_ptr[0].te_long     = TE_L;
    subghz_protocols_list_ptr[0].te_tolerance = 20;
    subghz_protocols_list_ptr[0].data_bits   = BITS;

    uint16_t pc = build_ook_pwm_pulses(KEY, BITS, TE_S, TE_L);
    uint8_t ret = subghz_decode_came(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

void test_came_roundtrip_all_zeros(void)
{
    const uint32_t KEY = 0x000u;
    const uint8_t  BITS = 12;
    const uint16_t TE_S = 320, TE_L = 640;

    subghz_protocols_list_ptr[0].te_short    = TE_S;
    subghz_protocols_list_ptr[0].te_long     = TE_L;
    subghz_protocols_list_ptr[0].te_tolerance = 20;
    subghz_protocols_list_ptr[0].data_bits   = BITS;

    uint16_t pc = build_ook_pwm_pulses(KEY, BITS, TE_S, TE_L);
    uint8_t ret = subghz_decode_came(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

/* ======================================================================= */
/* GateTX 24-bit roundtrip tests                                           */
/* Verified correct: same encoding as CAME but 24-bit with 1:2 timings    */
/* ======================================================================= */

void test_gate_tx_roundtrip_typical_key(void)
{
    /* Typical 24-bit gate remote signal */
    const uint32_t KEY = 0xA1B2C3u;
    const uint8_t  BITS = 24;
    const uint16_t TE_S = 350, TE_L = 700;

    subghz_protocols_list_ptr[0].te_short    = TE_S;
    subghz_protocols_list_ptr[0].te_long     = TE_L;
    subghz_protocols_list_ptr[0].te_tolerance = 20;
    subghz_protocols_list_ptr[0].data_bits   = BITS;

    uint16_t pc = build_ook_pwm_pulses(KEY, BITS, TE_S, TE_L);
    uint8_t ret = subghz_decode_gate_tx(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT16(BITS, subghz_decenc_ctl.ndecodedbitlength);
}

void test_gate_tx_roundtrip_all_ones(void)
{
    const uint32_t KEY = 0xFFFFFFu;
    const uint8_t  BITS = 24;
    const uint16_t TE_S = 350, TE_L = 700;

    subghz_protocols_list_ptr[0].te_short    = TE_S;
    subghz_protocols_list_ptr[0].te_long     = TE_L;
    subghz_protocols_list_ptr[0].te_tolerance = 20;
    subghz_protocols_list_ptr[0].data_bits   = BITS;

    uint16_t pc = build_ook_pwm_pulses(KEY, BITS, TE_S, TE_L);
    uint8_t ret = subghz_decode_gate_tx(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

/* ======================================================================= */
/* Nice FLO 12-bit roundtrip tests                                         */
/* Verified correct: same encoding convention, te_short~700µs te_long~1400*/
/* ======================================================================= */

void test_nice_flo_roundtrip_typical_key(void)
{
    const uint32_t KEY = 0x5A3u; /* 12-bit typical remote code */
    const uint8_t  BITS = 12;
    const uint16_t TE_S = 700, TE_L = 1400;

    subghz_protocols_list_ptr[0].te_short    = TE_S;
    subghz_protocols_list_ptr[0].te_long     = TE_L;
    subghz_protocols_list_ptr[0].te_tolerance = 20;
    subghz_protocols_list_ptr[0].data_bits   = BITS;

    uint16_t pc = build_ook_pwm_pulses(KEY, BITS, TE_S, TE_L);
    uint8_t ret = subghz_decode_nice_flo(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT16(BITS, subghz_decenc_ctl.ndecodedbitlength);
}

void test_nice_flo_roundtrip_all_ones(void)
{
    const uint32_t KEY = 0xFFFu;
    const uint8_t  BITS = 12;
    const uint16_t TE_S = 700, TE_L = 1400;

    subghz_protocols_list_ptr[0].te_short    = TE_S;
    subghz_protocols_list_ptr[0].te_long     = TE_L;
    subghz_protocols_list_ptr[0].te_tolerance = 20;
    subghz_protocols_list_ptr[0].data_bits   = BITS;

    uint16_t pc = build_ook_pwm_pulses(KEY, BITS, TE_S, TE_L);
    uint8_t ret = subghz_decode_nice_flo(0, pc);
    TEST_ASSERT_EQUAL_UINT8(0, ret);
    TEST_ASSERT_EQUAL_UINT32(KEY, (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

/* ======================================================================= */
/* main                                                                    */
/* ======================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* Magellan — polarity + header-skip fix */
    RUN_TEST(test_magellan_bare_data_all_ones);
    RUN_TEST(test_magellan_bare_data_all_zeros);
    RUN_TEST(test_magellan_bare_data_alternating);
    RUN_TEST(test_magellan_bare_data_real_key);
    RUN_TEST(test_magellan_full_frame_all_ones);
    RUN_TEST(test_magellan_full_frame_real_key);
    RUN_TEST(test_magellan_full_frame_from_database);
    RUN_TEST(test_magellan_polarity_not_inverted);

    /* CAME 12-bit — standard polarity verified correct */
    RUN_TEST(test_came_roundtrip_hampton_bay);
    RUN_TEST(test_came_roundtrip_all_ones);
    RUN_TEST(test_came_roundtrip_all_zeros);

    /* GateTX 24-bit — standard polarity verified correct */
    RUN_TEST(test_gate_tx_roundtrip_typical_key);
    RUN_TEST(test_gate_tx_roundtrip_all_ones);

    /* Nice FLO 12-bit — standard polarity verified correct */
    RUN_TEST(test_nice_flo_roundtrip_typical_key);
    RUN_TEST(test_nice_flo_roundtrip_all_ones);

    return UNITY_END();
}
