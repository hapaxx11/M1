/* See COPYING.txt for license details. */

/*
 * Host tests for the M1 ProtoPirate "Fiat V0" and "Fiat V1" decoders
 * (Sub_Ghz/protocols/m1_fiat_v0_decode.c, m1_fiat_v1_decode.c).
 *
 * Waveforms are synthesised test-side exactly the way the upstream
 * ProtoPirate encoders (and the matching M1 encoders in
 * Sub_Ghz/subghz_proto_pirate.c) emit them:
 *  - Fiat V0: 150 short/short preamble pairs with the last low replaced by
 *    the 800 us sync gap, then 64 key bits + 6 end-byte bits of differential
 *    Manchester (te 200/400), then the trailer low merged with the 25 ms
 *    inter-burst gap as it appears on air.
 *  - Fiat V1: lead-in high (2033 us variant A / 850 us variant B), 104 wire
 *    bits as (level = bit, te) Manchester cell pairs with adjacent same-level
 *    cells merged into long pulses as on air, then the frame gap (3252 us A /
 *    1230 us B).
 *
 * All Fiat V1 expected values (raw frames, authenticators) are known-answer
 * vectors produced by the unmodified upstream ProtoPirate fiat_v1.c
 * construction run on the host.
 */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

uint8_t subghz_decode_fiat_v0(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_fiat_v1(uint16_t protocol_index, uint16_t pulse_count);

/* Pure helpers exported by the decoders */
void m1_fiat_v0_parse_fields(uint64_t data, uint32_t *hop, uint32_t *fix);
const char *m1_fiat_v0_button_name(uint8_t endbyte);
bool m1_fiat_v1_frame_valid(const uint8_t raw[13]);
void m1_fiat_v1_parse_frame(const uint8_t raw[13], uint32_t *uid,
                            uint8_t *button, uint32_t *control, uint32_t *hop,
                            uint8_t *tail_bits, uint8_t *frame_xor);
uint32_t m1_fiat_v1_bcm_generate_authenticator(uint32_t uid, uint8_t button,
                                               uint16_t control,
                                               const uint8_t key[6],
                                               uint32_t epoch);
bool m1_fiat_v1_find_known_key(uint32_t uid, uint8_t button, uint16_t control,
                               uint32_t hop, uint8_t key_out[6]);
uint64_t m1_fiat_v1_pack_key2(bool key_valid, const uint8_t key[6],
                              uint8_t te_variant, uint8_t tail_bits,
                              uint8_t frame_xor);

/*--------------------------------------------------------------------*/
/* Capture buffer emitter (index 0 = HIGH), same idiom as the Kia tests */
/*--------------------------------------------------------------------*/

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

/*====================================================================*/
/* Fiat V0                                                             */
/*====================================================================*/

#define V0_TE_SHORT 200u
#define V0_TE_LONG  400u
#define V0_GAP_US   800u

/* Differential-Manchester bit emitter, verbatim from the upstream/M1 encoder. */
static void fiat_v0_emit_bit(bool *prev, bool bit)
{
    if (!*prev && !bit)
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_SHORT);
    }
    else if (!*prev && bit)
    {
        emit_merge(true, V0_TE_LONG);
    }
    else if (*prev && !bit)
    {
        emit_merge(false, V0_TE_LONG);
    }
    else
    {
        emit_merge(false, V0_TE_SHORT);
        emit_merge(true, V0_TE_SHORT);
    }
    *prev = bit;
}

/* One full burst: 150 preamble pairs (last low -> 800 us gap), 64 data bits,
 * 6 end-byte bits, trailer.  The trailer low and the 25 ms inter-burst gap
 * merge on air; emit them separately and let emit_merge() join them. */
