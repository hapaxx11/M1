/* See COPYING.txt for license details. */

/*
 * Host tests for the M1 ProtoPirate "Mazda V0", "Subaru" and
 * "Chrysler V0" decoders (Sub_Ghz/protocols/m1_mazda_v0_decode.c,
 * m1_subaru_pp_decode.c, m1_chrysler_v0_decode.c).
 *
 * Waveforms are synthesised test-side exactly the way the M1 encoders
 * in Sub_Ghz/subghz_proto_pirate.c (encode_mazda_v0, encode_subaru,
 * encode_chrysler_v0) emit them — which matches the upstream ProtoPirate
 * encoders — and are then delivered to the decoders the way the M1 pulse
 * handler delivers them:
 *  - Mazda V0: the 52 ms gaps hard-separate captures, so the 12-byte
 *    preamble block and the frame body (2x 0xFF, 0xD7 sync, 8 inverted
 *    key bytes, 0x5A tail) arrive as separate buffers.
 *  - Subaru: te_long (1600 us) and the 2800 us gap/sync stay inside one
 *    buffer via the < 5000 us reopen path, so the full burst (75
 *    preamble pairs + gap + sync + 64 bits + trailing gap/sync) arrives
 *    as ONE buffer.
 *  - Chrysler V0: the 3400/3700 us long marks stay inside one buffer,
 *    the 15.6 ms frame gaps hard-separate, so each 80-bit payload
 *    arrives at the start of its own buffer (followed by the next
 *    payload's preamble + frame gap).
 *
 * All expected keys/fields are known-answer vectors produced by a
 * host-side reimplementation of the upstream ProtoPirate construction,
 * cross-checked against the upstream decoder logic.
 */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

uint8_t subghz_decode_mazda_v0(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_subaru_pp(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_chrysler_v0(uint16_t protocol_index, uint16_t pulse_count);

/* Pure helpers exported by the decoders */
uint8_t m1_mazda_v0_checksum(uint32_t serial, uint8_t button, uint32_t counter);
void m1_mazda_v0_parse_key(uint64_t key, uint32_t *serial, uint8_t *button,
                           uint32_t *counter);
bool m1_subaru_pp_decode_fields(uint64_t key, uint32_t *serial,
                                uint8_t *button, uint16_t *counter);
uint8_t m1_chrysler_v0_reverse6(uint32_t value);
void m1_chrysler_v0_transform_block(const uint8_t in[9], uint8_t out[9],
                                    uint32_t key, uint8_t button);
bool m1_chrysler_v0_decode_packet(uint64_t data, uint16_t data_2,
                                  uint32_t *serial, uint8_t *button,
                                  uint8_t *seed_out);

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

static void reset_buffer(void)
{
    s_pulse_count = 0;
}

void setUp(void)
{
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
    s_pulse_count = 0;
}

void tearDown(void) {}

/*====================================================================*/
/* Mazda V0                                                            */
/*====================================================================*/

#define MZ_TE_SHORT   250u
#define MZ_TE_LONG    500u
#define MZ_GAP_US     52000u /* 0xCB20 */
#define MZ_SYNC_BYTE  0xD7u
#define MZ_TAIL_BYTE  0x5Au

/* Test-side reimplementation of the upstream/M1 mazda_v0_encode_key(). */
static uint8_t mz_parity8(uint8_t v)
{
    uint8_t p = 0;
    while (v)
    {
        p ^= v & 1u;
        v >>= 1;
    }
    return p;
}

static uint64_t mz_build_key(uint32_t serial, uint8_t button, uint32_t counter)
{
    uint8_t d[8];
    counter &= 0xFFFFFu;
    button &= 0x0Fu;
    d[0] = (uint8_t)(serial >> 24);
    d[1] = (uint8_t)(serial >> 16);
    d[2] = (uint8_t)(serial >> 8);
    d[3] = (uint8_t)serial;
    d[4] = (uint8_t)((button << 4) | ((counter >> 16) & 0x0Fu));
    d[5] = (uint8_t)(counter >> 8);
    d[6] = (uint8_t)counter;
    d[7] = m1_mazda_v0_checksum(serial, button, counter);

    const uint8_t s5 = (uint8_t)((d[6] & 0x55u) | (d[5] & 0xAAu));
    const uint8_t s6 = (uint8_t)((d[6] & 0xAAu) | (d[5] & 0x55u));
    const uint8_t xm = (uint8_t)(s5 ^ s6);
    const bool rep = mz_parity8(d[7]) == 0u;
    const uint8_t fm = rep ? s5 : s6;
    d[5] = rep ? s5 : xm;
    d[6] = rep ? xm : s6;
    for (uint8_t i = 0; i < 5u; i++)
        d[i] ^= fm;

    uint64_t key = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key = (key << 8) | d[i];
    return key;
}

/* Manchester byte emitter, same cell order as mazda_v0_append_byte() in
 * Sub_Ghz/subghz_proto_pirate.c (bit 1 = high te, low te). */
static void mz_emit_byte_te(uint8_t value, uint32_t te)
{
    for (int b = 7; b >= 0; b--)
    {
        const bool bit = ((value >> b) & 1u) != 0u;
        emit_merge(bit, te);
        emit_merge(!bit, te);
    }
}

static void mz_emit_byte(uint8_t value)
{
    mz_emit_byte_te(value, MZ_TE_SHORT);
}

/* Preamble block (buffer 1): 12x 0xFF + 52 ms low gap (hard split). */
static void mz_emit_preamble_block(void)
{
    for (uint8_t i = 0; i < 12u; i++)
        mz_emit_byte(0xFFu);
    emit_merge(false, MZ_GAP_US);
}

/* Frame body (buffer 2): 2x 0xFF, sync, 8 inverted key bytes, tail. */
static void mz_emit_body(uint64_t key, uint8_t sync_byte)
{
    mz_emit_byte(0xFFu);
    mz_emit_byte(0xFFu);
    mz_emit_byte(sync_byte);
    for (int i = 0; i < 8; i++)
        mz_emit_byte((uint8_t)~((key >> (56 - i * 8)) & 0xFFu));
    mz_emit_byte(MZ_TAIL_BYTE);
}

void test_mazda_v0_checksum_known_answer(void)
{
    TEST_ASSERT_EQUAL_UINT8(0xD8u,
                            m1_mazda_v0_checksum(0x12345678u, 0x02u, 0x0ABCDEu));
    TEST_ASSERT_EQUAL_UINT8(0xB9u,
                            m1_mazda_v0_checksum(0xDEADBEEFu, 0x08u, 0x00001u));
    TEST_ASSERT_EQUAL_UINT8(0xDBu,
                            m1_mazda_v0_checksum(0x0A1B2C3Du, 0x04u, 0xFFFFFu));
}

void test_mazda_v0_parse_key_known_answer(void)
{
    uint32_t serial = 0, counter = 0;
    uint8_t button = 0;

    m1_mazda_v0_parse_key(0xEEC8AA84D6FC62D8ULL, &serial, &button, &counter);
    TEST_ASSERT_EQUAL_UINT32(0x12345678u, serial);
    TEST_ASSERT_EQUAL_UINT8(0x02u, button);
    TEST_ASSERT_EQUAL_UINT32(0x0ABCDEu, counter);

    m1_mazda_v0_parse_key(0xDEADBEEF800100B9ULL, &serial, &button, &counter);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, serial);
    TEST_ASSERT_EQUAL_UINT8(0x08u, button);
    TEST_ASSERT_EQUAL_UINT32(0x00001u, counter);

    m1_mazda_v0_parse_key(0xF5E4D3C2B0FF00DBULL, &serial, &button, &counter);
    TEST_ASSERT_EQUAL_UINT32(0x0A1B2C3Du, serial);
    TEST_ASSERT_EQUAL_UINT8(0x04u, button);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFu, counter);
}

