/* See COPYING.txt for license details. */

/*
 * Host tests for the M1 ProtoPirate "Renault V0" decoder
 * (Sub_Ghz/protocols/m1_renault_v0_decode.c).
 *
 * Waveforms are synthesised test-side from the upstream ProtoPirate
 * renault_v0.c wire format (1000 us sync high, 16 Manchester preamble
 * one-bits, 82 Manchester data bits = 64-bit key + 18-bit Key2, optional
 * 125 us trailing high, trailing low), covering single-burst and
 * per-burst-buffer decode, the M1 encoder waveform shape, the upstream
 * leading-ones preamble quirk, and the structural/checksum negative
 * gates.  The test-side key builder reimplements the upstream
 * renault_v0_build_key parity model and is anchored by known-answer
 * vectors generated independently from the upstream algorithm.
 */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

uint8_t subghz_decode_renault_v0(uint16_t protocol_index, uint16_t pulse_count);

/* Pure helpers exported by the decoder */
uint8_t m1_renault_v0_checksum(uint64_t data, uint32_t key2);
bool m1_renault_v0_button_valid(uint8_t button);
bool m1_renault_v0_frame_valid(uint64_t data, uint32_t key2);
bool m1_renault_v0_parse_frame(const uint8_t *bits, uint8_t bit_count,
                               uint64_t *out_data, uint32_t *out_key2);

#define TE_SHORT 125u
#define TE_LONG  250u
#define SYNC_HIGH_US 1000u

/* Upstream-shaped burst endings. */
#define UPSTREAM_FINAL_LOW_US 250u
/* M1 encoder (encode_renault_v0) burst endings: 1500 us for both the
 * inter-burst and the final low. */
#define M1_ENCODER_TRAILING_LOW_US 1500u

/* Known-answer vectors (upstream renault_v0_build_key output). */
#define KAT_SERIAL  0x100092u
#define KAT_BUTTON  0x06u
#define KAT_COUNTER 0x42u
#define KAT_DATA    0x10009206428A4DC3uLL
#define KAT_KEY2    0x15E18u
#define KAT_CHECKSUM 0x13u

#define KAT2_SERIAL  0x100092u
#define KAT2_BUTTON  0x0Au
#define KAT2_COUNTER 0xC3u
#define KAT2_DATA    0x1000920AC3AA8EC6uLL
#define KAT2_KEY2    0x2C22Au
#define KAT2_CHECKSUM 0x93u

#define KAT3_SERIAL  0x100091u
#define KAT3_BUTTON  0x05u
#define KAT3_COUNTER 0x00u
#define KAT3_DATA    0x1000910500EADDD9uLL
#define KAT3_KEY2    0x3F610u

/* Same (serial, counter) family with the button byte patched to the
 * invalid code 0x07 and one parity bit flipped to keep the checksum
 * fold intact: passes C1/C2, fails only the button gate. */
#define BADBTN_DATA 0x10009207428A4DC2uLL
#define BADBTN_KEY2 0x15E18u

/* Serial bit 23 set (key-word MSB = 1): a structurally valid frame that
 * upstream rejects because the leading data one is absorbed into the
 * preamble run. */
#define QUIRK_DATA 0x80000206428A4DC3uLL
#define QUIRK_KEY2 0x15E18u

static uint16_t s_pulse_count;

static void emit_merge(bool level, uint32_t duration)
{
    TEST_ASSERT_GREATER_THAN_UINT32(0, duration);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(UINT16_MAX, duration);
    if (s_pulse_count == 0)
    {
        if (!level)
        {
            /* M1 pulse arrays treat index 0 as HIGH. */
            subghz_decenc_ctl.pulse_times[s_pulse_count++] = 1;
        }
        else
        {
            subghz_decenc_ctl.pulse_times[s_pulse_count++] = (uint16_t)duration;
            return;
        }
    }

    const bool previous_level = ((s_pulse_count - 1u) & 1u) == 0u;
    if (level == previous_level)
        subghz_decenc_ctl.pulse_times[s_pulse_count - 1u] += (uint16_t)duration;
    else
    {
        TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
        subghz_decenc_ctl.pulse_times[s_pulse_count++] = (uint16_t)duration;
    }
}

