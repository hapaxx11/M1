/* See COPYING.txt for license details. */

/*
 * Host tests for the M1 ProtoPirate "Kia V1" and "Kia V2" decoders
 * (Sub_Ghz/protocols/m1_kia_v1_decode.c, m1_kia_v2_decode.c).
 *
 * Waveforms are synthesised test-side exactly the way the M1 TX encoders in
 * Sub_Ghz/subghz_proto_pirate.c (encode_kia_v1/encode_kia_v2) emit them:
 * low/high long preamble pairs, a low te_short sync, then the data bits MSB
 * first as (level = bit, te_short) half-symbol pairs, with adjacent
 * same-level halves merged into te_long pulses as they appear on air.
 */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

uint8_t subghz_decode_kia_v1(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_kia_v2(uint16_t protocol_index, uint16_t pulse_count);

/* Pure helpers exported by the decoders */
uint8_t m1_kia_v1_crc4(const uint8_t *bytes, uint8_t count, uint8_t offset);
void m1_kia_v1_parse_fields(uint64_t data, uint32_t *serial, uint8_t *button,
                            uint16_t *counter, uint8_t *crc, bool *crc_valid);
uint8_t m1_kia_v2_crc4(uint64_t data);
void m1_kia_v2_parse_fields(uint64_t data, uint32_t *serial, uint8_t *button,
                            uint16_t *counter, uint8_t *crc, bool *crc_valid);

#define V1_TE_SHORT 800u
#define V1_TE_LONG  1600u
#define V2_TE_SHORT 500u
#define V2_TE_LONG  1000u

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
/* Test-side field -> wire-word builders (independent reimplementation */
/* of the upstream layout)                                             */
/*--------------------------------------------------------------------*/

static uint8_t ref_crc4_fold(const uint8_t *bytes, uint8_t count)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < count; i++)
        crc = (uint8_t)(crc ^ ((bytes[i] & 0x0Fu) ^ (bytes[i] >> 4)));
    return (uint8_t)((crc + 1u) & 0x0Fu);
}

/* 56-bit wire word (seeded bit 56 not included). */
static uint64_t build_v1_word(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    const uint8_t bytes[7] = {
        (uint8_t)(serial >> 24), (uint8_t)(serial >> 16), (uint8_t)(serial >> 8),
        (uint8_t)serial,         btn,                     (uint8_t)(cnt & 0xFFu),
        (uint8_t)((cnt >> 8) & 0x0Fu),
    };
    const uint8_t crc = ref_crc4_fold(bytes, sizeof(bytes));
    return ((uint64_t)serial << 24) | ((uint64_t)btn << 16) |
           ((uint64_t)(cnt & 0xFFu) << 8) | (((uint64_t)(cnt >> 8) & 0x0Fu) << 4) |
           crc;
}

/* 53-bit wire word including the fixed bit 52. */
static uint64_t build_v2_word(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    const uint32_t packed = ((uint32_t)(cnt & 0xFFu) << 8) |
                            ((uint32_t)(btn & 0x0Fu) << 16) |
                            ((uint32_t)(cnt >> 4) & 0xF0u);
    uint64_t data = (1uLL << 52) |
                    (((uint64_t)serial << 20) & 0xFFFFFFFFF00000uLL) | packed;
    const uint64_t shifted = data >> 4;
    uint8_t bytes[6];
    for (uint8_t i = 0; i < 6u; i++)
        bytes[i] = (uint8_t)(shifted >> (i * 8u));
    return (data & ~0x0FuLL) | ref_crc4_fold(bytes, sizeof(bytes));
}

/*--------------------------------------------------------------------*/
/* Waveform emitters (same emission as the M1 encoders)                */
/*--------------------------------------------------------------------*/

static void emit_v1_burst_partial(uint64_t word56, uint8_t preamble_pairs,
                                  int8_t data_bits)
{
    for (uint8_t i = 0; i < preamble_pairs; i++)
    {
        emit_merge(false, V1_TE_LONG);
        emit_merge(true, V1_TE_LONG);
    }
    emit_merge(false, V1_TE_SHORT);
    for (int8_t i = 55; i >= 0 && data_bits-- > 0; i--)
    {
        const bool bit = (word56 >> i) & 1u;
        emit_merge(bit, V1_TE_SHORT);
        emit_merge(!bit, V1_TE_SHORT);
    }
}