/* Full transmission as the M1 pipeline delivers it: preamble block
 * buffer decodes to nothing, frame body buffer commits. */
void test_mazda_v0_full_frame_decodes(void)
{
    const uint64_t key = 0xEEC8AA84D6FC62D8ULL; /* serial 12345678, btn 2, cnt ABCDE */
    TEST_ASSERT_EQUAL_UINT64(key, mz_build_key(0x12345678u, 0x02u, 0x0ABCDEu));

    mz_emit_preamble_block();
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));

    reset_buffer();
    mz_emit_body(key, MZ_SYNC_BYTE);
    emit_merge(false, MZ_GAP_US); /* trailing frame gap */
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));

    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x12345678u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x0ABCDEu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x02u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(64u, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(0u, subghz_decenc_ctl.ndecodeddelay);
    TEST_ASSERT_EQUAL_UINT16(MAZDA_V0, subghz_decenc_ctl.ndecodedprotocol);
}

/* M1-encoder cross-check: the emitters mirror encode_mazda_v0()'s wire
 * order (12x 0xFF, gap, 2x 0xFF, 0xD7, 8x ~key, 0x5A, gap); this vector
 * exercises the other obfuscation branch (odd checksum parity). */
void test_mazda_v0_m1_encoder_waveform_decodes(void)
{
    const uint64_t key = 0xDEADBEEF800100B9ULL; /* serial DEADBEEF, btn 8, cnt 1 */
    TEST_ASSERT_EQUAL_UINT64(key, mz_build_key(0xDEADBEEFu, 0x08u, 0x00001u));

    mz_emit_preamble_block();
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));

    reset_buffer();
    mz_emit_body(key, MZ_SYNC_BYTE);
    emit_merge(false, MZ_GAP_US);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));

    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(1u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x08u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(MAZDA_V0, subghz_decenc_ctl.ndecodedprotocol);
}

/* Third vector: max counter, trunk button. */
void test_mazda_v0_max_counter_decodes(void)
{
    const uint64_t key = 0xF5E4D3C2B0FF00DBULL; /* serial 0A1B2C3D, btn 4, cnt FFFFF */
    TEST_ASSERT_EQUAL_UINT64(key, mz_build_key(0x0A1B2C3Du, 0x04u, 0xFFFFFu));

    reset_buffer();
    mz_emit_body(key, MZ_SYNC_BYTE);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));

    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x0A1B2C3Du, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x04u, subghz_decenc_ctl.n8_buttonid);
}