static void emit_fiat_v0_burst(uint64_t data, uint8_t endbyte)
{
    const uint8_t wire6 = (uint8_t)(endbyte >> 1);

    for (uint8_t i = 0; i < 149u; i++)
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_SHORT);
    }
    emit_merge(true, V0_TE_SHORT);
    emit_merge(false, V0_GAP_US);

    bool prev = ((data >> 63) & 1u) != 0u;
    if (prev)
        emit_merge(true, V0_TE_LONG);
    else
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_LONG);
    }
    for (int bit = 62; bit >= 0; bit--)
        fiat_v0_emit_bit(&prev, ((data >> bit) & 1u) != 0u);
    for (int bit = 5; bit >= 0; bit--)
        fiat_v0_emit_bit(&prev, ((wire6 >> bit) & 1u) != 0u);

    if (prev)
        emit_merge(false, V0_TE_SHORT);
    emit_merge(false, V0_TE_SHORT * 8u);
    emit_merge(false, 25000u); /* inter-burst gap (merges with the trailer) */
}

void test_fiat_v0_burst_bit63_set_decodes(void)
{
    /* hop top bit set: exercises the M1 first-bit seeding adaptation. */
    const uint32_t hop = 0x89ABCDEFu;
    const uint32_t fix = 0x01234567u;
    const uint64_t data = ((uint64_t)hop << 32) | fix;
    const uint8_t endbyte = 0x05u; /* wire 0x02 (even) -> decoded 0x07 ("Lock") */

    emit_fiat_v0_burst(data, endbyte);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v0(FIAT_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(data, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(fix, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(hop, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x07u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(71, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodeddelay);
    TEST_ASSERT_EQUAL_UINT16(FIAT_V0, subghz_decenc_ctl.ndecodedprotocol);
}

void test_fiat_v0_burst_bit63_clear_decodes(void)
{
    /* Reference check against the upstream continuous-feed codec, which
     * decodes this frame to the identical fields. */
    const uint32_t hop = 0x0FEDCB98u;
    const uint32_t fix = 0x76543210u;
    const uint64_t data = ((uint64_t)hop << 32) | fix;
    const uint8_t endbyte = 0x0Bu; /* wire 0x05 -> decoded 0x0B ("Unlock") */

    emit_fiat_v0_burst(data, endbyte);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v0(FIAT_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(data, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(fix, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(hop, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x0Bu, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(71, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(FIAT_V0, subghz_decenc_ctl.ndecodedprotocol);
}

void test_fiat_v0_three_bursts_each_decode(void)
{
    /* On M1 the 25 ms inter-burst gap hard-separates bursts: each burst
     * arrives as its own capture buffer and must decode on its own. */
    const uint32_t hop = 0x12345678u;
    const uint32_t fix = 0x9ABCDEF0u;
    const uint64_t data = ((uint64_t)hop << 32) | fix;

    for (uint8_t burst = 0; burst < 3u; burst++)
    {
        memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
        s_pulse_count = 0;
        emit_fiat_v0_burst(data, 0x08u); /* wire 0x04 (even) -> decoded 0x0B */
        TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
        TEST_ASSERT_EQUAL_UINT8(0,
                                subghz_decode_fiat_v0(FIAT_V0, s_pulse_count));
        TEST_ASSERT_EQUAL_UINT64(data, subghz_decenc_ctl.n64_decodedvalue);
        TEST_ASSERT_EQUAL_UINT32(fix, subghz_decenc_ctl.n32_serialnumber);
        TEST_ASSERT_EQUAL_UINT32(hop, subghz_decenc_ctl.n32_rollingcode);
        TEST_ASSERT_EQUAL_UINT8(0x0Bu, subghz_decenc_ctl.n8_buttonid);
    }
}

void test_fiat_v0_endbyte_matrix_matches_upstream(void)
{
    /* Known-answer end bytes produced by the unmodified upstream codec fed
     * the same (air-merged) waveforms: the merged trailer absorbs the final
     * low Manchester cell, so even wire values lose their trailing 0 and
     * upstream's following-preamble artifacts end the byte in ...11. */
    static const struct {
        uint8_t endbyte_in;
        uint8_t decoded;
    } vectors[] = {
        {0x02u, 0x03u}, {0x03u, 0x03u}, {0x04u, 0x07u}, {0x05u, 0x07u},
        {0x06u, 0x07u}, {0x07u, 0x07u}, {0x08u, 0x0Bu}, {0x09u, 0x0Bu},
        {0x0Au, 0x0Bu}, {0x0Bu, 0x0Bu}, {0x0Cu, 0x0Fu},
    };

    for (uint8_t i = 0; i < (uint8_t)(sizeof(vectors) / sizeof(vectors[0]));
         i++)
    {
        const uint32_t hop = 0x0FEDCB98u;
        const uint32_t fix = 0x01234567u;
        const uint64_t data = ((uint64_t)hop << 32) | fix;

        memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
        s_pulse_count = 0;
        emit_fiat_v0_burst(data, vectors[i].endbyte_in);
        TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

        TEST_ASSERT_EQUAL_UINT8(0,
                                subghz_decode_fiat_v0(FIAT_V0, s_pulse_count));
        TEST_ASSERT_EQUAL_UINT64(data, subghz_decenc_ctl.n64_decodedvalue);
        TEST_ASSERT_EQUAL_UINT32(fix, subghz_decenc_ctl.n32_serialnumber);
        TEST_ASSERT_EQUAL_UINT32(hop, subghz_decenc_ctl.n32_rollingcode);
        TEST_ASSERT_EQUAL_UINT8(vectors[i].decoded,
                                subghz_decenc_ctl.n8_buttonid);
    }
}

void test_fiat_v0_short_preamble_rejected(void)
{
    const uint64_t data = ((uint64_t)0x0FEDCB98u << 32) | 0x76543210u;

    /* 50 pairs = 99 counted edges, below the 150-edge gate. */
    for (uint8_t i = 0; i < 50u; i++)
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_SHORT);
    }
    emit_merge(false, V0_GAP_US);
    bool prev = true;
    emit_merge(true, V0_TE_LONG);
    for (int bit = 62; bit >= 0; bit--)
        fiat_v0_emit_bit(&prev, ((data >> bit) & 1u) != 0u);
    emit_merge(false, V0_TE_SHORT * 8u);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v0(FIAT_V0, s_pulse_count));
}

void test_fiat_v0_out_of_tolerance_data_rejected(void)
{
    /* 300/600 us matches neither 200 +/- 100 nor 400 +/- 100. */
    for (uint8_t i = 0; i < 149u; i++)
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_SHORT);
    }
    emit_merge(true, V0_TE_SHORT);
    emit_merge(false, V0_GAP_US);
    for (uint8_t i = 0; i < 70u; i++)
    {
        emit_merge(true, 300u);
        emit_merge(false, 300u);
    }
    emit_merge(false, 26600u);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v0(FIAT_V0, s_pulse_count));
}

void test_fiat_v0_truncated_before_key_snapshot_rejected(void)
{
    const uint64_t data = ((uint64_t)0x0FEDCB98u << 32) | 0x76543210u;

    for (uint8_t i = 0; i < 149u; i++)
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_SHORT);
    }
    emit_merge(true, V0_TE_SHORT);
    emit_merge(false, V0_GAP_US);
    bool prev = ((data >> 63) & 1u) != 0u;
    if (prev)
        emit_merge(true, V0_TE_LONG);
    else
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_LONG);
    }
    /* Only 39 further bits: 40 total, never reaches the 64-bit snapshot. */
    for (int bit = 62; bit >= 23; bit--)
        fiat_v0_emit_bit(&prev, ((data >> bit) & 1u) != 0u);
    emit_merge(false, 26600u);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v0(FIAT_V0, s_pulse_count));
}