static void emit_v1_burst(uint64_t word56)
{
    emit_v1_burst_partial(word56, 90u, 56);
}

static void emit_v2_burst_partial(uint64_t word53, uint16_t preamble_pairs,
                                  int8_t data_bits)
{
    for (uint16_t i = 0; i < preamble_pairs; i++)
    {
        emit_merge(false, V2_TE_LONG);
        emit_merge(true, V2_TE_LONG);
    }
    emit_merge(false, V2_TE_SHORT);
    for (int8_t i = 51; i >= 0 && data_bits-- > 0; i--)
    {
        const bool bit = (word53 >> i) & 1u;
        emit_merge(bit, V2_TE_SHORT);
        emit_merge(!bit, V2_TE_SHORT);
    }
}

static void emit_v2_burst(uint64_t word53)
{
    emit_v2_burst_partial(word53, 252u, 52);
}

static void assert_v1_committed(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    const uint64_t word = build_v1_word(serial, btn, cnt);
    TEST_ASSERT_EQUAL_UINT64((1uLL << 56) | word, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(serial, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(cnt, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(btn, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(57, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(KIA_V1, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodeddelay);
}

static void assert_v2_committed(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    const uint64_t word = build_v2_word(serial, btn, cnt);
    TEST_ASSERT_EQUAL_UINT64(word, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(serial, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(cnt, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(btn, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(53, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(KIA_V2, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodeddelay);
}

/*--------------------------------------------------------------------*/
/* Kia V1                                                              */
/*--------------------------------------------------------------------*/

void test_kia_v1_full_burst_decodes(void)
{
    /* Serial MSB (bit 55) set: the sync low stays unmerged. */
    emit_v1_burst(build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu));
    emit_merge(false, 25000); /* inter-burst gap ends the capture */
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    assert_v1_committed(0x9ABCDEF1u, 0x02u, 0x0ABCu);
}

void test_kia_v1_merged_sync_decodes(void)
{
    /* Bits 55..54 = 00: the sync merges with the first data half into a
     * 1600 us low and the first short pulse is HIGH. */
    emit_v1_burst(build_v1_word(0x3ABCDEF1u, 0x01u, 0x0123u));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    assert_v1_committed(0x3ABCDEF1u, 0x01u, 0x0123u);
}

void test_kia_v1_hidden_pairs_decode(void)
{
    /* Bits 55..52 = 0101: two leading "01" pairs merge into preamble-shaped
     * long/long pairs and are recovered by half-symbol accounting. */
    emit_v1_burst(build_v1_word(0x52E6B438u, 0x03u, 0x0000u));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    assert_v1_committed(0x52E6B438u, 0x03u, 0x0000u);
}

void test_kia_v1_all_zeros_and_ones_decode(void)
{
    emit_v1_burst(build_v1_word(0x00000000u, 0x00u, 0x0000u));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    assert_v1_committed(0x00000000u, 0x00u, 0x0000u);

    setUp();
    emit_v1_burst(build_v1_word(0xFFFFFFFFu, 0xFFu, 0x0FFFu));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    assert_v1_committed(0xFFFFFFFFu, 0xFFu, 0x0FFFu);
}

void test_kia_v1_leading_noise_skipped(void)
{
    emit_merge(true, 300);  /* garbage highs/lows before the burst */
    emit_merge(false, 300);
    emit_merge(true, 700);
    emit_merge(false, 1600); /* a lone long pair, below threshold */
    emit_merge(true, 1600);
    emit_v1_burst(build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    assert_v1_committed(0x9ABCDEF1u, 0x02u, 0x0ABCu);
}

void test_kia_v1_bad_crc_still_commits(void)
{
    /* Corrupt the CRC nibble: the serial has bit 55 set, so the frame needs
     * zero hidden-pair reconstruction and keeps upstream's display-only
     * CRC-4 policy — the decode must still commit with the exact fields. */
    const uint64_t word = build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu) ^ 0x01u;
    emit_v1_burst(word);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64((1uLL << 56) | word, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x9ABCDEF1u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x0ABCu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x02u, subghz_decenc_ctl.n8_buttonid);

    bool crc_valid = true;
    uint8_t crc = 0;
    m1_kia_v1_parse_fields(subghz_decenc_ctl.n64_decodedvalue, NULL, NULL,
                           NULL, &crc, &crc_valid);
    TEST_ASSERT_EQUAL_UINT8(0x7u, crc); /* recomputed over intact fields */
    TEST_ASSERT_FALSE(crc_valid);       /* received nibble is 0x6 */
}

void test_kia_v1_short_preamble_rejected(void)
{
    const uint64_t word = build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu);
    for (uint8_t i = 0; i < 50u; i++) /* below the >70 pair threshold */
    {
        emit_merge(false, V1_TE_LONG);
        emit_merge(true, V1_TE_LONG);
    }
    emit_merge(false, V1_TE_SHORT);
    for (int8_t i = 55; i >= 0; i--)
    {
        const bool bit = (word >> i) & 1u;
        emit_merge(bit, V1_TE_SHORT);
        emit_merge(!bit, V1_TE_SHORT);
    }
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
}

void test_kia_v1_random_short_pulses_rejected(void)
{
    for (uint16_t i = 0; i < 400u; i++)
        emit_merge((i & 1u) == 0u, V1_TE_SHORT);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
}

void test_kia_v1_tolerance_edge_rejected(void)
{
    /* Preamble pulses at te_long + te_delta: get_diff == delta is not < delta. */
    const uint64_t word = build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu);
    for (uint8_t i = 0; i < 90u; i++)
    {
        emit_merge(false, V1_TE_LONG + 200u);
        emit_merge(true, V1_TE_LONG + 200u);
    }
    emit_merge(false, V1_TE_SHORT);
    for (int8_t i = 55; i >= 0; i--)
    {
        const bool bit = (word >> i) & 1u;
        emit_merge(bit, V1_TE_SHORT);
        emit_merge(!bit, V1_TE_SHORT);
    }
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v1(KIA_V1, s_pulse_count));

    /* Valid preamble but data half-symbols at te_short + te_delta. */
    setUp();
    for (uint8_t i = 0; i < 90u; i++)
    {
        emit_merge(false, V1_TE_LONG);
        emit_merge(true, V1_TE_LONG);
    }
    emit_merge(false, V1_TE_SHORT);
    for (int8_t i = 55; i >= 0; i--)
    {
        const bool bit = (word >> i) & 1u;
        emit_merge(bit, V1_TE_SHORT + 200u);
        emit_merge(!bit, V1_TE_SHORT + 200u);
    }
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
}

void test_kia_v1_truncated_data_rejected(void)
{
    emit_v1_burst(build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu));
    /* Keep the preamble and roughly the first third of the data. */
    const uint16_t full = s_pulse_count;
    uint16_t cut = 0;
    for (uint16_t i = 0; i < full; i++)
    {
        /* find the sync: first 800 us low after the long run */
        if (i > 100 && subghz_decenc_ctl.pulse_times[i] == V1_TE_SHORT &&
            (i & 1u) == 1u)
        {
            cut = (uint16_t)(i + 30u);
            break;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT16(0, cut);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v1(KIA_V1, cut));
}

void test_kia_v1_three_burst_upload_pulse_count(void)
{
    /* Full upstream upload: 3 bursts separated by 25 ms gaps.  Must fit the
     * M1 capture buffer and decode from the first burst. */
    const uint64_t word = build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu);
    for (uint8_t burst = 0; burst < 3u; burst++)
    {
        if (burst)
            emit_merge(false, 25000);
        emit_v1_burst(word);
    }
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    assert_v1_committed(0x9ABCDEF1u, 0x02u, 0x0ABCu);
}

void test_kia_v1_truncated_phantom_prefix_rejected(void)
{
    /* Drop the last 2 bits (4 half-symbols).  Without the CRC gate the
     * decoder committed a phantom "01" prefix (0x166AF37BC40AF29); now the
     * reconstruction fails CRC-4 and the capture must be rejected. */
    emit_v1_burst_partial(build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu), 90u, 54);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
}

void test_kia_v1_leading_fragment_tolerated(void)
{
    /* Reopen scenario: an undecodable partial burst (30 preamble pairs +
     * 10 data bits of another frame), a 2 ms gap, then a complete burst. */
    emit_v1_burst_partial(build_v1_word(0x11111111u, 0x01u, 0x0111u), 30u, 10);
    emit_merge(false, 2000);
    emit_v1_burst(build_v1_word(0x9ABCDEF1u, 0x02u, 0x0ABCu));
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v1(KIA_V1, s_pulse_count));
    assert_v1_committed(0x9ABCDEF1u, 0x02u, 0x0ABCu);
}

/*--------------------------------------------------------------------*/
/* Kia V2                                                              */
/*--------------------------------------------------------------------*/

void test_kia_v2_single_burst_decodes(void)
{
    /* Serial MSB (bit 51) set: the sync low stays unmerged. */
    emit_v2_burst(build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu));
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x89ABCDEFu, 0x03u, 0x00EFu);
}

void test_kia_v2_counter_boundaries(void)
{
    emit_v2_burst(build_v2_word(0x12345678u, 0x0Au, 0x0000u));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x12345678u, 0x0Au, 0x0000u);

    setUp();
    emit_v2_burst(build_v2_word(0x12345678u, 0x0Au, 0x0FFFu));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x12345678u, 0x0Au, 0x0FFFu);
}

