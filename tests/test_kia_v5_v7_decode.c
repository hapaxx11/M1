/* See COPYING.txt for license details. */

/*
 * Host tests for the M1 ProtoPirate "Kia V5" and "Kia V7" decoders
 * (Sub_Ghz/protocols/m1_kia_v5_decode.c, m1_kia_v7_decode.c).
 *
 * Waveforms are synthesised test-side from the upstream ProtoPirate wire
 * formats (which the M1 TX encoders in Sub_Ghz/subghz_proto_pirate.c
 * mirror): Kia V5 = te 400/800, ~200-pair preamble, 4-entry sync, 67
 * Manchester bits (64-bit key + 3-bit CRC, bit 1 = low->high); Kia V7 =
 * te 250/500, 319-pair first frame / 15-pair tail frame, one extra sync
 * high, 64 Manchester bits (bit 1 = high->low on the on-air candidate),
 * trailing high + 2000 us low gap.
 *
 * The test target does not link the keystore stub, so the synthetic
 * KIA_KEY4 mixer-key symbols are defined here (same pattern as
 * tests/stubs/protopirate_keys_test_data.c).
 */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_blocks_math.h"
#include "subghz_manchester_encoder.h"
#include "subghz_protopirate_keys_builtin.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Synthetic keystore (no real key material). */
const uint64_t m1_kia_v5_key_builtin = 0xA1B2C3D4E5F60718ULL;
const bool m1_kia_v5_key_builtin_available = true;