void test_fiat_v0_truncated_endbyte_rejected(void)
{
    /* Full 64-bit key but only 4 of the 6 end-byte bits: the packet
     * relaxation must not commit a partial end byte. */
    const uint64_t data = ((uint64_t)0x0FEDCB98u << 32) | 0x76543210u;

    for (uint8_t i = 0; i < 149u; i++)
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_SHORT);
    }
    emit_merge(true, V0_TE_SHORT);
    emit_merge(false, V0_GAP_US);
    bool prev = ((data >> 63) & 1u) != 0u;
    if (prev)
        emit_merge(true, V0_TE_LONG);
    else
    {
        emit_merge(true, V0_TE_SHORT);
        emit_merge(false, V0_TE_LONG);
    }
    for (int bit = 62; bit >= 0; bit--)
        fiat_v0_emit_bit(&prev, ((data >> bit) & 1u) != 0u);
    for (int bit = 5; bit >= 2; bit--) /* only end-byte bits 5..2 */
        fiat_v0_emit_bit(&prev, ((0x05u >> bit) & 1u) != 0u);
    emit_merge(false, 26600u);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v0(FIAT_V0, s_pulse_count));
}

void test_fiat_v0_pure_helpers(void)
{
    uint32_t hop = 0, fix = 0;
    m1_fiat_v0_parse_fields(0x89ABCDEF01234567uLL, &hop, &fix);
    TEST_ASSERT_EQUAL_UINT32(0x89ABCDEFu, hop);
    TEST_ASSERT_EQUAL_UINT32(0x01234567u, fix);

    /* NULL outputs are tolerated. */
    m1_fiat_v0_parse_fields(0x89ABCDEF01234567uLL, NULL, NULL);

    TEST_ASSERT_EQUAL_STRING("Lock", m1_fiat_v0_button_name(0x04u));
    TEST_ASSERT_EQUAL_STRING("Lock", m1_fiat_v0_button_name(0x07u));
    TEST_ASSERT_EQUAL_STRING("Unlock", m1_fiat_v0_button_name(0x08u));
    TEST_ASSERT_EQUAL_STRING("Unlock", m1_fiat_v0_button_name(0x0Bu));
    TEST_ASSERT_EQUAL_STRING("??", m1_fiat_v0_button_name(0x03u));
    TEST_ASSERT_EQUAL_STRING("??", m1_fiat_v0_button_name(0x0Cu));
    TEST_ASSERT_EQUAL_STRING("??", m1_fiat_v0_button_name(0x00u));
}