void test_mazda_v0_bad_sync_rejected(void)
{
    mz_emit_body(0, 0x00u); /* 0x00 sync + all-zero data: no 0xD7 window */
    emit_merge(false, MZ_GAP_US);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));
}

void test_mazda_v0_bad_checksum_rejected(void)
{
    const uint64_t key = 0xEEC8AA84D6FC62D8ULL;
    /* Flip one serial bit in the first data byte: checksum gate fails. */
    mz_emit_body(key ^ (1ULL << 63), MZ_SYNC_BYTE);
    emit_merge(false, MZ_GAP_US);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));
}

void test_mazda_v0_out_of_tolerance_rejected(void)
{
    /* 370 us cells: 120 us from te_short and 130 us from te_long —
     * outside the upstream te_delta + 20 tolerance. */
    for (uint8_t i = 0; i < 4u; i++)
        mz_emit_byte_te(0xFFu, 370u);
    mz_emit_byte_te(MZ_SYNC_BYTE, 370u);
    for (uint8_t i = 0; i < 8u; i++)
        mz_emit_byte_te(0x35u, 370u);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));
}

void test_mazda_v0_truncated_rejected(void)
{
    const uint64_t key = 0xEEC8AA84D6FC62D8ULL;
    mz_emit_byte(0xFFu);
    mz_emit_byte(0xFFu);
    mz_emit_byte(MZ_SYNC_BYTE);
    for (int i = 0; i < 5; i++) /* only 40 of 64 data bits */
        mz_emit_byte((uint8_t)~((key >> (56 - i * 8)) & 0xFFu));
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_mazda_v0(MAZDA_V0, s_pulse_count));
}

/*====================================================================*/
/* Subaru                                                              */
/*====================================================================*/

#define SU_TE_SHORT 800u
#define SU_TE_LONG  1600u
#define SU_GAP_US   2800u
#define SU_SYNC_US  2800u
#define SU_PAIRS    75u

/* Test-side reimplementation of the upstream/M1 subaru_encode_count(). */
static void su_encode_count(uint8_t *kb, uint16_t count)
{
    const uint8_t lo = (uint8_t)count;
    const uint8_t hi = (uint8_t)(count >> 8);

    kb[4] &= (uint8_t)~0xC0u;
    kb[5] &= (uint8_t)~0xC3u;
    kb[6] &= (uint8_t)~0x03u;

    if ((lo & 0x01u) == 0u) kb[4] |= 0x40u;
    if ((lo & 0x02u) == 0u) kb[4] |= 0x80u;
    if ((lo & 0x04u) == 0u) kb[5] |= 0x01u;
    if ((lo & 0x08u) == 0u) kb[5] |= 0x02u;
    if ((lo & 0x10u) == 0u) kb[6] |= 0x01u;
    if ((lo & 0x20u) == 0u) kb[6] |= 0x02u;
    if ((lo & 0x40u) == 0u) kb[5] |= 0x40u;
    if ((lo & 0x80u) == 0u) kb[5] |= 0x80u;

    uint8_t ser0 = kb[3], ser1 = kb[1], ser2 = kb[2];
    for (uint8_t i = 0; i < (uint8_t)(4u + lo); i++)
    {
        const uint8_t t = (uint8_t)(ser0 >> 7);
        ser0 = (uint8_t)(((ser0 << 1) & 0xFEu) | (ser1 >> 7));
        ser1 = (uint8_t)(((ser1 << 1) & 0xFEu) | (ser2 >> 7));
        ser2 = (uint8_t)(((ser2 << 1) & 0xFEu) | t);
    }

    const uint8_t rel = (uint8_t)(ser0 ^ kb[0]);
    kb[4] = (uint8_t)((kb[4] & 0xC0u) | ((rel >> 2) & 0x3Fu));
    kb[5] = (uint8_t)((kb[5] & 0xCFu) | ((rel << 4) & 0x30u));

    uint8_t t1 = 0xFFu, t2 = 0xFFu;
    if (hi & 0x04u) t1 &= (uint8_t)~0x10u;
    if (hi & 0x08u) t1 &= (uint8_t)~0x20u;
    if (hi & 0x02u) t2 &= (uint8_t)~0x80u;
    if (hi & 0x01u) t2 &= (uint8_t)~0x40u;
    if (hi & 0x40u) t1 &= (uint8_t)~0x01u;
    if (hi & 0x80u) t1 &= (uint8_t)~0x02u;
    if (hi & 0x20u) t2 &= (uint8_t)~0x08u;
    if (hi & 0x10u) t2 &= (uint8_t)~0x04u;

    const uint8_t n1 = (uint8_t)(t1 ^ ser1);
    const uint8_t n2 = (uint8_t)(t2 ^ ser2);

    kb[5] &= (uint8_t)~0x0Cu;
    kb[6] &= (uint8_t)~0xC0u;
    kb[7] = (uint8_t)((kb[7] & 0xF0u) | ((n1 >> 4) & 0x0Fu));
    if (n1 & 0x04u) kb[5] |= 0x04u;
    if (n1 & 0x08u) kb[5] |= 0x08u;
    if (n1 & 0x02u) kb[6] |= 0x80u;
    if (n1 & 0x01u) kb[6] |= 0x40u;
    kb[6] = (uint8_t)((kb[6] & 0xC3u) | ((n2 >> 2) & 0x3Cu));
    kb[7] = (uint8_t)((kb[7] & 0x0Fu) | ((n2 << 4) & 0xF0u));
}