void setUp(void)
{
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
    s_pulse_count = 0;
}

void tearDown(void) {}

/*--------------------------------------------------------------------*/
/* Test-side field -> key builder (independent reimplementation of     */
/* the upstream renault_v0_build_key parity model)                     */
/*--------------------------------------------------------------------*/

static const uint32_t k_matrix_low[42] = {
    0x00000001, 0x04000029, 0x0000001B, 0x00000000, 0x00000001, 0x05220124,
    0x00000001, 0x00088410, 0x60132D1D, 0x60170F87, 0x00000000, 0x002000A9,
    0x20863E01, 0x24BB3755, 0x640199A4, 0x24225C43, 0x607886F1, 0x6007A101,
    0x66672A10, 0x4651623F, 0x43380BBF, 0x20237F84, 0x4245755E, 0x60AAF581,
    0x22722DAD, 0x27C617F7, 0x46DE8F1B, 0x231DEC51, 0x03ACAA0B, 0x22D2BF81,
    0x626EF6AE, 0x40441F95, 0x00000001, 0x00000000, 0x20B9A590, 0x656C8E86,
    0x60129F96, 0x2368F667, 0x442A1A5C, 0x04C43242, 0x22198640, 0x23D6B958,
};
static const uint32_t k_matrix_high[42] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00001004, 0x00000000, 0x00000000,
    0x0000100C, 0x00000004, 0x00000004, 0x00001004, 0x0000100C, 0x0000000C,
    0x00000004, 0x00001008, 0x00001008, 0x00001000, 0x00001008, 0x00000004,
    0x0000000C, 0x00000000, 0x0000000C, 0x00000000, 0x00000008, 0x00000004,
    0x0000100C, 0x0000100C, 0x00000000, 0x00000000, 0x00000008, 0x00001008,
    0x0000000C, 0x00001000, 0x00000000, 0x0000100C, 0x00001000, 0x00001008,
};

static uint8_t parity32(uint32_t value)
{
    value ^= value >> 16;
    value ^= value >> 8;
    value ^= value >> 4;
    value ^= value >> 2;
    value ^= value >> 1;
    return (uint8_t)(value & 1u);
}

static void build_key(uint32_t serial, uint8_t button, uint8_t counter,
                      uint64_t *out_data, uint32_t *out_key2)
{
    uint8_t vars[7];
    uint8_t parity_bits[42];

    vars[0] = (button == 0x0Au) ? 1u : 0u;
    for (uint8_t bit = 0; bit < 6u; bit++)
        vars[bit + 1u] = (uint8_t)((counter >> bit) & 1u);

    uint32_t mask_low = 1u;
    uint32_t mask_high = 0u;
    uint8_t mask_bit = 1u;

    for (uint8_t i = 0; i < 7u; i++, mask_bit++)
    {
        if (vars[i])
        {
            if (mask_bit < 32u) mask_low |= 1uL << mask_bit;
            else                mask_high |= 1uL << (mask_bit - 32u);
        }
    }
    for (uint8_t i = 0; i < 6u; i++)
    {
        for (uint8_t j = (uint8_t)(i + 1u); j < 7u; j++, mask_bit++)
        {
            if (vars[i] & vars[j])
            {
                if (mask_bit < 32u) mask_low |= 1uL << mask_bit;
                else                mask_high |= 1uL << (mask_bit - 32u);
            }
        }
    }
    for (uint8_t i = 0; i < 6u; i++)
    {
        for (uint8_t j = (uint8_t)(i + 1u); j < 7u; j++)
        {
            for (uint8_t k = (uint8_t)(j + 1u); k < 7u; k++, mask_bit++)
            {
                if (vars[i] & vars[j] & vars[k])
                {
                    if (mask_bit < 32u) mask_low |= 1uL << mask_bit;
                    else                mask_high |= 1uL << (mask_bit - 32u);
                }
            }
        }
    }

    for (uint8_t row = 0; row < 42u; row++)
        parity_bits[row] = parity32((k_matrix_low[row] & mask_low) ^
                                    (k_matrix_high[row] & mask_high));

    if (counter & 0x40u) parity_bits[41] ^= 1u;
    if (counter >> 7u)   parity_bits[40] ^= 1u;

    uint32_t data_low = (uint32_t)counter << 24;
    const uint32_t data_high = (serial << 8) | (uint32_t)button;
    for (uint8_t i = 0; i < 24u; i++)
        if (parity_bits[i]) data_low |= 1uL << (23u - i);

    uint32_t key2 = 0;
    for (uint8_t i = 24u; i < 42u; i++)
        if (parity_bits[i]) key2 |= 1uL << (41u - i);

    *out_data = ((uint64_t)data_high << 32) | data_low;
    *out_key2 = key2;
}