/*====================================================================*/
/* Fiat V1                                                             */
/*====================================================================*/

#define V1_WIRE_BITS  104u
#define V1_WIRE_BYTES 13u

#define V1_VARIANT_A 0u
#define V1_VARIANT_B 1u

/* n32_key2 layout (must match m1_fiat_v1_decode.c). */
#define V1_KEY2_KEY_MASK     0xFFFFFFFFFFFFuLL
#define V1_KEY2_VARIANT_BIT  48u
#define V1_KEY2_TAIL_SHIFT   49u
#define V1_KEY2_VALID_BIT    51u
#define V1_KEY2_XOR_SHIFT    52u

/* Publicly-known BCM keys (from the upstream source). */
static const uint8_t V1_KEY0[6] = {0xB7u, 0x92u, 0x80u, 0xAEu, 0xCCu, 0x37u};
static const uint8_t V1_KEY5[6] = {0x4Du, 0x49u, 0x4Bu, 0x52u, 0x4Fu, 0x4Eu};
static const uint8_t V1_KEY8[6] = {0xF6u, 0x1Au, 0xEFu, 0x9Cu, 0xD0u, 0x1Bu};

/* Known-answer frames produced by the unmodified upstream construction:
 *  - KAV_A: uid 1A2B3C4D btn 04 ctrl 2A5 key0 tail 2 -> auth B02C8F5D
 *  - KAV_B: uid 0ABCDEF1 btn 01 ctrl 3FF key8 tail 3 -> auth 192B5502
 *  - KAV_UNKNOWN: uid 0BADF00D btn 02 ctrl 155 key 0123456789AB (not in the
 *    known-key table) tail 1 -> auth FDC782AA
 *  - KAV_ANYBTN: uid 1A2B3C4D, frame button 04, authenticator computed for
 *    button 01 with key0 (F40708D0) — matches via the any-button path. */
static const uint8_t KAV_A[V1_WIRE_BYTES] = {
    0x00, 0x01, 0x1A, 0x2B, 0x3C, 0x4D, 0x4A, 0x96, 0xC0, 0xB2, 0x3D, 0x76,
    0xA5,
};
static const uint8_t KAV_B[V1_WIRE_BYTES] = {
    0x00, 0x01, 0x0A, 0xBC, 0xDE, 0xF1, 0x1F, 0xFC, 0x64, 0xAD, 0x54, 0x0B,
    0xEC,
};
static const uint8_t KAV_UNKNOWN[V1_WIRE_BYTES] = {
    0x00, 0x01, 0x0B, 0xAD, 0xF0, 0x0D, 0x25, 0x57, 0xF7, 0x1E, 0x0A, 0xA9,
    0x63,
};
static const uint8_t KAV_ANYBTN[V1_WIRE_BYTES] = {
    0x00, 0x01, 0x1A, 0x2B, 0x3C, 0x4D, 0x4A, 0x97, 0xD0, 0x1C, 0x23, 0x42,
    0x30,
};