uint8_t subghz_decode_kia_v5(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_kia_v7(uint16_t protocol_index, uint16_t pulse_count);

/* Pure helpers exported by the decoders */
uint16_t m1_kia_v5_mixer_decode(uint32_t encrypted, uint64_t mfkey);
void m1_kia_v5_decode_fields(uint64_t data, uint64_t mfkey,
                             bool mfkey_available, uint32_t *serial,
                             uint8_t *button, uint32_t *counter);
bool m1_kia_v7_decode_fields(uint64_t candidate, uint32_t *serial,
                             uint8_t *button, uint32_t *counter);

#define V5_TE_SHORT 400u
#define V5_TE_LONG  800u
#define V7_TE       250u

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
/* Kia V5 test-side builders (upstream mixer_encode / CRC / yek layout) */
/*--------------------------------------------------------------------*/

static uint8_t rev8(uint8_t x)
{
    x = (uint8_t)(((x & 0xF0u) >> 4) | ((x & 0x0Fu) << 4));
    x = (uint8_t)(((x & 0xCCu) >> 2) | ((x & 0x33u) << 2));
    x = (uint8_t)(((x & 0xAAu) >> 1) | ((x & 0x55u) << 1));
    return x;
}

/* Port of upstream kia_v5.c mixer_encode() (keystore = big-endian key). */
static uint32_t v5_mixer_encode(uint32_t serial, uint16_t counter,
                                uint8_t button, uint64_t mfkey)
{
    uint8_t keystore[8];
    for (uint8_t i = 0; i < 8u; i++)
        keystore[i] = (uint8_t)((mfkey >> ((7u - i) * 8u)) & 0xFFu);

    uint8_t state_a = (uint8_t)(((serial >> 8) & 0x0Fu) | ((button & 0x0Fu) << 4));
    uint8_t state_b = (uint8_t)(counter >> 8);
    uint8_t state_c = (uint8_t)(serial & 0xFFu);
    uint8_t state_d = (uint8_t)(counter & 0xFFu);

    int ks_idx = 0;
    for (uint8_t round = 0; round < 18u; round++)
    {
        uint8_t r = keystore[ks_idx];
        ks_idx = (ks_idx + 1) & 0x07;

        uint8_t running_d = state_d;
        for (uint8_t step = 0; step < 8u; step++)
        {
            uint8_t base;
            if ((state_a & 0x80u) == 0u)
                base = (state_a & 0x04u) == 0u ? 0x74u : 0x2Eu;
            else
                base = (state_a & 0x04u) == 0u ? 0x3Au : 0x5Cu;

            if (state_c & 0x10u)
                base = (uint8_t)(((base >> 4) & 0x0Fu) | ((base & 0x0Fu) << 4));
            if (state_b & 0x02u)
                base = (uint8_t)((base & 0x3Fu) << 2);

            uint8_t base_final = base;
            if (running_d & 0x02u)
                base_final = (uint8_t)((base & 0x7Fu) << 1);

            const bool carry_b = (state_b & 0x01u) != 0u;
            const bool carry_c = (state_c & 0x01u) != 0u;
            const bool carry_a = (state_a & 0x01u) != 0u;

            uint8_t new_d = (uint8_t)(running_d >> 1);
            if (carry_b) new_d |= 0x80u;

            running_d ^= state_c;

            state_b = (uint8_t)(state_b >> 1);
            if (carry_c) state_b |= 0x80u;

            state_c = (uint8_t)(state_c >> 1);
            if (carry_a) state_c |= 0x80u;

            const uint8_t feedback = (uint8_t)(((running_d ^ r) << 7) ^ base_final);
            state_a = (uint8_t)(state_a >> 1);
            if (feedback & 0x80u) state_a |= 0x80u;

            r = (uint8_t)(r >> 1);
            running_d = new_d;
        }
        state_d = running_d;
    }

    return ((uint32_t)state_a << 24) | ((uint32_t)state_c << 16) |
           ((uint32_t)state_b << 8) | (uint32_t)state_d;
}

static uint8_t v5_crc3(uint64_t data)
{
    uint8_t crc = 0;
    for (int i = 63; i >= 0; i--)
    {
        const uint8_t bit = (uint8_t)((data >> i) & 1u);
        const uint8_t shifted_out = (uint8_t)((crc >> 1) & 1u);
        crc = (uint8_t)(((crc & 1u) << 1) | bit);
        if (shifted_out) crc ^= 3u;
    }
    return (uint8_t)(crc & 3u);
}

/* Build the on-air 64-bit key from fields (encoder layout). */
static uint64_t v5_build_wire(uint32_t serial, uint8_t btn, uint16_t cnt,
                              uint8_t *crc3_out)
{
    const uint32_t mixer =
        v5_mixer_encode(serial & 0x0FFFFFFFu, cnt, btn, m1_kia_v5_key_builtin);
    const uint64_t yek = ((uint64_t)(btn & 0x0Fu) << 60) |
                         ((uint64_t)(serial & 0x0FFFFFFFu) << 32) | mixer;
    uint64_t data = 0;
    for (uint8_t i = 0; i < 8u; i++)
        data |= ((uint64_t)rev8((uint8_t)((yek >> (i * 8u)) & 0xFFu))
                 << ((7u - i) * 8u));
    *crc3_out = v5_crc3(data);
    return data;
}

static void emit_v5_manchester_bit(bool bit)
{
    /* Bit 1 = low->high, bit 0 = high->low. */
    if (bit)
    {
        emit_merge(false, V5_TE_SHORT);
        emit_merge(true, V5_TE_SHORT);
    }
    else
    {
        emit_merge(true, V5_TE_SHORT);
        emit_merge(false, V5_TE_SHORT);
    }
}

static void emit_v5_burst(uint64_t data, uint8_t crc3, bool trailing_gap)
{
    for (uint16_t p = 0; p < 200u; p++)
    {
        emit_merge(true, V5_TE_SHORT);
        emit_merge(false, V5_TE_SHORT);
    }

    /* Sync: low 400, high 800, low 400, high 400 (the first low merges
     * with the last preamble low into a single 800 us low). */
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_LONG);
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_SHORT);

    for (int b = 63; b >= 0; b--)
        emit_v5_manchester_bit(((data >> b) & 1u) != 0u);

    /* 3 CRC bits: 0, then crc bit 1, then crc bit 0. */
    emit_v5_manchester_bit(false);
    emit_v5_manchester_bit(((crc3 >> 1) & 1u) != 0u);
    emit_v5_manchester_bit((crc3 & 1u) != 0u);

    /* Trailer: low 400, high 400. */
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_SHORT);

    if (trailing_gap)
        emit_merge(false, 5000);
}

/*--------------------------------------------------------------------*/
/* Kia V7 test-side builders (upstream on-air candidate layout)         */
/*--------------------------------------------------------------------*/

static uint64_t v7_build_candidate(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    uint8_t b[8];
    b[0] = 0x4Cu;
    b[1] = (uint8_t)(cnt >> 8);
    b[2] = (uint8_t)(cnt & 0xFFu);
    b[3] = (uint8_t)((serial >> 20) & 0xFFu);
    b[4] = (uint8_t)((serial >> 12) & 0xFFu);
    b[5] = (uint8_t)((serial >> 4) & 0xFFu);
    b[6] = (uint8_t)(((serial & 0x0Fu) << 4) | (btn & 0x0Fu));
    b[7] = subghz_protocol_blocks_crc8(b, 7u, 0x7Fu, 0x4Cu);
    uint64_t key = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key = (key << 8) | b[i];
    return key;
}