/*--------------------------------------------------------------------*/
/* Waveform emitters (upstream encoder algorithm, MSB first)           */
/*--------------------------------------------------------------------*/

static uint8_t s_enc_state;
static uint32_t s_te_short = TE_SHORT;
static uint32_t s_te_long  = TE_LONG;

static void emit_encoded_bit(bool bit)
{
    if (s_enc_state == 1u)
    {
        if (bit)
        {
            emit_merge(false, s_te_short);
            emit_merge(true, s_te_short);
        }
        else
        {
            s_enc_state = 2u;
            emit_merge(false, s_te_long);
        }
    }
    else
    {
        if (bit)
        {
            s_enc_state = 1u;
            emit_merge(true, s_te_long);
        }
        else
        {
            emit_merge(true, s_te_short);
            emit_merge(false, s_te_short);
        }
    }
}

static bool wire_bit82(uint64_t data, uint32_t key2, uint8_t bit_index)
{
    if (bit_index <= 63u)
        return ((data >> (63u - bit_index)) & 1uLL) != 0uLL;
    return ((key2 >> (81u - bit_index)) & 1u) != 0u;
}

/* One full burst: sync high, preamble one-bits, 82 data bits, the
 * conditional trailing 125 us high, then the given trailing low. */
static void emit_burst_ex(uint64_t data, uint32_t key2, uint8_t preamble_bits,
                          uint8_t data_bits, uint32_t sync_high_us,
                          uint32_t trailing_low_us, bool with_trailing_low)
{
    emit_merge(true, sync_high_us);
    s_enc_state = 1u;
    for (uint8_t i = 0; i < preamble_bits; i++)
        emit_encoded_bit(true);
    for (uint8_t i = 0; i < data_bits; i++)
        emit_encoded_bit(wire_bit82(data, key2, i));
    if (s_enc_state == 2u)
        emit_merge(true, s_te_short);
    if (with_trailing_low)
        emit_merge(false, trailing_low_us);
}

static void emit_burst(uint64_t data, uint32_t key2, uint32_t trailing_low_us)
{
    emit_burst_ex(data, key2, 16u, 82u, SYNC_HIGH_US, trailing_low_us, true);
}