/* Wire emitter per the upstream encoder: lead-in high, then each wire bit as
 * a (bit, !bit) Manchester cell pair of te each, then the frame gap.
 * emit_merge() merges adjacent same-level cells into long pulses as on air. */
static void emit_fiat_v1_frame(const uint8_t raw[V1_WIRE_BYTES], uint8_t variant)
{
    const uint32_t te = (variant == V1_VARIANT_A) ? 250u : 100u;
    const uint32_t lead = (variant == V1_VARIANT_A) ? 2033u : 850u;
    const uint32_t gap = (variant == V1_VARIANT_A) ? 3252u : 1230u;

    emit_merge(true, lead);
    for (uint8_t i = 0; i < V1_WIRE_BITS; i++)
    {
        const bool bit = ((raw[i >> 3] >> (7u - (i & 7u))) & 1u) != 0u;
        emit_merge(bit, te);
        emit_merge(!bit, te);
    }
    emit_merge(false, gap);
}

void test_fiat_v1_variant_a_known_key_decodes(void)
{
    emit_fiat_v1_frame(KAV_A, V1_VARIANT_A);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x1A2B3C4Du, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT8(0x04u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT32(0x2A5u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT64(((uint64_t)0x1A2B3C4Du << 32) | 0xB02C8F5Du,
                             subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT16(102, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodeddelay);
    TEST_ASSERT_EQUAL_UINT16(FIAT_V1, subghz_decenc_ctl.ndecodedprotocol);

    const uint64_t key2 = subghz_decenc_ctl.n32_key2;
    TEST_ASSERT_EQUAL_UINT64(0xB79280AECC37uLL, key2 & V1_KEY2_KEY_MASK);
    TEST_ASSERT_EQUAL_UINT64(0, (key2 >> V1_KEY2_VARIANT_BIT) & 1u);
    TEST_ASSERT_EQUAL_UINT64(2, (key2 >> V1_KEY2_TAIL_SHIFT) & 3u);
    TEST_ASSERT_EQUAL_UINT64(1, (key2 >> V1_KEY2_VALID_BIT) & 1u);
    TEST_ASSERT_EQUAL_UINT64(0xA5u, (key2 >> V1_KEY2_XOR_SHIFT) & 0xFFu);
}

void test_fiat_v1_variant_b_known_key_decodes(void)
{
    emit_fiat_v1_frame(KAV_B, V1_VARIANT_B);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x0ABCDEF1u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT8(0x01u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT32(0x3FFu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT64(((uint64_t)0x0ABCDEF1u << 32) | 0x192B5502u,
                             subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT16(102, subghz_decenc_ctl.ndecodedbitlength);

    const uint64_t key2 = subghz_decenc_ctl.n32_key2;
    TEST_ASSERT_EQUAL_UINT64(0xF61AEF9CD01BuLL, key2 & V1_KEY2_KEY_MASK);
    TEST_ASSERT_EQUAL_UINT64(1, (key2 >> V1_KEY2_VARIANT_BIT) & 1u);
    TEST_ASSERT_EQUAL_UINT64(3, (key2 >> V1_KEY2_TAIL_SHIFT) & 3u);
    TEST_ASSERT_EQUAL_UINT64(1, (key2 >> V1_KEY2_VALID_BIT) & 1u);
    TEST_ASSERT_EQUAL_UINT64(0xECu, (key2 >> V1_KEY2_XOR_SHIFT) & 0xFFu);
}

void test_fiat_v1_structural_commit_without_known_key(void)
{
    /* Valid frame whose authenticator matches none of the nine known keys:
     * upstream commits structurally, the key fields stay clear. */
    emit_fiat_v1_frame(KAV_UNKNOWN, V1_VARIANT_A);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x0BADF00Du, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT8(0x02u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT32(0x155u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT64(((uint64_t)0x0BADF00Du << 32) | 0xFDC782AAu,
                             subghz_decenc_ctl.n64_decodedvalue);

    const uint64_t key2 = subghz_decenc_ctl.n32_key2;
    TEST_ASSERT_EQUAL_UINT64(0, key2 & V1_KEY2_KEY_MASK);
    TEST_ASSERT_EQUAL_UINT64(0, (key2 >> V1_KEY2_VALID_BIT) & 1u);
    TEST_ASSERT_EQUAL_UINT64(0, (key2 >> V1_KEY2_VARIANT_BIT) & 1u);
    TEST_ASSERT_EQUAL_UINT64(1, (key2 >> V1_KEY2_TAIL_SHIFT) & 3u);
    TEST_ASSERT_EQUAL_UINT64(0x63u, (key2 >> V1_KEY2_XOR_SHIFT) & 0xFFu);
}

void test_fiat_v1_any_button_key_match(void)
{
    /* The frame's button nibble is 0x4 but the authenticator was generated
     * for button 0x1 with known key0 — upstream's any-button path still
     * identifies the key. */
    emit_fiat_v1_frame(KAV_ANYBTN, V1_VARIANT_A);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x1A2B3C4Du, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT8(0x04u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT64(((uint64_t)0x1A2B3C4Du << 32) | 0xF40708D0u,
                             subghz_decenc_ctl.n64_decodedvalue);

    const uint64_t key2 = subghz_decenc_ctl.n32_key2;
    TEST_ASSERT_EQUAL_UINT64(0xB79280AECC37uLL, key2 & V1_KEY2_KEY_MASK);
    TEST_ASSERT_EQUAL_UINT64(1, (key2 >> V1_KEY2_VALID_BIT) & 1u);
}

void test_fiat_v1_leading_garbage_realigns(void)
{
    /* Noise before the lead-in must not poison the cell windows. */
    emit_merge(true, 7000u);
    emit_merge(false, 700u);
    emit_merge(true, 250u);
    emit_merge(false, 900u);
    emit_fiat_v1_frame(KAV_A, V1_VARIANT_A);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x1A2B3C4Du, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xB02C8F5Du,
                             (uint32_t)subghz_decenc_ctl.n64_decodedvalue);
}

void test_fiat_v1_bad_xor_rejected(void)
{
    uint8_t raw[V1_WIRE_BYTES];
    memcpy(raw, KAV_A, sizeof(raw));
    raw[12] ^= 0xFFu;
    emit_fiat_v1_frame(raw, V1_VARIANT_A);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
}

void test_fiat_v1_bad_button_rejected(void)
{
    /* Button nibble 0x3 is not in {1,2,4,8}; fix the XOR so the checksum
     * gate alone cannot reject the frame. */
    uint8_t raw[V1_WIRE_BYTES];
    memcpy(raw, KAV_A, sizeof(raw));
    const uint8_t old6 = raw[6];
    raw[6] = (uint8_t)((0x3u << 4) | (raw[6] & 0x0Fu));
    raw[12] ^= (uint8_t)(old6 ^ raw[6]);
    TEST_ASSERT_FALSE(m1_fiat_v1_frame_valid(raw));
    emit_fiat_v1_frame(raw, V1_VARIANT_A);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
}

void test_fiat_v1_bad_marker_rejected(void)
{
    uint8_t raw[V1_WIRE_BYTES];
    memcpy(raw, KAV_A, sizeof(raw));
    raw[1] = 0x02u;
    raw[12] ^= (uint8_t)(0x01u ^ 0x02u);
    emit_fiat_v1_frame(raw, V1_VARIANT_A);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
}

void test_fiat_v1_truncated_rejected(void)
{
    /* Only 96 of 104 wire bits: the 208-cell window never fills. */
    emit_merge(true, 2033u);
    for (uint8_t i = 0; i < 96u; i++)
    {
        const bool bit = ((KAV_A[i >> 3] >> (7u - (i & 7u))) & 1u) != 0u;
        emit_merge(bit, 250u);
        emit_merge(!bit, 250u);
    }
    emit_merge(false, 3252u);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
}

void test_fiat_v1_out_of_tolerance_rejected(void)
{
    /* te 400 us matches neither variant's short or long. */
    emit_merge(true, 2033u);
    for (uint8_t i = 0; i < V1_WIRE_BITS; i++)
    {
        const bool bit = ((KAV_A[i >> 3] >> (7u - (i & 7u))) & 1u) != 0u;
        emit_merge(bit, 400u);
        emit_merge(!bit, 400u);
    }
    emit_merge(false, 3252u);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_fiat_v1(FIAT_V1, s_pulse_count));
}

void test_fiat_v1_authenticator_known_answers(void)
{
    /* Reference values from the unmodified upstream construction. */
    TEST_ASSERT_EQUAL_UINT32(
        0xB02C8F5Du,
        m1_fiat_v1_bcm_generate_authenticator(0x1A2B3C4Du, 0x4u, 0x2A5u,
                                              V1_KEY0, 0u));
    TEST_ASSERT_EQUAL_UINT32(
        0x6E6DBFC8u,
        m1_fiat_v1_bcm_generate_authenticator(0x00C0FFEEu, 0x8u, 0x123u,
                                              V1_KEY5, 0u));
    TEST_ASSERT_EQUAL_UINT32(
        0x192B5502u,
        m1_fiat_v1_bcm_generate_authenticator(0x0ABCDEF1u, 0x1u, 0x3FFu,
                                              V1_KEY8, 0u));
    /* Unknown key and any-button reference vectors. */
    static const uint8_t unk[6] = {0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xABu};
    TEST_ASSERT_EQUAL_UINT32(
        0xFDC782AAu,
        m1_fiat_v1_bcm_generate_authenticator(0x0BADF00Du, 0x2u, 0x155u, unk,
                                              0u));
    TEST_ASSERT_EQUAL_UINT32(
        0xF40708D0u,
        m1_fiat_v1_bcm_generate_authenticator(0x1A2B3C4Du, 0x1u, 0x2A5u,
                                              V1_KEY0, 0u));
}

void test_fiat_v1_frame_valid_and_parse(void)
{
    uint32_t uid = 0, control = 0, hop = 0;
    uint8_t button = 0, tail = 0, xorb = 0;

    TEST_ASSERT_TRUE(m1_fiat_v1_frame_valid(KAV_A));
    m1_fiat_v1_parse_frame(KAV_A, &uid, &button, &control, &hop, &tail, &xorb);
    TEST_ASSERT_EQUAL_UINT32(0x1A2B3C4Du, uid);
    TEST_ASSERT_EQUAL_UINT8(0x04u, button);
    TEST_ASSERT_EQUAL_UINT32(0x2A5u, control);
    TEST_ASSERT_EQUAL_UINT32(0xB02C8F5Du, hop);
    TEST_ASSERT_EQUAL_UINT8(2u, tail);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, xorb);

    TEST_ASSERT_FALSE(m1_fiat_v1_frame_valid(NULL));

    /* UID 0 and UID 0xFFFFFFFF are rejected. */
    uint8_t raw[V1_WIRE_BYTES] = {0};
    raw[1] = 0x01u;
    raw[6] = 0x40u;
    raw[12] = (uint8_t)(0x01u ^ 0x01u ^ 0x40u);
    TEST_ASSERT_FALSE(m1_fiat_v1_frame_valid(raw));
    memset(&raw[2], 0xFF, 4);
    raw[12] = (uint8_t)(0x01u ^ 0x01u ^ 0xFFu ^ 0xFFu ^ 0xFFu ^ 0xFFu ^ 0x40u);
    TEST_ASSERT_FALSE(m1_fiat_v1_frame_valid(raw));
}

void test_fiat_v1_find_known_key(void)
{
    uint8_t key[6] = {0};

    TEST_ASSERT_TRUE(
        m1_fiat_v1_find_known_key(0x1A2B3C4Du, 0x4u, 0x2A5u, 0xB02C8F5Du, key));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(V1_KEY0, key, 6);

    /* Any-button path: authenticator computed for button 0x1. */
    memset(key, 0, sizeof(key));
    TEST_ASSERT_TRUE(
        m1_fiat_v1_find_known_key(0x1A2B3C4Du, 0x4u, 0x2A5u, 0xF40708D0u, key));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(V1_KEY0, key, 6);

    /* Unknown authenticator: no match. */
    TEST_ASSERT_FALSE(
        m1_fiat_v1_find_known_key(0x0BADF00Du, 0x2u, 0x155u, 0xFDC782AAu, key));

    /* NULL key_out is tolerated. */
    TEST_ASSERT_TRUE(
        m1_fiat_v1_find_known_key(0x1A2B3C4Du, 0x4u, 0x2A5u, 0xB02C8F5Du, NULL));
}

void test_fiat_v1_pack_key2_known_answer(void)
{
    const uint64_t packed =
        m1_fiat_v1_pack_key2(true, V1_KEY0, V1_VARIANT_B, 3u, 0xECu);
    TEST_ASSERT_EQUAL_UINT64(0x0ECFB79280AECC37uLL, packed);

    /* Invalid key: key bits and the valid flag stay clear. */
    const uint64_t structural =
        m1_fiat_v1_pack_key2(false, NULL, V1_VARIANT_A, 1u, 0x63u);
    TEST_ASSERT_EQUAL_UINT64(0, structural & V1_KEY2_KEY_MASK);
    TEST_ASSERT_EQUAL_UINT64(0, (structural >> V1_KEY2_VALID_BIT) & 1u);
    TEST_ASSERT_EQUAL_UINT64(0, (structural >> V1_KEY2_VARIANT_BIT) & 1u);
    TEST_ASSERT_EQUAL_UINT64(1, (structural >> V1_KEY2_TAIL_SHIFT) & 3u);
    TEST_ASSERT_EQUAL_UINT64(0x63u, (structural >> V1_KEY2_XOR_SHIFT) & 0xFFu);
    TEST_ASSERT_EQUAL_UINT64(0, structural >> 60);
}

void test_fiat_capture_key2_len_dispatch(void)
{
    /* Fiat V1 persists the packed auxiliary word as an 8-byte Key_2;
     * Fiat V0 carries no auxiliary word. */
    TEST_ASSERT_EQUAL_UINT8(8u, subghz_capture_key2_len(FIAT_V1, 0x11223344u));
    TEST_ASSERT_EQUAL_UINT8(0u, subghz_capture_key2_len(FIAT_V0, 0x11223344u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fiat_v0_burst_bit63_set_decodes);
    RUN_TEST(test_fiat_v0_burst_bit63_clear_decodes);
    RUN_TEST(test_fiat_v0_three_bursts_each_decode);
    RUN_TEST(test_fiat_v0_endbyte_matrix_matches_upstream);
    RUN_TEST(test_fiat_v0_short_preamble_rejected);
    RUN_TEST(test_fiat_v0_out_of_tolerance_data_rejected);
    RUN_TEST(test_fiat_v0_truncated_before_key_snapshot_rejected);
    RUN_TEST(test_fiat_v0_truncated_endbyte_rejected);
    RUN_TEST(test_fiat_v0_pure_helpers);
    RUN_TEST(test_fiat_v1_variant_a_known_key_decodes);
    RUN_TEST(test_fiat_v1_variant_b_known_key_decodes);
    RUN_TEST(test_fiat_v1_structural_commit_without_known_key);
    RUN_TEST(test_fiat_v1_any_button_key_match);
    RUN_TEST(test_fiat_v1_leading_garbage_realigns);
    RUN_TEST(test_fiat_v1_bad_xor_rejected);
    RUN_TEST(test_fiat_v1_bad_button_rejected);
    RUN_TEST(test_fiat_v1_bad_marker_rejected);
    RUN_TEST(test_fiat_v1_truncated_rejected);
    RUN_TEST(test_fiat_v1_out_of_tolerance_rejected);
    RUN_TEST(test_fiat_v1_authenticator_known_answers);
    RUN_TEST(test_fiat_v1_frame_valid_and_parse);
    RUN_TEST(test_fiat_v1_find_known_key);
    RUN_TEST(test_fiat_v1_pack_key2_known_answer);
    RUN_TEST(test_fiat_capture_key2_len_dispatch);
    return UNITY_END();
}