static void emit_v7_frame(uint64_t candidate, uint16_t preamble_pairs,
                          bool tail_gap)
{
    for (uint16_t p = 0; p < preamble_pairs; p++)
    {
        emit_merge(true, V7_TE);
        emit_merge(false, V7_TE);
    }
    emit_merge(true, V7_TE); /* extra sync high */

    /* On-air candidate, MSB first; bit 1 = high->low, bit 0 = low->high. */
    for (int b = 63; b >= 0; b--)
    {
        const bool v = ((candidate >> b) & 1u) != 0u;
        emit_merge(v, V7_TE);
        emit_merge(!v, V7_TE);
    }

    emit_merge(true, V7_TE); /* trailing high */
    if (tail_gap)
        emit_merge(false, 2000);
}

/*--------------------------------------------------------------------*/
/* Kia V5                                                              */
/*--------------------------------------------------------------------*/

void test_kia_v5_full_burst_decodes(void)
{
    uint8_t crc3 = 0;
    const uint64_t wire = v5_build_wire(0x0123456u, 0x02u, 0x1234u, &crc3);

    emit_v5_burst(wire, crc3, true);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v5(KIA_V5, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(wire, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x0123456u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x1234u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x02u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(67, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(KIA_V5, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodeddelay);
}

void test_kia_v5_burst_without_trailing_gap_decodes_at_packet_end(void)
{
    uint8_t crc3 = 0;
    const uint64_t wire = v5_build_wire(0x1ABCDEFu, 0x01u, 0xBEEFu, &crc3);

    emit_v5_burst(wire, crc3, false);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v5(KIA_V5, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(wire, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x1ABCDEFu, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xBEEFu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x01u, subghz_decenc_ctl.n8_buttonid);
}

void test_kia_v5_counter_zero_boundary_decodes(void)
{
    uint8_t crc3 = 0;
    const uint64_t wire = v5_build_wire(0x0000042u, 0x03u, 0x0000u, &crc3);

    emit_v5_burst(wire, crc3, true);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v5(KIA_V5, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x42u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x0000u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x03u, subghz_decenc_ctl.n8_buttonid);
}

void test_kia_v5_mixer_round_trip(void)
{
    static const uint16_t counters[] = {0x0000u, 0x0001u, 0x1234u, 0xBEEFu, 0xFFFFu};
    for (uint8_t i = 0; i < sizeof(counters) / sizeof(counters[0]); i++)
    {
        const uint32_t enc =
            v5_mixer_encode(0x0123456u, counters[i], 0x02u, m1_kia_v5_key_builtin);
        TEST_ASSERT_EQUAL_UINT16(
            counters[i], m1_kia_v5_mixer_decode(enc, m1_kia_v5_key_builtin));
    }
    /* A different serial/button mix changes the mixer output. */
    const uint32_t enc =
        v5_mixer_encode(0x0A5A5A5u, 0x4711u, 0x0Bu, m1_kia_v5_key_builtin);
    TEST_ASSERT_EQUAL_UINT16(0x4711u,
                             m1_kia_v5_mixer_decode(enc, m1_kia_v5_key_builtin));
}

void test_kia_v5_short_preamble_rejected(void)
{
    uint8_t crc3 = 0;
    const uint64_t wire = v5_build_wire(0x0123456u, 0x02u, 0x1234u, &crc3);

    for (uint16_t p = 0; p < 30u; p++) /* below the >40 low-edge gate */
    {
        emit_merge(true, V5_TE_SHORT);
        emit_merge(false, V5_TE_SHORT);
    }
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_LONG);
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_SHORT);
    for (int b = 63; b >= 0; b--)
        emit_v5_manchester_bit(((wire >> b) & 1u) != 0u);
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_SHORT);
    emit_merge(false, 5000);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v5(KIA_V5, s_pulse_count));
}

void test_kia_v5_malformed_manchester_rejected(void)
{
    uint8_t crc3 = 0;
    const uint64_t wire = v5_build_wire(0x0123456u, 0x02u, 0x1234u, &crc3);
    (void)crc3;

    for (uint16_t p = 0; p < 200u; p++)
    {
        emit_merge(true, V5_TE_SHORT);
        emit_merge(false, V5_TE_SHORT);
    }
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_LONG);
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_SHORT);
    for (int b = 63; b >= 14; b--) /* 50 bits, below the 64-bit gate */
        emit_v5_manchester_bit(((wire >> b) & 1u) != 0u);
    emit_merge(true, 600); /* out of tolerance: not short, not long */
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_SHORT);
    emit_merge(false, V5_TE_SHORT);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v5(KIA_V5, s_pulse_count));
}

