/* See COPYING.txt for license details. */

/*
 * Host tests for the M1 ProtoPirate "Kia V0" family decoder
 * (Sub_Ghz/protocols/m1_kia_v0_decode.c).
 *
 * Waveforms are synthesised test-side from the upstream ProtoPirate
 * kia_v0.c wire format (symmetric PWM pairs, MSB first), covering the
 * Kia, Suzuki, Honda V0 and Mitsubishi sub-types, the M1 packet-split
 * adaptations, and the CRC/structural negative gates.
 */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_blocks_math.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

uint8_t subghz_decode_kia_v0(uint16_t protocol_index, uint16_t pulse_count);
void m1_kia_v0_decode_reset(void);

/* Pure helpers exported by the decoder */
uint8_t m1_kia_v0_wire_crc(uint64_t data);
bool m1_kia_v0_kia_crc_valid(uint64_t data);
bool m1_kia_v0_suzuki_resolve(uint64_t data, uint64_t *shifted);
uint64_t m1_kia_v0_honda_transform(uint64_t data);
bool m1_kia_v0_honda_key_valid(uint64_t key);

#define TE_SHORT 250u
#define TE_LONG  500u

#define TYPE_KIA    1u
#define TYPE_SUZUKI 2u
#define TYPE_HONDA  3u
#define TYPE_MITSU  4u

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
    m1_kia_v0_decode_reset();
}

void tearDown(void) {}

/*--------------------------------------------------------------------*/
/* Test-side field -> key builders (independent reimplementation of    */
/* the upstream layout)                                                */
/*--------------------------------------------------------------------*/

static uint8_t rev8(uint8_t x)
{
    x = (uint8_t)(((x & 0xF0u) >> 4) | ((x & 0x0Fu) << 4));
    x = (uint8_t)(((x & 0xCCu) >> 2) | ((x & 0x33u) << 2));
    x = (uint8_t)(((x & 0xAAu) >> 1) | ((x & 0x55u) << 1));
    return x;
}

static uint64_t build_kia_key(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    const uint32_t high = 0x0F000000u | ((uint32_t)cnt << 8) |
                          ((serial >> 20) & 0xFFu);
    const uint32_t low = ((serial & 0x000FFFFFu) << 12) |
                         ((uint32_t)(btn & 0x0Fu) << 8);
    const uint64_t partial = ((uint64_t)high << 32) | low;
    uint8_t buf[6];
    for (uint8_t i = 0; i < 6u; i++)
        buf[i] = (uint8_t)(partial >> (48u - i * 8u));
    return partial | subghz_protocol_blocks_crc8(buf, 6, 0x7Fu, 0x00u);
}

static uint64_t build_suzuki_key(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    const uint16_t c_sw = (uint16_t)((cnt << 8) | (cnt >> 8));
    const uint8_t buf[6] = {
        (uint8_t)(c_sw & 0xFFu),
        (uint8_t)(c_sw >> 8),
        (uint8_t)((serial >> 20) & 0xFFu),
        (uint8_t)((serial >> 12) & 0xFFu),
        (uint8_t)((serial >> 4) & 0xFFu),
        (uint8_t)((btn & 0x0Fu) | (uint8_t)(serial << 4)),
    };
    const uint8_t crc = subghz_protocol_blocks_crc8(buf, 6, 0x7Fu, 0x00u);
    const uint32_t r8 = (serial << 16) | ((uint32_t)crc << 4) |
                        ((uint32_t)(btn & 0x0Fu) << 12);
    const uint32_t r5 = ((serial >> 16) & 0xFFFu) | ((uint32_t)cnt << 12) |
                        0xF0000000u;
    return ((uint64_t)r5 << 32) | r8;
}