static uint64_t su_build_key(uint32_t serial, uint8_t button, uint16_t count)
{
    uint8_t b[8] = {0};
    b[0] = button & 0x0Fu;
    b[1] = (uint8_t)(serial >> 16);
    b[2] = (uint8_t)(serial >> 8);
    b[3] = (uint8_t)serial;
    su_encode_count(b, count);
    uint64_t key = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key = (key << 8) | b[i];
    return key;
}

/* Burst emitter, same wire order as encode_subaru() in
 * Sub_Ghz/subghz_proto_pirate.c: 75 (low, high) te_long pairs, gap,
 * sync, 64 PWM bits (bit 1 = low te_long + high te_short), trailing
 * gap + sync.  Everything stays in ONE capture buffer. */
static void su_emit_burst(uint64_t key)
{
    for (uint8_t i = 0; i < SU_PAIRS; i++)
    {
        emit_merge(false, SU_TE_LONG);
        emit_merge(true, SU_TE_LONG);
    }
    emit_merge(false, SU_GAP_US);
    emit_merge(true, SU_SYNC_US);
    for (int i = 63; i >= 0; i--)
    {
        if ((key >> i) & 1u)
        {
            emit_merge(false, SU_TE_LONG);
            emit_merge(true, SU_TE_SHORT);
        }
        else
        {
            emit_merge(false, SU_TE_SHORT);
            emit_merge(true, SU_TE_LONG);
        }
    }
    emit_merge(false, SU_GAP_US);
    emit_merge(true, SU_SYNC_US);
}

void test_subaru_decode_fields_known_answer(void)
{
    uint32_t serial = 0;
    uint16_t counter = 0;
    uint8_t button = 0;

    (void)m1_subaru_pp_decode_fields(0x01ABCDEFEAE2A443ULL, &serial, &button,
                                     &counter);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEFu, serial);
    TEST_ASSERT_EQUAL_UINT8(0x01u, button);
    TEST_ASSERT_EQUAL_UINT16(0x1234u, counter);

    (void)m1_subaru_pp_decode_fields(0x03000001023C0C3CULL, &serial, &button,
                                     &counter);
    TEST_ASSERT_EQUAL_UINT32(0x000001u, serial);
    TEST_ASSERT_EQUAL_UINT8(0x03u, button);
    TEST_ASSERT_EQUAL_UINT16(0xFFFFu, counter);

    (void)m1_subaru_pp_decode_fields(0x0211223398CB6F9DULL, &serial, &button,
                                     &counter);
    TEST_ASSERT_EQUAL_UINT32(0x112233u, serial);
    TEST_ASSERT_EQUAL_UINT8(0x02u, button);
    TEST_ASSERT_EQUAL_UINT16(0x8001u, counter);

    /* Random garbage: outputs still written; the upstream
     * self-consistency flag reports false. */
    const bool valid = m1_subaru_pp_decode_fields(0xDEADBEEFCAFEBABEULL,
                                                  &serial, &button, &counter);
    TEST_ASSERT_FALSE(valid);
    TEST_ASSERT_EQUAL_UINT32(0xADBEEFu, serial);
    TEST_ASSERT_EQUAL_UINT8(0x0Eu, button);
    TEST_ASSERT_EQUAL_UINT16(0x2E14u, counter);
}

void test_subaru_full_frame_decodes(void)
{
    const uint64_t key = 0x01ABCDEFEAE2A443ULL; /* serial ABCDEF, btn 1, cnt 1234 */
    TEST_ASSERT_EQUAL_UINT64(key, su_build_key(0xABCDEFu, 1u, 0x1234u));

    su_emit_burst(key);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_subaru_pp(SUBARU_PP, s_pulse_count));

    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEFu, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x1234u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x01u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(64u, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(0u, subghz_decenc_ctl.ndecodeddelay);
    TEST_ASSERT_EQUAL_UINT16(SUBARU_PP, subghz_decenc_ctl.ndecodedprotocol);
}

/* M1-encoder cross-check: su_emit_burst() mirrors encode_subaru()'s
 * wire order exactly (75 pairs, 2800 us gap/sync, PWM bits). */
void test_subaru_m1_encoder_waveform_decodes(void)
{
    /* cnt FFFF exercises the upstream uint8_t rotation-count wrap
     * ((4 + 0xFF) & 0xFF = 3) shared by the encoder and decoder. */
    const uint64_t key = 0x03000001023C0C3CULL; /* serial 000001, btn 3, cnt FFFF */
    TEST_ASSERT_EQUAL_UINT64(key, su_build_key(0x000001u, 3u, 0xFFFFu));

    su_emit_burst(key);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_subaru_pp(SUBARU_PP, s_pulse_count));

    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x000001u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x03u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(SUBARU_PP, subghz_decenc_ctl.ndecodedprotocol);
}