void test_kia_v5_truncated_frame_rejected(void)
{
    uint8_t crc3 = 0;
    const uint64_t wire = v5_build_wire(0x0123456u, 0x02u, 0x1234u, &crc3);
    (void)crc3;

    for (uint16_t p = 0; p < 200u; p++)
    {
        emit_merge(true, V5_TE_SHORT);
        emit_merge(false, V5_TE_SHORT);
    }
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_LONG);
    emit_merge(false, V5_TE_SHORT);
    emit_merge(true, V5_TE_SHORT);
    for (int b = 63; b >= 34; b--) /* 30 bits only */
        emit_v5_manchester_bit(((wire >> b) & 1u) != 0u);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v5(KIA_V5, s_pulse_count));
}

void test_kia_v5_no_mixer_key_commits_with_counter_zero(void)
{
    uint8_t crc3 = 0;
    const uint64_t wire = v5_build_wire(0x0123456u, 0x02u, 0x1234u, &crc3);

    /* Key unavailable: serial and button still recover, counter is 0. */
    uint32_t serial = 0xFFFFFFFFu;
    uint32_t counter = 0xFFFFFFFFu;
    uint8_t button = 0xFFu;
    m1_kia_v5_decode_fields(wire, 0, false, &serial, &button, &counter);
    TEST_ASSERT_EQUAL_UINT32(0x0123456u, serial);
    TEST_ASSERT_EQUAL_UINT8(0x02u, button);
    TEST_ASSERT_EQUAL_UINT32(0u, counter);

    /* Key available (same helper path the commit uses): counter recovers. */
    m1_kia_v5_decode_fields(wire, m1_kia_v5_key_builtin, true,
                            &serial, &button, &counter);
    TEST_ASSERT_EQUAL_UINT32(0x0123456u, serial);
    TEST_ASSERT_EQUAL_UINT8(0x02u, button);
    TEST_ASSERT_EQUAL_UINT32(0x1234u, counter);
}

/*--------------------------------------------------------------------*/
/* Kia V7                                                              */
/*--------------------------------------------------------------------*/