void test_kia_v2_merged_sync_decodes(void)
{
    /* Bits 51..50 = 00: the sync merges with the first data half and the
     * first short pulse is HIGH. */
    emit_v2_burst(build_v2_word(0x714040AEu, 0x02u, 0x0000u));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x714040AEu, 0x02u, 0x0000u);
}

void test_kia_v2_hidden_pair_decodes(void)
{
    /* Bits 51..50 = 01: one leading "01" pair hides in the long run. */
    emit_v2_burst(build_v2_word(0x532DBE56u, 0x01u, 0x0FFFu));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x532DBE56u, 0x01u, 0x0FFFu);
}

void test_kia_v2_all_zeros_and_ones_decode(void)
{
    emit_v2_burst(build_v2_word(0x00000000u, 0x00u, 0x0000u));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x00000000u, 0x00u, 0x0000u);

    setUp();
    emit_v2_burst(build_v2_word(0xFFFFFFFFu, 0x0Fu, 0x0FFFu));
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0xFFFFFFFFu, 0x0Fu, 0x0FFFu);
}

void test_kia_v2_next_burst_prefix_tolerated(void)
{
    /* The encoder emits two bursts back to back; the start of the second
     * burst's preamble must not shift the first burst's alignment. */
    const uint64_t word = build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu);
    emit_v2_burst(word);
    for (uint8_t i = 0; i < 80u; i++)
    {
        emit_merge(false, V2_TE_LONG);
        emit_merge(true, V2_TE_LONG);
    }
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x89ABCDEFu, 0x03u, 0x00EFu);
}