void test_subaru_second_vector_decodes(void)
{
    const uint64_t key = 0x0211223398CB6F9DULL; /* serial 112233, btn 2, cnt 8001 */
    TEST_ASSERT_EQUAL_UINT64(key, su_build_key(0x112233u, 2u, 0x8001u));

    su_emit_burst(key);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_subaru_pp(SUBARU_PP, s_pulse_count));

    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x112233u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x8001u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x02u, subghz_decenc_ctl.n8_buttonid);
}

void test_subaru_short_preamble_rejected(void)
{
    const uint64_t key = 0x01ABCDEFEAE2A443ULL;
    for (uint8_t i = 0; i < 10u; i++) /* < the 21-pair minimum */
    {
        emit_merge(false, SU_TE_LONG);
        emit_merge(true, SU_TE_LONG);
    }
    emit_merge(false, SU_GAP_US);
    emit_merge(true, SU_SYNC_US);
    for (int i = 63; i >= 0; i--)
    {
        if ((key >> i) & 1u)
        {
            emit_merge(false, SU_TE_LONG);
            emit_merge(true, SU_TE_SHORT);
        }
        else
        {
            emit_merge(false, SU_TE_SHORT);
            emit_merge(true, SU_TE_LONG);
        }
    }
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_subaru_pp(SUBARU_PP, s_pulse_count));
}

void test_subaru_bad_gap_rejected(void)
{
    const uint64_t key = 0x01ABCDEFEAE2A443ULL;
    for (uint8_t i = 0; i < SU_PAIRS; i++)
    {
        emit_merge(false, SU_TE_LONG);
        emit_merge(true, SU_TE_LONG);
    }
    emit_merge(false, 4200u); /* outside the 2800 +/- 800 gap window */
    emit_merge(true, SU_SYNC_US);
    for (int i = 63; i >= 0; i--)
    {
        if ((key >> i) & 1u)
        {
            emit_merge(false, SU_TE_LONG);
            emit_merge(true, SU_TE_SHORT);
        }
        else
        {
            emit_merge(false, SU_TE_SHORT);
            emit_merge(true, SU_TE_LONG);
        }
    }
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_subaru_pp(SUBARU_PP, s_pulse_count));
}

void test_subaru_out_of_tolerance_rejected(void)
{
    const uint64_t key = 0x01ABCDEFEAE2A443ULL;
    for (uint8_t i = 0; i < SU_PAIRS; i++)
    {
        emit_merge(false, SU_TE_LONG);
        emit_merge(true, SU_TE_LONG);
    }
    emit_merge(false, SU_GAP_US);
    emit_merge(true, SU_SYNC_US);
    for (int i = 63; i >= 0; i--)
    {
        /* Bit 60's low is 1400 us: 200 from te_long (not < 200) and 600
         * from te_short — the sync check rejects it. */
        const bool bad = (i == 60);
        if ((key >> i) & 1u)
        {
            emit_merge(false, bad ? 1400u : SU_TE_LONG);
            emit_merge(true, SU_TE_SHORT);
        }
        else
        {
            emit_merge(false, bad ? 1400u : SU_TE_SHORT);
            emit_merge(true, SU_TE_LONG);
        }
    }
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_subaru_pp(SUBARU_PP, s_pulse_count));
}

void test_subaru_truncated_rejected(void)
{
    const uint64_t key = 0x01ABCDEFEAE2A443ULL;
    for (uint8_t i = 0; i < SU_PAIRS; i++)
    {
        emit_merge(false, SU_TE_LONG);
        emit_merge(true, SU_TE_LONG);
    }
    emit_merge(false, SU_GAP_US);
    emit_merge(true, SU_SYNC_US);
    for (int i = 63; i >= 24; i--) /* only 40 of 64 bits */
    {
        if ((key >> i) & 1u)
        {
            emit_merge(false, SU_TE_LONG);
            emit_merge(true, SU_TE_SHORT);
        }
        else
        {
            emit_merge(false, SU_TE_SHORT);
            emit_merge(true, SU_TE_LONG);
        }
    }
    emit_merge(false, SU_GAP_US);
    emit_merge(true, SU_SYNC_US);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_subaru_pp(SUBARU_PP, s_pulse_count));
}

/*====================================================================*/
/* Chrysler V0                                                         */
/*====================================================================*/

#define CH_TE_SHORT    300u
#define CH_TE_ONE      600u
#define CH_LONG_A      3400u
#define CH_LONG_B      3700u
#define CH_FRAME_GAP   15600u
#define CH_PREAMBLE    24u

/* Test-side reimplementation of the upstream/M1 payload construction. */
static void ch_build_payload(const uint8_t plain[9], uint8_t counter,
                             uint8_t button, uint8_t header_low2,
                             uint8_t out[10])
{
    uint8_t transformed[9];
    m1_chrysler_v0_transform_block(plain, transformed, counter, button);
    out[0] = (uint8_t)((m1_chrysler_v0_reverse6(counter) << 2) |
                       (header_low2 & 0x03u));
    memcpy(&out[1], transformed, 9);
}