void test_kia_v7_first_frame_decodes(void)
{
    const uint64_t candidate = v7_build_candidate(0x0123456u, 0x02u, 0x1234u);

    emit_v7_frame(candidate, 319, true);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v7(KIA_V7, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(candidate, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x0123456u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x1234u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x02u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(64, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(KIA_V7, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodeddelay);
}

void test_kia_v7_tail_frame_decodes(void)
{
    const uint64_t candidate = v7_build_candidate(0x1ABCDEFu, 0x01u, 0xBEEFu);

    emit_v7_frame(candidate, 15, false);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v7(KIA_V7, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(candidate, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x1ABCDEFu, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xBEEFu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x01u, subghz_decenc_ctl.n8_buttonid);
}

void test_kia_v7_bad_header_rejected(void)
{
    /* Header 0x4D with a correctly recomputed CRC: only the 0x4C gate
     * can reject this frame. */
    uint8_t b[8];
    const uint64_t good = v7_build_candidate(0x0123456u, 0x02u, 0x1234u);
    for (uint8_t i = 0; i < 8u; i++)
        b[i] = (uint8_t)(good >> (56u - i * 8u));
    b[0] = 0x4Du;
    b[7] = subghz_protocol_blocks_crc8(b, 7u, 0x7Fu, 0x4Cu);
    uint64_t bad = 0;
    for (uint8_t i = 0; i < 8u; i++)
        bad = (bad << 8) | b[i];

    emit_v7_frame(bad, 319, true);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v7(KIA_V7, s_pulse_count));
}

void test_kia_v7_corrupted_crc_rejected(void)
{
    const uint64_t candidate = v7_build_candidate(0x0123456u, 0x02u, 0x1234u) ^ 0x01u;

    emit_v7_frame(candidate, 319, true);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v7(KIA_V7, s_pulse_count));
}

void test_kia_v7_truncated_frame_rejected(void)
{
    const uint64_t candidate = v7_build_candidate(0x0123456u, 0x02u, 0x1234u);

    for (uint16_t p = 0; p < 319u; p++)
    {
        emit_merge(true, V7_TE);
        emit_merge(false, V7_TE);
    }
    emit_merge(true, V7_TE);
    for (int b = 63; b >= 24; b--) /* 40 bits only */
    {
        const bool v = ((candidate >> b) & 1u) != 0u;
        emit_merge(v, V7_TE);
        emit_merge(!v, V7_TE);
    }

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v7(KIA_V7, s_pulse_count));
}

void test_kia_v7_pure_field_helper(void)
{
    const uint64_t candidate = v7_build_candidate(0x00556677u, 0x08u, 0x4321u);
    uint32_t serial = 0;
    uint32_t counter = 0;
    uint8_t button = 0;
    TEST_ASSERT_TRUE(m1_kia_v7_decode_fields(candidate, &serial, &button, &counter));
    TEST_ASSERT_EQUAL_UINT32(0x00556677u, serial);
    TEST_ASSERT_EQUAL_UINT8(0x08u, button);
    TEST_ASSERT_EQUAL_UINT32(0x4321u, counter);

    TEST_ASSERT_FALSE(
        m1_kia_v7_decode_fields(candidate ^ 0x01u, &serial, &button, &counter));
    TEST_ASSERT_FALSE(m1_kia_v7_decode_fields(candidate ^ (0xFFuLL << 56),
                                              &serial, &button, &counter));
}

/*--------------------------------------------------------------------*/
/* Kia V7 encoder-convention round trip                                */
/*--------------------------------------------------------------------*/

static void emit_v7_m1_symbol(ManchesterEncoderResult result)
{
    const bool level = (result == ManchesterEncoderResultLongHigh) ||
                       (result == ManchesterEncoderResultShortHigh);
    const bool is_long = (result == ManchesterEncoderResultLongLow) ||
                         (result == ManchesterEncoderResultLongHigh);
    emit_merge(level, is_long ? 500u : V7_TE);
}

void test_kia_v7_m1_encoder_convention_round_trip(void)
{
    /* The firmware encoder (encode_kia_v7) Manchester-encodes the bitwise
     * inverse of the saved candidate word with M1's encoder convention
     * (bit 1 = low->high half-symbols).  Feeding that exact waveform shape
     * to the decoder must recover the candidate — this pins the encoder's
     * polarity against the upstream-faithful decode side. */
    const uint64_t candidate = v7_build_candidate(0x00ABCDE1u, 0x02u, 0x1234u);
    const uint64_t wire = ~candidate;

    for (uint16_t p = 0; p < 319u; p++)
    {
        emit_merge(true, V7_TE);
        emit_merge(false, V7_TE);
    }
    emit_merge(true, V7_TE); /* extra sync high */

    ManchesterEncoderState state;
    manchester_encoder_reset(&state);
    for (int b = 63; b >= 0; b--)
    {
        const bool bit = ((wire >> b) & 1u) != 0u;
        ManchesterEncoderResult result;
        while (!manchester_encoder_advance(&state, bit, &result))
            emit_v7_m1_symbol(result);
        emit_v7_m1_symbol(result);
    }
    emit_v7_m1_symbol(manchester_encoder_finish(&state));

    emit_merge(true, V7_TE);  /* trailing high */
    emit_merge(false, 2000);  /* tail gap */
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v7(KIA_V7, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(candidate, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x00ABCDE1u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x1234u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x02u, subghz_decenc_ctl.n8_buttonid);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kia_v5_full_burst_decodes);
    RUN_TEST(test_kia_v5_burst_without_trailing_gap_decodes_at_packet_end);
    RUN_TEST(test_kia_v5_counter_zero_boundary_decodes);
    RUN_TEST(test_kia_v5_mixer_round_trip);
    RUN_TEST(test_kia_v5_short_preamble_rejected);
    RUN_TEST(test_kia_v5_malformed_manchester_rejected);
    RUN_TEST(test_kia_v5_truncated_frame_rejected);
    RUN_TEST(test_kia_v5_no_mixer_key_commits_with_counter_zero);
    RUN_TEST(test_kia_v7_first_frame_decodes);
    RUN_TEST(test_kia_v7_tail_frame_decodes);
    RUN_TEST(test_kia_v7_bad_header_rejected);
    RUN_TEST(test_kia_v7_corrupted_crc_rejected);
    RUN_TEST(test_kia_v7_truncated_frame_rejected);
    RUN_TEST(test_kia_v7_pure_field_helper);
    RUN_TEST(test_kia_v7_m1_encoder_convention_round_trip);
    return UNITY_END();
}