void test_kia_v2_bad_crc_still_commits(void)
{
    /* Corrupt the CRC nibble: the serial has bit 51 set, so the frame needs
     * zero hidden-pair reconstruction and keeps upstream's display-only
     * CRC-4 policy — the decode must still commit with the exact fields. */
    const uint64_t word = build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu) ^ 0x01u;
    emit_v2_burst(word);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(word, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x89ABCDEFu, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x00EFu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x03u, subghz_decenc_ctl.n8_buttonid);

    bool crc_valid = true;
    uint8_t crc = 0;
    m1_kia_v2_parse_fields(subghz_decenc_ctl.n64_decodedvalue, NULL, NULL,
                           NULL, &crc, &crc_valid);
    TEST_ASSERT_EQUAL_UINT8(0x3u, crc); /* recomputed over intact fields */
    TEST_ASSERT_FALSE(crc_valid);       /* received nibble is 0x2 */
}

void test_kia_v2_short_preamble_rejected(void)
{
    const uint64_t word = build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu);
    for (uint8_t i = 0; i < 30u; i++) /* 60 longs, below the >=100 threshold */
    {
        emit_merge(false, V2_TE_LONG);
        emit_merge(true, V2_TE_LONG);
    }
    emit_merge(false, V2_TE_SHORT);
    for (int8_t i = 51; i >= 0; i--)
    {
        const bool bit = (word >> i) & 1u;
        emit_merge(bit, V2_TE_SHORT);
        emit_merge(!bit, V2_TE_SHORT);
    }
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
}

void test_kia_v2_random_short_pulses_rejected(void)
{
    for (uint16_t i = 0; i < 400u; i++)
        emit_merge((i & 1u) == 0u, V2_TE_SHORT);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
}