/* Payload emitter, same wire order as encode_chrysler_v0() in
 * Sub_Ghz/subghz_proto_pirate.c: 24 (high 300, low 3700) preamble
 * pairs, high 300, 15.6 ms frame gap, then 80 PWM bits
 * (bit 1 = high 600 + low 3400; bit 0 = high 300 + low 3700). */
static void ch_emit_preamble(void)
{
    for (uint8_t i = 0; i < CH_PREAMBLE; i++)
    {
        emit_merge(true, CH_TE_SHORT);
        emit_merge(false, CH_LONG_B);
    }
    emit_merge(true, CH_TE_SHORT);
    emit_merge(false, CH_FRAME_GAP);
}

static void ch_emit_data(const uint8_t payload[10], uint8_t bits)
{
    for (uint8_t bit = 0; bit < bits; bit++)
    {
        const bool v = ((payload[bit >> 3] >> (7u - (bit & 7u))) & 1u) != 0u;
        emit_merge(true, v ? CH_TE_ONE : CH_TE_SHORT);
        emit_merge(false, v ? CH_LONG_A : CH_LONG_B);
    }
}

/* Valid frame vectors (known answers from the upstream construction):
 * frame 1 (button 1): payload A counter 0x2A, payload B counter 0x29.
 * frame 2 (button 2): payload A counter 0x10, payload B counter 0x0F. */
static const uint8_t CH_PLAIN_A1[9] = {0x51, 0x22, 0x33, 0x44, 0x92, 0x55, 0x77, 0x88, 0x99};
static const uint8_t CH_PLAIN_B1[9] = {0x34, 0xBB, 0xCC, 0xDD, 0x34, 0x56, 0x12, 0x34, 0x56};
static const uint8_t CH_PLAIN_A2[9] = {0x66, 0x11, 0x22, 0x33, 0xA5, 0x6E, 0x77, 0x88, 0x99};
static const uint8_t CH_PLAIN_B2[9] = {0x21, 0xA1, 0xB2, 0x30, 0x21, 0x43, 0xC4, 0xD5, 0x20};

#define CH_KEY1_A1 0x560E7D6C1BCD0A28ULL
#define CH_KEY2_A1 0xD7C6u
#define CH_KEY1_B1 0x96CE413627CEACE8ULL
#define CH_KEY2_B1 0xCEACu
#define CH_KEY1_A2 0x09691E2D3CAA6178ULL
#define CH_KEY2_A2 0x8796u
#define CH_KEY1_B2 0xF1D15142C0D1B334ULL
#define CH_KEY2_B2 0x25D0u

void test_chrysler_v0_reverse6_known_answer(void)
{
    TEST_ASSERT_EQUAL_UINT8(0x00u, m1_chrysler_v0_reverse6(0x00u));
    TEST_ASSERT_EQUAL_UINT8(0x20u, m1_chrysler_v0_reverse6(0x01u));
    TEST_ASSERT_EQUAL_UINT8(0x15u, m1_chrysler_v0_reverse6(0x2Au));
    TEST_ASSERT_EQUAL_UINT8(0x3Fu, m1_chrysler_v0_reverse6(0x3Fu));
}

void test_chrysler_v0_transform_block_known_answer(void)
{
    const uint8_t in[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t out[9];

    m1_chrysler_v0_transform_block(in, out, 0u, 1u);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, 9); /* mask 0x0F ^ 0x0F = 0 */

    static const uint8_t expect5[9] =
        {0xFE, 0xFF, 0xFC, 0xFD, 0xFA, 0xFB, 0xF8, 0xF9, 0xF6};
    m1_chrysler_v0_transform_block(in, out, 5u, 1u);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect5, out, 9); /* mask 0x0E ^ 0xF0 */
}

void test_chrysler_v0_decode_packet_known_answer(void)
{
    uint32_t serial = 0;
    uint8_t button = 0, seed = 0;

    TEST_ASSERT_TRUE(m1_chrysler_v0_decode_packet(CH_KEY1_A1, CH_KEY2_A1,
                                                  &serial, &button, &seed));
    TEST_ASSERT_EQUAL_UINT8(0x2Au, seed);
    TEST_ASSERT_EQUAL_UINT8(1u, button);
    TEST_ASSERT_EQUAL_UINT32(0x51223344u, serial); /* even seed: plain_A[0..3] */

    TEST_ASSERT_TRUE(m1_chrysler_v0_decode_packet(CH_KEY1_B1, CH_KEY2_B1,
                                                  &serial, &button, &seed));
    TEST_ASSERT_EQUAL_UINT8(0x29u, seed);
    TEST_ASSERT_EQUAL_UINT8(1u, button);
    TEST_ASSERT_EQUAL_UINT32(0x34BBCC34u, serial); /* odd seed: plain_B[0..2,7] */

    TEST_ASSERT_TRUE(m1_chrysler_v0_decode_packet(CH_KEY1_A2, CH_KEY2_A2,
                                                  &serial, &button, &seed));
    TEST_ASSERT_EQUAL_UINT8(0x10u, seed);
    TEST_ASSERT_EQUAL_UINT8(2u, button);
    TEST_ASSERT_EQUAL_UINT32(0x66112233u, serial);

    TEST_ASSERT_TRUE(m1_chrysler_v0_decode_packet(CH_KEY1_B2, CH_KEY2_B2,
                                                  &serial, &button, &seed));
    TEST_ASSERT_EQUAL_UINT8(0x0Fu, seed);
    TEST_ASSERT_EQUAL_UINT8(2u, button);
    TEST_ASSERT_EQUAL_UINT32(0x21A1B2D5u, serial);

    /* Corrupted check byte (key[5] 0xCD -> 0xCE): check gate fails. */
    TEST_ASSERT_FALSE(m1_chrysler_v0_decode_packet(0x560E7D6C1BCE0A28ULL,
                                                   CH_KEY2_A1, &serial,
                                                   &button, &seed));
}