static uint64_t build_honda_key(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    static const uint8_t fold_table[16] = {
        0x4A, 0x25, 0x96, 0x4B, 0xA1, 0xD4, 0x6A, 0x35,
        0x9E, 0x4F, 0xA3, 0xD5, 0xEE, 0x77, 0xBF, 0xDB,
    };
    const uint8_t b3 = btn & 0x07u;
    const uint8_t header = (uint8_t)(((b3 << 5) & 0xE0u) | (b3 == 7u ? 0x1Au : 0x0Au));
    uint8_t crc = 0;
    for (uint8_t i = 0; i < 16u; i++)
        if ((cnt >> i) & 1u)
            crc ^= fold_table[i];
    switch (header)
    {
        case 0xAA: crc ^= 0xA5u; break;
        case 0x2A: crc ^= 0x21u; break;
        case 0x6A: crc ^= 0x15u; break;
        case 0xFA: crc ^= 0x73u; break;
        default:   crc ^= 0xC6u; break;
    }
    const uint8_t bytes[8] = {
        0xF0,
        rev8((uint8_t)(cnt >> 8)),
        rev8((uint8_t)cnt),
        (uint8_t)(serial >> 16),
        (uint8_t)(serial >> 8),
        (uint8_t)serial,
        header,
        crc,
    };
    uint64_t key = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key = (key << 8) | bytes[i];
    return key;
}

/*--------------------------------------------------------------------*/
/* Waveform emitters (upstream wire format: symmetric PWM pairs)       */
/*--------------------------------------------------------------------*/

static void emit_short_pairs(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        emit_merge(true, TE_SHORT);
        emit_merge(false, TE_SHORT);
    }
}

static void emit_long_pairs(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        emit_merge(true, TE_LONG);
        emit_merge(false, TE_LONG);
    }
}

static void emit_data_msb(uint64_t raw, uint8_t bits)
{
    for (int i = (int)bits - 1; i >= 0; i--)
    {
        const uint32_t d = ((raw >> i) & 1u) ? TE_LONG : TE_SHORT;
        emit_merge(true, d);
        emit_merge(false, d);
    }
}

/* Kia/Mitsubishi first copy: sync + 319 pairs + 61 bits + 1500/1500. */
static void emit_kia_first_copy(uint64_t raw)
{
    emit_merge(true, 750);
    emit_merge(false, 750);
    emit_short_pairs(319);
    emit_data_msb(raw, 61);
    emit_merge(true, 1500);
    emit_merge(false, 1500);
}

/* Kia tail copy: 15 pairs + 61 bits + high 1000 commit gap. */
static void emit_kia_tail_copy(uint64_t raw)
{
    emit_short_pairs(15);
    emit_data_msb(raw, 61);
    emit_merge(true, 1000);
}

/* Mitsubishi tail copy: 80 pairs + 61 bits + 1500/1500. */
static void emit_mitsu_tail_copy(uint64_t raw)
{
    emit_short_pairs(80);
    emit_data_msb(raw, 61);
    emit_merge(true, 1500);
    emit_merge(false, 1500);
}

/* Suzuki first copy: 320 pairs + 64 bits; the trailing 2000 us low is
 * merged into the last data low by emit_merge(), as on air. */
static void emit_suzuki_first_copy(uint64_t shifted)
{
    emit_short_pairs(320);
    emit_data_msb(shifted, 64);
}

/*--------------------------------------------------------------------*/
/* Kia (type 1)                                                        */
/*--------------------------------------------------------------------*/