void test_kia_v2_tolerance_edge_rejected(void)
{
    const uint64_t word = build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu);
    for (uint16_t i = 0; i < 252u; i++)
    {
        emit_merge(false, V2_TE_LONG + 150u);
        emit_merge(true, V2_TE_LONG + 150u);
    }
    emit_merge(false, V2_TE_SHORT);
    for (int8_t i = 51; i >= 0; i--)
    {
        const bool bit = (word >> i) & 1u;
        emit_merge(bit, V2_TE_SHORT);
        emit_merge(!bit, V2_TE_SHORT);
    }
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
}

void test_kia_v2_truncated_data_rejected(void)
{
    emit_v2_burst(build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu));
    const uint16_t full = s_pulse_count;
    uint16_t cut = 0;
    for (uint16_t i = 0; i < full; i++)
    {
        if (i > 400 && subghz_decenc_ctl.pulse_times[i] == V2_TE_SHORT &&
            (i & 1u) == 1u)
        {
            cut = (uint16_t)(i + 20u);
            break;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT16(0, cut);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v2(KIA_V2, cut));
}

void test_kia_v2_gapless_two_burst_upload(void)
{
    /* The encoder emits two bursts back to back with no inter-burst gap;
     * the whole upload must fit the capture buffer and decode from burst 1. */
    const uint64_t word = build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu);
    emit_v2_burst(word);
    emit_v2_burst(word);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x89ABCDEFu, 0x03u, 0x00EFu);
}

void test_kia_v2_truncated_phantom_prefix_rejected(void)
{
    /* Drop the last 2 bits (4 half-symbols).  Without the CRC gate the
     * decoder committed a phantom "01" prefix (0x1626AF37BCFBC0); now the
     * reconstruction fails CRC-4 and the capture must be rejected. */
    emit_v2_burst_partial(build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu), 252u, 50);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
}

void test_kia_v2_leading_fragment_tolerated(void)
{
    /* Reopen scenario: an undecodable partial burst (40 preamble pairs +
     * 10 data bits of another frame), a 3 ms gap, then a complete burst. */
    emit_v2_burst_partial(build_v2_word(0x11111111u, 0x01u, 0x0111u), 40u, 10);
    emit_merge(false, 3000);
    emit_v2_burst(build_v2_word(0x89ABCDEFu, 0x03u, 0x00EFu));
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v2(KIA_V2, s_pulse_count));
    assert_v2_committed(0x89ABCDEFu, 0x03u, 0x00EFu);
}

/*--------------------------------------------------------------------*/
/* Pure field parsers                                                  */
/*--------------------------------------------------------------------*/