static void assert_committed_fields(uint64_t data, uint32_t key2,
                                    uint32_t serial, uint8_t button,
                                    uint8_t counter, uint16_t protocol_index)
{
    TEST_ASSERT_EQUAL_UINT64(data, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(serial, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(counter, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(button, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT64(key2, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT16(82, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodeddelay);
    TEST_ASSERT_EQUAL_UINT16(protocol_index, subghz_decenc_ctl.ndecodedprotocol);
}

/*--------------------------------------------------------------------*/
/* Positive decodes                                                    */
/*--------------------------------------------------------------------*/

void test_renault_v0_single_burst_decodes(void)
{
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);
    TEST_ASSERT_EQUAL_UINT64(KAT_DATA, data);
    TEST_ASSERT_EQUAL_UINT32(KAT_KEY2, key2);

    emit_burst(data, key2, UPSTREAM_FINAL_LOW_US);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
    assert_committed_fields(KAT_DATA, KAT_KEY2, KAT_SERIAL, KAT_BUTTON,
                            KAT_COUNTER, RENAULT_V0);
}

void test_renault_v0_three_bursts_decode_as_separate_buffers(void)
{
    /* Renault V0 sends 3 bursts 25 ms apart; M1 hard-splits there, so
     * each burst arrives as its own capture buffer and must decode. */
    uint64_t data;
    uint32_t key2;
    build_key(KAT2_SERIAL, KAT2_BUTTON, KAT2_COUNTER, &data, &key2);
    TEST_ASSERT_EQUAL_UINT64(KAT2_DATA, data);
    TEST_ASSERT_EQUAL_UINT32(KAT2_KEY2, key2);

    for (uint8_t burst = 0; burst < 3u; burst++)
    {
        memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
        s_pulse_count = 0;
        emit_burst(data, key2, UPSTREAM_FINAL_LOW_US);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
        TEST_ASSERT_EQUAL_UINT8(0,
                                subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
        assert_committed_fields(KAT2_DATA, KAT2_KEY2, KAT2_SERIAL, KAT2_BUTTON,
                                KAT2_COUNTER, RENAULT_V0);
    }
}

void test_renault_v0_key2_second_word_surfaced(void)
{
    uint64_t data;
    uint32_t key2;
    build_key(KAT3_SERIAL, KAT3_BUTTON, KAT3_COUNTER, &data, &key2);
    TEST_ASSERT_EQUAL_UINT64(KAT3_DATA, data);
    TEST_ASSERT_EQUAL_UINT32(KAT3_KEY2, key2);

    emit_burst(data, key2, UPSTREAM_FINAL_LOW_US);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
    /* The 18-bit Key2 rides n32_key2 exactly as upstream's "Key2:" field. */
    TEST_ASSERT_EQUAL_UINT64(KAT3_KEY2, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT8(KAT3_BUTTON, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT32(KAT3_SERIAL, subghz_decenc_ctl.n32_serialnumber);
}

/*--------------------------------------------------------------------*/
/* M1 encoder waveform cross-check (encode_renault_v0 output shape)    */
/*--------------------------------------------------------------------*/

void test_renault_v0_m1_encoder_waveform_gap_in_buffer_decodes(void)
{
    /* M1's encoder ends every burst with a 1500 us low (inter-burst and
     * final alike).  With the gap pulse included in the buffer it is
     * unclassifiable and triggers the commit mid-buffer. */
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    emit_burst(data, key2, M1_ENCODER_TRAILING_LOW_US);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
    assert_committed_fields(KAT_DATA, KAT_KEY2, KAT_SERIAL, KAT_BUTTON,
                            KAT_COUNTER, RENAULT_V0);
}

void test_renault_v0_m1_encoder_waveform_split_at_gap_decodes(void)
{
    /* Same encoder waveform, but the 1500 us gap is consumed by the M1
     * packet split and never reaches the decoder: packet-end commit. */
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    for (uint8_t burst = 0; burst < 3u; burst++)
    {
        memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
        s_pulse_count = 0;
        emit_burst_ex(data, key2, 16u, 82u, SYNC_HIGH_US, 0u, false);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
        TEST_ASSERT_EQUAL_UINT8(0,
                                subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
        assert_committed_fields(KAT_DATA, KAT_KEY2, KAT_SERIAL, KAT_BUTTON,
                                KAT_COUNTER, RENAULT_V0);
    }
}

/*--------------------------------------------------------------------*/
/* Timing tolerance                                                    */
/*--------------------------------------------------------------------*/

void test_renault_v0_jittered_timings_decode(void)
{
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    s_te_short = 130u;
    s_te_long  = 240u;
    emit_burst(data, key2, UPSTREAM_FINAL_LOW_US);
    s_te_short = TE_SHORT;
    s_te_long  = TE_LONG;

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
    assert_committed_fields(KAT_DATA, KAT_KEY2, KAT_SERIAL, KAT_BUTTON,
                            KAT_COUNTER, RENAULT_V0);
}

void test_renault_v0_boundary_timings_decode(void)
{
    /* Nastiest in-tolerance edges: 180 us still classifies SHORT
     * (250-180 > 69), 181 us resolves to LONG (ambiguous window). */
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    s_te_short = 180u;
    s_te_long  = 181u;
    emit_burst(data, key2, UPSTREAM_FINAL_LOW_US);
    s_te_short = TE_SHORT;
    s_te_long  = TE_LONG;

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
    assert_committed_fields(KAT_DATA, KAT_KEY2, KAT_SERIAL, KAT_BUTTON,
                            KAT_COUNTER, RENAULT_V0);
}

/*--------------------------------------------------------------------*/
/* Negative gates                                                      */
/*--------------------------------------------------------------------*/

void test_renault_v0_short_preamble_rejected(void)
{
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    /* 8 preamble one-bits: below the upstream > 9 leading-ones gate. */
    emit_burst_ex(data, key2, 8u, 82u, SYNC_HIGH_US, UPSTREAM_FINAL_LOW_US, true);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
}

void test_renault_v0_wrong_sync_rejected(void)
{
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    /* Sync high 700 us is below the 800 us minimum: never arms. */
    emit_burst_ex(data, key2, 16u, 82u, 700u, UPSTREAM_FINAL_LOW_US, true);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
}

void test_renault_v0_out_of_tolerance_pulse_rejected(void)
{
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    emit_merge(true, SYNC_HIGH_US);
    s_enc_state = 1u;
    for (uint8_t i = 0; i < 8u; i++)
        emit_encoded_bit(true);
    /* 320 us low: 70 us past te_long, outside the 69 us delta. */
    emit_merge(false, 320u);
    for (uint8_t i = 0; i < 8u; i++)
        emit_encoded_bit(true);
    for (uint8_t i = 0; i < 82u; i++)
        emit_encoded_bit(wire_bit82(data, key2, i));
    emit_merge(false, UPSTREAM_FINAL_LOW_US);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
}

void test_renault_v0_truncated_frame_rejected(void)
{
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    /* Only 40 of the 82 data bits: below the 82-bit minimum. */
    emit_burst_ex(data, key2, 16u, 40u, SYNC_HIGH_US, UPSTREAM_FINAL_LOW_US, true);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
}

void test_renault_v0_bad_checksum_rejected(void)
{
    uint64_t data;
    uint32_t key2;
    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);

    /* Flip one parity bit in the key word: C1 fold no longer matches. */
    emit_burst(data ^ 0x01uLL, key2, UPSTREAM_FINAL_LOW_US);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
}

void test_renault_v0_invalid_button_rejected(void)
{
    /* C1/C2 pass; only the button code (0x07) is invalid. */
    TEST_ASSERT_TRUE(m1_renault_v0_frame_valid(KAT_DATA, KAT_KEY2));
    TEST_ASSERT_EQUAL_UINT8(KAT_CHECKSUM,
                            m1_renault_v0_checksum(BADBTN_DATA, BADBTN_KEY2));
    TEST_ASSERT_FALSE(m1_renault_v0_frame_valid(BADBTN_DATA, BADBTN_KEY2));

    emit_burst(BADBTN_DATA, BADBTN_KEY2, UPSTREAM_FINAL_LOW_US);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
}

void test_renault_v0_serial_msb_absorbed_into_preamble_rejected(void)
{
    /* Upstream quirk, kept for fidelity: a key word with its MSB set is
     * type-13 valid on paper, but the leading data one extends the
     * preamble run and the remaining bit count drops to 81 (< 82). */
    TEST_ASSERT_TRUE(m1_renault_v0_frame_valid(QUIRK_DATA, QUIRK_KEY2));

    emit_burst(QUIRK_DATA, QUIRK_KEY2, UPSTREAM_FINAL_LOW_US);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_renault_v0(RENAULT_V0, s_pulse_count));
}

/*--------------------------------------------------------------------*/
/* Pure helpers                                                        */
/*--------------------------------------------------------------------*/

void test_renault_v0_pure_helpers_known_answers(void)
{
    uint64_t data;
    uint32_t key2;

    build_key(KAT_SERIAL, KAT_BUTTON, KAT_COUNTER, &data, &key2);
    TEST_ASSERT_EQUAL_UINT64(KAT_DATA, data);
    TEST_ASSERT_EQUAL_UINT32(KAT_KEY2, key2);
    build_key(KAT2_SERIAL, KAT2_BUTTON, KAT2_COUNTER, &data, &key2);
    TEST_ASSERT_EQUAL_UINT64(KAT2_DATA, data);
    TEST_ASSERT_EQUAL_UINT32(KAT2_KEY2, key2);
    build_key(KAT3_SERIAL, KAT3_BUTTON, KAT3_COUNTER, &data, &key2);
    TEST_ASSERT_EQUAL_UINT64(KAT3_DATA, data);
    TEST_ASSERT_EQUAL_UINT32(KAT3_KEY2, key2);

    TEST_ASSERT_EQUAL_UINT8(KAT_CHECKSUM, m1_renault_v0_checksum(KAT_DATA, KAT_KEY2));
    TEST_ASSERT_EQUAL_UINT8(KAT2_CHECKSUM, m1_renault_v0_checksum(KAT2_DATA, KAT2_KEY2));

    TEST_ASSERT_TRUE(m1_renault_v0_frame_valid(KAT_DATA, KAT_KEY2));
    TEST_ASSERT_TRUE(m1_renault_v0_frame_valid(KAT2_DATA, KAT2_KEY2));
    TEST_ASSERT_TRUE(m1_renault_v0_frame_valid(KAT3_DATA, KAT3_KEY2));
    TEST_ASSERT_FALSE(m1_renault_v0_frame_valid(KAT_DATA ^ 0x01uLL, KAT_KEY2));
    TEST_ASSERT_FALSE(m1_renault_v0_frame_valid(KAT_DATA, KAT_KEY2 ^ 0x01u));

    TEST_ASSERT_TRUE(m1_renault_v0_button_valid(0x05u));
    TEST_ASSERT_TRUE(m1_renault_v0_button_valid(0x06u));
    TEST_ASSERT_TRUE(m1_renault_v0_button_valid(0x0Au));
    TEST_ASSERT_FALSE(m1_renault_v0_button_valid(0x00u));
    TEST_ASSERT_FALSE(m1_renault_v0_button_valid(0x07u));
    TEST_ASSERT_FALSE(m1_renault_v0_button_valid(0x0Bu));
}

void test_renault_v0_parse_frame_known_answers(void)
{
    uint8_t bits[120];
    uint64_t data = 0;
    uint32_t key2 = 0;

    /* 16 preamble ones + 82 MSB-first wire bits of the KAT frame. */
    memset(bits, 1, 16);
    for (uint8_t i = 0; i < 82u; i++)
        bits[16u + i] = wire_bit82(KAT_DATA, KAT_KEY2, i) ? 1u : 0u;

    TEST_ASSERT_TRUE(m1_renault_v0_parse_frame(bits, 98u, &data, &key2));
    TEST_ASSERT_EQUAL_UINT64(KAT_DATA, data);
    TEST_ASSERT_EQUAL_UINT32(KAT_KEY2, key2);

    /* A longer preamble run decodes the same (upstream skips all of it). */
    memmove(bits + 20u, bits + 16u, 82u);
    memset(bits, 1, 20);
    data = 0;
    key2 = 0;
    TEST_ASSERT_TRUE(m1_renault_v0_parse_frame(bits, 102u, &data, &key2));
    TEST_ASSERT_EQUAL_UINT64(KAT_DATA, data);
    TEST_ASSERT_EQUAL_UINT32(KAT_KEY2, key2);

    /* Extra trailing bits after the 82 are ignored. */
    memset(bits, 1, 16);
    for (uint8_t i = 0; i < 82u; i++)
        bits[16u + i] = wire_bit82(KAT_DATA, KAT_KEY2, i) ? 1u : 0u;
    bits[98] = 0u;
    bits[99] = 1u;
    data = 0;
    key2 = 0;
    TEST_ASSERT_TRUE(m1_renault_v0_parse_frame(bits, 100u, &data, &key2));
    TEST_ASSERT_EQUAL_UINT64(KAT_DATA, data);
}

void test_renault_v0_parse_frame_negatives(void)
{
    uint8_t bits[120];
    uint64_t data = 0;
    uint32_t key2 = 0;

    memset(bits, 1, 16);
    for (uint8_t i = 0; i < 82u; i++)
        bits[16u + i] = wire_bit82(KAT_DATA, KAT_KEY2, i) ? 1u : 0u;

    /* Fewer than 82 bits overall. */
    TEST_ASSERT_FALSE(m1_renault_v0_parse_frame(bits, 81u, &data, &key2));

    /* Preamble run of 9 ones: not above the > 9 gate. */
    uint8_t short_pre[120];
    memset(short_pre, 1, 9);
    memcpy(short_pre + 9u, bits + 16u, 82u);
    TEST_ASSERT_FALSE(m1_renault_v0_parse_frame(short_pre, 91u, &data, &key2));

    /* 20-one preamble with only 80 following bits: < 82 remaining. */
    uint8_t long_pre[120];
    memset(long_pre, 1, 20);
    memcpy(long_pre + 20u, bits + 16u, 80u);
    TEST_ASSERT_FALSE(m1_renault_v0_parse_frame(long_pre, 100u, &data, &key2));

    /* Valid shape, corrupted content: flip a parity-field bit so the
     * preamble run is untouched and the checksum gate fires. */
    bits[16u + 60u] ^= 1u;
    TEST_ASSERT_FALSE(m1_renault_v0_parse_frame(bits, 98u, &data, &key2));
}

void test_renault_v0_capture_key2_len_is_three(void)
{
    /* The 18-bit Key2 tail word persists as 3 bytes via the generic
     * capture Key_2/Key2 persistence dispatch. */
    TEST_ASSERT_EQUAL_UINT8(3u, subghz_capture_key2_len(RENAULT_V0, 0x100092u));
    TEST_ASSERT_EQUAL_UINT8(0u, subghz_capture_key2_len(RENAULT_V1 - 1, 0x100092u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_renault_v0_single_burst_decodes);
    RUN_TEST(test_renault_v0_three_bursts_decode_as_separate_buffers);
    RUN_TEST(test_renault_v0_key2_second_word_surfaced);
    RUN_TEST(test_renault_v0_m1_encoder_waveform_gap_in_buffer_decodes);
    RUN_TEST(test_renault_v0_m1_encoder_waveform_split_at_gap_decodes);
    RUN_TEST(test_renault_v0_jittered_timings_decode);
    RUN_TEST(test_renault_v0_boundary_timings_decode);
    RUN_TEST(test_renault_v0_short_preamble_rejected);
    RUN_TEST(test_renault_v0_wrong_sync_rejected);
    RUN_TEST(test_renault_v0_out_of_tolerance_pulse_rejected);
    RUN_TEST(test_renault_v0_truncated_frame_rejected);
    RUN_TEST(test_renault_v0_bad_checksum_rejected);
    RUN_TEST(test_renault_v0_invalid_button_rejected);
    RUN_TEST(test_renault_v0_serial_msb_absorbed_into_preamble_rejected);
    RUN_TEST(test_renault_v0_pure_helpers_known_answers);
    RUN_TEST(test_renault_v0_parse_frame_known_answers);
    RUN_TEST(test_renault_v0_parse_frame_negatives);
    RUN_TEST(test_renault_v0_capture_key2_len_is_three);
    return UNITY_END();
}