void test_kia_v0_kia_full_frame_decodes(void)
{
    const uint64_t raw = build_kia_key(0x0123456u, 0x02u, 0x1234u);

    emit_kia_first_copy(raw);
    emit_kia_tail_copy(raw);
    emit_merge(false, 5000); /* inter-frame silence ends the capture */
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    /* Bit 60 is forced to 1 by the decoder's sync pre-load (0x1F top byte). */
    TEST_ASSERT_EQUAL_UINT64(raw | (1uLL << 60), subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x0123456u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x1234u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x02u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(61, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(KIA_V0, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_UINT8(TYPE_KIA,
                            (uint8_t)(subghz_decenc_ctl.n32_key2 >> 16));
}

void test_kia_v0_kia_first_copy_alone_decodes(void)
{
    const uint64_t raw = build_kia_key(0x1ABCDEFu, 0x01u, 0xBEEFu);
    emit_kia_first_copy(raw);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x1ABCDEFu, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xBEEFu, subghz_decenc_ctl.n32_rollingcode);
}

void test_kia_v0_kia_tail_copy_alone_decodes(void)
{
    const uint64_t raw = build_kia_key(0x0000042u, 0x03u, 0x0001u);
    emit_kia_tail_copy(raw);
    emit_merge(false, 5000);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x42u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT8(0x03u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT8(TYPE_KIA,
                            (uint8_t)(subghz_decenc_ctl.n32_key2 >> 16));
}

void test_kia_v0_kia_bad_crc_rejected(void)
{
    const uint64_t raw = build_kia_key(0x0123456u, 0x02u, 0x1234u) ^ 0x01u;
    emit_kia_first_copy(raw);
    emit_kia_tail_copy(raw);
    emit_merge(false, 5000);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
}

void test_kia_v0_short_preamble_rejected(void)
{
    const uint64_t raw = build_kia_key(0x0123456u, 0x02u, 0x1234u);
    emit_short_pairs(10); /* below the >14 pair threshold */
    emit_data_msb(raw, 61);
    emit_merge(true, 1000);
    emit_merge(false, 5000);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
}

/*--------------------------------------------------------------------*/
/* Suzuki (type 2)                                                     */
/*--------------------------------------------------------------------*/

void test_kia_v0_suzuki_first_copy_decodes_at_packet_end(void)
{
    const uint64_t shifted = build_suzuki_key(0x0BADC0Du, 0x03u, 0x4711u);

    emit_suzuki_first_copy(shifted);
    emit_merge(false, 2000); /* merges into the last data low (2250/2500) */
    /* M1 splits here: the high 2000 lands in the next capture buffer. */

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x0BADC0Du, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x4711u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x03u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(64, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT8(TYPE_SUZUKI,
                            (uint8_t)(subghz_decenc_ctl.n32_key2 >> 16));
}

void test_kia_v0_suzuki_full_frame_commits_on_double_gap(void)
{
    const uint64_t shifted = build_suzuki_key(0x0765432u, 0x04u, 0xABCDu);

    emit_suzuki_first_copy(shifted);
    emit_merge(false, 2000);  /* merged separator low  */
    emit_merge(true, 2000);   /* separator high: commit here, mid-buffer */
    emit_merge(false, 250);
    emit_short_pairs(15);
    emit_data_msb(shifted, 64);
    emit_merge(false, 8000);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(0x0765432u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0xABCDu, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(TYPE_SUZUKI,
                            (uint8_t)(subghz_decenc_ctl.n32_key2 >> 16));
}

void test_kia_v0_suzuki_bad_crc_rejected(void)
{
    const uint64_t shifted = build_suzuki_key(0x0BADC0Du, 0x03u, 0x4711u) ^ 0x10u;
    emit_suzuki_first_copy(shifted);
    emit_merge(false, 2000);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
}

/*--------------------------------------------------------------------*/
/* Honda V0 (type 3)                                                   */
/*--------------------------------------------------------------------*/

void test_kia_v0_honda_frame_decodes(void)
{
    const uint64_t key = build_honda_key(0x0A1B2Cu, 0x01u, 0x4321u);
    const uint64_t wire = m1_kia_v0_honda_transform(key); /* self-inverse */

    emit_short_pairs(40);
    emit_long_pairs(4);
    emit_data_msb(wire, 56);
    emit_merge(true, 1000);
    emit_merge(false, 5000);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(key, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(0x0A1B2Cu, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(0x4321u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(0x01u, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(72, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT8(TYPE_HONDA,
                            (uint8_t)(subghz_decenc_ctl.n32_key2 >> 16));
}

void test_kia_v0_honda_bad_header_rejected(void)
{
    /* Flip header bit 0 (0x?A -> 0x?B): fails the structural nibble gate. */
    const uint64_t key = build_honda_key(0x0A1B2Cu, 0x01u, 0x4321u) ^ 0x100u;
    const uint64_t wire = m1_kia_v0_honda_transform(key);

    emit_short_pairs(40);
    emit_long_pairs(4);
    emit_data_msb(wire, 56);
    emit_merge(true, 1000);
    emit_merge(false, 5000);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
}

/*--------------------------------------------------------------------*/
/* Mitsubishi (type 4): sticky previous-copy key + 72..88 pair tail    */
/*--------------------------------------------------------------------*/

void test_kia_v0_mitsubishi_identified_across_buffers(void)
{
    const uint64_t raw = build_kia_key(0x00556677u & 0x0FFFFFFFu, 0x02u, 0x7777u);

    /* First copy: commits as plain Kia and seeds the sticky cache. */
    emit_kia_first_copy(raw);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT8(TYPE_KIA,
                            (uint8_t)(subghz_decenc_ctl.n32_key2 >> 16));

    /* Second capture buffer: 80-pair tail preamble with the same key. */
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
    s_pulse_count = 0;
    emit_mitsu_tail_copy(raw);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT8(TYPE_MITSU,
                            (uint8_t)(subghz_decenc_ctl.n32_key2 >> 16));
    TEST_ASSERT_EQUAL_UINT32(0x00556677u, subghz_decenc_ctl.n32_serialnumber);
}

void test_kia_v0_kia_tail_not_misread_as_mitsubishi(void)
{
    const uint64_t raw = build_kia_key(0x0123456u, 0x02u, 0x1234u);

    emit_kia_first_copy(raw);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));

    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
    s_pulse_count = 0;
    emit_kia_tail_copy(raw); /* 15 pairs: outside the 72..88 window */
    emit_merge(false, 5000);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v0(KIA_V0, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT8(TYPE_KIA,
                            (uint8_t)(subghz_decenc_ctl.n32_key2 >> 16));
}

/*--------------------------------------------------------------------*/
/* Pure helpers                                                        */
/*--------------------------------------------------------------------*/

void test_kia_v0_pure_helpers(void)
{
    const uint64_t raw = build_kia_key(0x0123456u, 0x02u, 0x1234u);
    TEST_ASSERT_TRUE(m1_kia_v0_kia_crc_valid(raw));
    TEST_ASSERT_EQUAL_UINT8(raw & 0xFFu, m1_kia_v0_wire_crc(raw));
    TEST_ASSERT_FALSE(m1_kia_v0_kia_crc_valid(raw ^ 0x01u));

    const uint64_t shifted = build_suzuki_key(0x0BADC0Du, 0x03u, 0x4711u);
    uint64_t resolved = 0;
    /* The decoder's sync pre-load shifts the wire word right by one with
     * two preloaded 1 bits; the <<1 candidate must restore it (bit 0 of
     * the encoder word is always 0). */
    const uint64_t collected = (3uLL << 62) | ((shifted >> 1) & 0x3FFFFFFFFFFFFFFFuLL);
    TEST_ASSERT_TRUE(m1_kia_v0_suzuki_resolve(collected, &resolved));
    TEST_ASSERT_EQUAL_UINT64(shifted, resolved);
    TEST_ASSERT_FALSE(m1_kia_v0_suzuki_resolve(collected ^ 0x10u, &resolved));

    /* Honda transform is an involution (per-byte bit reversal). */
    const uint64_t key = build_honda_key(0x0A1B2Cu, 0x01u, 0x4321u);
    TEST_ASSERT_EQUAL_UINT64(key, m1_kia_v0_honda_transform(m1_kia_v0_honda_transform(key)));
    TEST_ASSERT_TRUE(m1_kia_v0_honda_key_valid(key));
    TEST_ASSERT_FALSE(m1_kia_v0_honda_key_valid(key ^ 0x100u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kia_v0_kia_full_frame_decodes);
    RUN_TEST(test_kia_v0_kia_first_copy_alone_decodes);
    RUN_TEST(test_kia_v0_kia_tail_copy_alone_decodes);
    RUN_TEST(test_kia_v0_kia_bad_crc_rejected);
    RUN_TEST(test_kia_v0_short_preamble_rejected);
    RUN_TEST(test_kia_v0_suzuki_first_copy_decodes_at_packet_end);
    RUN_TEST(test_kia_v0_suzuki_full_frame_commits_on_double_gap);
    RUN_TEST(test_kia_v0_suzuki_bad_crc_rejected);
    RUN_TEST(test_kia_v0_honda_frame_decodes);
    RUN_TEST(test_kia_v0_honda_bad_header_rejected);
    RUN_TEST(test_kia_v0_mitsubishi_identified_across_buffers);
    RUN_TEST(test_kia_v0_kia_tail_not_misread_as_mitsubishi);
    RUN_TEST(test_kia_v0_pure_helpers);
    return UNITY_END();
}