void test_kia_v1_parse_fields_unit(void)
{
    uint32_t serial;
    uint16_t cnt;
    uint8_t btn, crc;
    bool valid;

    /* Known answer: committed word 0x19ABCDEF102BCA7 (crc 0x7 valid). */
    m1_kia_v1_parse_fields(0x19ABCDEF102BCA7uLL, &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT32(0x9ABCDEF1u, serial);
    TEST_ASSERT_EQUAL_UINT8(0x02u, btn);
    TEST_ASSERT_EQUAL_UINT16(0x0ABCu, cnt);
    TEST_ASSERT_EQUAL_UINT8(0x7u, crc);
    TEST_ASSERT_TRUE(valid);

    /* Counter split/reassembly boundaries. */
    m1_kia_v1_parse_fields((1uLL << 56) | build_v1_word(0x01020304u, 0x05u, 0x0000u),
                           &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT16(0x0000u, cnt);
    TEST_ASSERT_TRUE(valid);
    m1_kia_v1_parse_fields((1uLL << 56) | build_v1_word(0x01020304u, 0x05u, 0x0FFFu),
                           &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT16(0x0FFFu, cnt);
    TEST_ASSERT_TRUE(valid);
    m1_kia_v1_parse_fields((1uLL << 56) | build_v1_word(0x01020304u, 0x05u, 0x0A5Cu),
                           &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT16(0x0A5Cu, cnt);
    TEST_ASSERT_TRUE(valid);

    /* Bad CRC nibble is reported, not rejected. */
    m1_kia_v1_parse_fields(0x19ABCDEF102BCA6uLL, &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT8(0x7u, crc);
    TEST_ASSERT_FALSE(valid);

    /* crc4 fold known answer: bytes 9A BC DE F1 02 BC 0A fold+1 = 0x7. */
    const uint8_t bytes[7] = {0x9A, 0xBC, 0xDE, 0xF1, 0x02, 0xBC, 0x0A};
    TEST_ASSERT_EQUAL_UINT8(0x7u, m1_kia_v1_crc4(bytes, 7u, 1u));
}

void test_kia_v2_parse_fields_unit(void)
{
    uint32_t serial;
    uint16_t cnt;
    uint8_t btn, crc;
    bool valid;

    /* Known answer: word 0x189ABCDEF3EF03 (crc 0x3 valid). */
    m1_kia_v2_parse_fields(0x189ABCDEF3EF03uLL, &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT32(0x89ABCDEFu, serial);
    TEST_ASSERT_EQUAL_UINT8(0x03u, btn);
    TEST_ASSERT_EQUAL_UINT16(0x00EFu, cnt);
    TEST_ASSERT_EQUAL_UINT8(0x3u, crc);
    TEST_ASSERT_TRUE(valid);

    /* Counter reassembly (raw >> 4) | ((raw & 0xF) << 8) at boundaries. */
    m1_kia_v2_parse_fields(build_v2_word(0x01020304u, 0x05u, 0x0000u),
                           &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT16(0x0000u, cnt);
    TEST_ASSERT_TRUE(valid);
    m1_kia_v2_parse_fields(build_v2_word(0x01020304u, 0x05u, 0x0FFFu),
                           &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT16(0x0FFFu, cnt);
    TEST_ASSERT_TRUE(valid);
    m1_kia_v2_parse_fields(build_v2_word(0x01020304u, 0x05u, 0x05A5u),
                           &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT16(0x05A5u, cnt);
    TEST_ASSERT_TRUE(valid);

    /* Button nibble. */
    m1_kia_v2_parse_fields(build_v2_word(0x01020304u, 0x0Fu, 0x0000u),
                           &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT8(0x0Fu, btn);

    /* Bad CRC nibble is reported, not rejected. */
    m1_kia_v2_parse_fields(0x189ABCDEF3EF02uLL, &serial, &btn, &cnt, &crc, &valid);
    TEST_ASSERT_EQUAL_UINT8(0x3u, crc);
    TEST_ASSERT_FALSE(valid);

    TEST_ASSERT_EQUAL_UINT8(0x3u, m1_kia_v2_crc4(0x189ABCDEF3EF03uLL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kia_v1_full_burst_decodes);
    RUN_TEST(test_kia_v1_merged_sync_decodes);
    RUN_TEST(test_kia_v1_hidden_pairs_decode);
    RUN_TEST(test_kia_v1_all_zeros_and_ones_decode);
    RUN_TEST(test_kia_v1_leading_noise_skipped);
    RUN_TEST(test_kia_v1_bad_crc_still_commits);
    RUN_TEST(test_kia_v1_short_preamble_rejected);
    RUN_TEST(test_kia_v1_random_short_pulses_rejected);
    RUN_TEST(test_kia_v1_tolerance_edge_rejected);
    RUN_TEST(test_kia_v1_truncated_data_rejected);
    RUN_TEST(test_kia_v1_truncated_phantom_prefix_rejected);
    RUN_TEST(test_kia_v1_leading_fragment_tolerated);
    RUN_TEST(test_kia_v1_three_burst_upload_pulse_count);
    RUN_TEST(test_kia_v2_single_burst_decodes);
    RUN_TEST(test_kia_v2_counter_boundaries);
    RUN_TEST(test_kia_v2_merged_sync_decodes);
    RUN_TEST(test_kia_v2_hidden_pair_decodes);
    RUN_TEST(test_kia_v2_all_zeros_and_ones_decode);
    RUN_TEST(test_kia_v2_next_burst_prefix_tolerated);
    RUN_TEST(test_kia_v2_gapless_two_burst_upload);
    RUN_TEST(test_kia_v2_bad_crc_still_commits);
    RUN_TEST(test_kia_v2_short_preamble_rejected);
    RUN_TEST(test_kia_v2_random_short_pulses_rejected);
    RUN_TEST(test_kia_v2_tolerance_edge_rejected);
    RUN_TEST(test_kia_v2_truncated_data_rejected);
    RUN_TEST(test_kia_v2_truncated_phantom_prefix_rejected);
    RUN_TEST(test_kia_v2_leading_fragment_tolerated);
    RUN_TEST(test_kia_v1_parse_fields_unit);
    RUN_TEST(test_kia_v2_parse_fields_unit);
    return UNITY_END();
}