void test_chrysler_v0_build_payload_matches_vectors(void)
{
    uint8_t payload[10];
    uint64_t key1;
    uint16_t key2;

    ch_build_payload(CH_PLAIN_A1, 0x2Au, 1u, 2u, payload);
    key1 = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key1 = (key1 << 8) | payload[i];
    key2 = (uint16_t)((payload[8] << 8) | payload[9]);
    TEST_ASSERT_EQUAL_UINT64(CH_KEY1_A1, key1);
    TEST_ASSERT_EQUAL_UINT16(CH_KEY2_A1, key2);

    ch_build_payload(CH_PLAIN_B2, 0x0Fu, 2u, 1u, payload);
    key1 = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key1 = (key1 << 8) | payload[i];
    key2 = (uint16_t)((payload[8] << 8) | payload[9]);
    TEST_ASSERT_EQUAL_UINT64(CH_KEY1_B2, key1);
    TEST_ASSERT_EQUAL_UINT16(CH_KEY2_B2, key2);
}

/* Full burst in ONE buffer: preamble + frame gap + 80 data bits.  This
 * is the shape the upstream machine handles natively. */
void test_chrysler_v0_single_burst_decodes(void)
{
    uint8_t payload_a[10];
    ch_build_payload(CH_PLAIN_A1, 0x2Au, 1u, 2u, payload_a);

    ch_emit_preamble();
    ch_emit_data(payload_a, 80u);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));

    TEST_ASSERT_EQUAL_UINT64(CH_KEY1_A1, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT64(CH_KEY2_A1, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT32(0x51223344u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x2Au, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(1u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(80u, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(0u, subghz_decenc_ctl.ndecodeddelay);
    TEST_ASSERT_EQUAL_UINT16(CHRYSLER_V0, subghz_decenc_ctl.ndecodedprotocol);
}

/* Full transmission as the M1 pipeline delivers it: the 15.6 ms frame
 * gaps hard-separate the capture, so each payload's 80 data bits arrive
 * at the start of their own buffer (followed by the next payload's
 * preamble + frame gap).  Mirrors encode_chrysler_v0()'s wire order
 * (preamble + gap + payload A, preamble + gap + payload B). */
void test_chrysler_v0_pipeline_split_decodes_both_payloads(void)
{
    uint8_t payload_a[10], payload_b[10];
    ch_build_payload(CH_PLAIN_A1, 0x2Au, 1u, 2u, payload_a);
    ch_build_payload(CH_PLAIN_B1, 0x29u, 1u, 2u, payload_b);

    /* Buffer 1: preamble A + frame gap (arms upstream Seek->Data). */
    ch_emit_preamble();
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));

    /* Buffer 2: data A + preamble B + frame gap. */
    reset_buffer();
    ch_emit_data(payload_a, 80u);
    ch_emit_preamble();
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(CH_KEY1_A1, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT64(CH_KEY2_A1, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT32(0x51223344u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x2Au, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(1u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(80u, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(CHRYSLER_V0, subghz_decenc_ctl.ndecodedprotocol);

    /* Buffer 3: data B alone (end of transmission). */
    reset_buffer();
    ch_emit_data(payload_b, 80u);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(CH_KEY1_B1, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT64(CH_KEY2_B1, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT32(0x34BBCC34u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x29u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(1u, subghz_decenc_ctl.n8_buttonid);
}

/* M1-encoder cross-check, button-2 frame: encode_chrysler_v0() emits
 * payload A (even counter) then payload B (counter - 1), same wire
 * shape as above. */
void test_chrysler_v0_button2_frame_decodes(void)
{
    uint8_t payload_a[10], payload_b[10];
    ch_build_payload(CH_PLAIN_A2, 0x10u, 2u, 1u, payload_a);
    ch_build_payload(CH_PLAIN_B2, 0x0Fu, 2u, 1u, payload_b);

    ch_emit_preamble();
    TEST_ASSERT_EQUAL_UINT8(1,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));

    reset_buffer();
    ch_emit_data(payload_a, 80u);
    ch_emit_preamble();
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(0,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(CH_KEY1_A2, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT64(CH_KEY2_A2, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT32(0x66112233u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x10u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(2u, subghz_decenc_ctl.n8_buttonid);

    reset_buffer();
    ch_emit_data(payload_b, 80u);
    TEST_ASSERT_EQUAL_UINT8(0,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(CH_KEY1_B2, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT64(CH_KEY2_B2, subghz_decenc_ctl.n32_key2);
    TEST_ASSERT_EQUAL_UINT32(0x21A1B2D5u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x0Fu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(2u, subghz_decenc_ctl.n8_buttonid);
}

void test_chrysler_v0_short_preamble_rejected(void)
{
    uint8_t payload_a[10];
    ch_build_payload(CH_PLAIN_A1, 0x2Au, 1u, 2u, payload_a);

    for (uint8_t i = 0; i < 12u; i++) /* < the > 15 pair minimum */
    {
        emit_merge(true, CH_TE_SHORT);
        emit_merge(false, CH_LONG_B);
    }
    emit_merge(true, CH_TE_SHORT);
    emit_merge(false, CH_FRAME_GAP);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));
}

void test_chrysler_v0_bad_check_rejected(void)
{
    uint8_t payload_a[10];
    ch_build_payload(CH_PLAIN_A1, 0x2Au, 1u, 2u, payload_a);
    payload_a[5] ^= 0x01u; /* breaks (key[1] ^ 0xC3) == key[5] */

    reset_buffer();
    ch_emit_data(payload_a, 80u);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));
}

void test_chrysler_v0_out_of_tolerance_rejected(void)
{
    uint8_t payload_a[10];
    ch_build_payload(CH_PLAIN_A1, 0x2Au, 1u, 2u, payload_a);

    /* Data bit 10's low mark is 2900 us: 500 from long A and 800 from
     * long B (outside the +/- 400 window) and below the 8000 us gap
     * threshold — the data run breaks before 80 bits. */
    ch_emit_preamble();
    for (uint8_t bit = 0; bit < 80u; bit++)
    {
        const bool v = ((payload_a[bit >> 3] >> (7u - (bit & 7u))) & 1u) != 0u;
        emit_merge(true, v ? CH_TE_ONE : CH_TE_SHORT);
        emit_merge(false, bit == 10u ? 2900u : (v ? CH_LONG_A : CH_LONG_B));
    }
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));
}

void test_chrysler_v0_truncated_rejected(void)
{
    uint8_t payload_a[10];
    ch_build_payload(CH_PLAIN_A1, 0x2Au, 1u, 2u, payload_a);

    ch_emit_preamble();
    ch_emit_data(payload_a, 79u); /* one bit short of the 80-bit commit */
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);
    TEST_ASSERT_EQUAL_UINT8(1,
                            subghz_decode_chrysler_v0(CHRYSLER_V0, s_pulse_count));
}

/*====================================================================*/

void test_capture_key2_len_dispatch_phase6(void)
{
    /* Chrysler V0 persists its 16-bit second wire word as a 2-byte
     * Key_2; Mazda V0 and Subaru carry no auxiliary word. */
    TEST_ASSERT_EQUAL_UINT8(2u, subghz_capture_key2_len(CHRYSLER_V0, 0x1234u));
    TEST_ASSERT_EQUAL_UINT8(0u, subghz_capture_key2_len(MAZDA_V0, 0x1234u));
    TEST_ASSERT_EQUAL_UINT8(0u, subghz_capture_key2_len(SUBARU_PP, 0x1234u));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_mazda_v0_checksum_known_answer);
    RUN_TEST(test_mazda_v0_parse_key_known_answer);
    RUN_TEST(test_mazda_v0_full_frame_decodes);
    RUN_TEST(test_mazda_v0_m1_encoder_waveform_decodes);
    RUN_TEST(test_mazda_v0_max_counter_decodes);
    RUN_TEST(test_mazda_v0_bad_sync_rejected);
    RUN_TEST(test_mazda_v0_bad_checksum_rejected);
    RUN_TEST(test_mazda_v0_out_of_tolerance_rejected);
    RUN_TEST(test_mazda_v0_truncated_rejected);

    RUN_TEST(test_subaru_decode_fields_known_answer);
    RUN_TEST(test_subaru_full_frame_decodes);
    RUN_TEST(test_subaru_m1_encoder_waveform_decodes);
    RUN_TEST(test_subaru_second_vector_decodes);
    RUN_TEST(test_subaru_short_preamble_rejected);
    RUN_TEST(test_subaru_bad_gap_rejected);
    RUN_TEST(test_subaru_out_of_tolerance_rejected);
    RUN_TEST(test_subaru_truncated_rejected);

    RUN_TEST(test_chrysler_v0_reverse6_known_answer);
    RUN_TEST(test_chrysler_v0_transform_block_known_answer);
    RUN_TEST(test_chrysler_v0_decode_packet_known_answer);
    RUN_TEST(test_chrysler_v0_build_payload_matches_vectors);
    RUN_TEST(test_chrysler_v0_single_burst_decodes);
    RUN_TEST(test_chrysler_v0_pipeline_split_decodes_both_payloads);
    RUN_TEST(test_chrysler_v0_button2_frame_decodes);
    RUN_TEST(test_chrysler_v0_short_preamble_rejected);
    RUN_TEST(test_chrysler_v0_bad_check_rejected);
    RUN_TEST(test_chrysler_v0_out_of_tolerance_rejected);
    RUN_TEST(test_chrysler_v0_truncated_rejected);

    RUN_TEST(test_capture_key2_len_dispatch_phase6);

    return UNITY_END();
}
