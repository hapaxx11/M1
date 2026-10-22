/* See COPYING.txt for license details. */

/*
 * Host tests for the M1 ProtoPirate "Kia V3" / "Kia V4" decoders
 * (Sub_Ghz/protocols/m1_kia_v3_v4_decode.c shared core plus the
 * m1_kia_v3_decode.c / m1_kia_v4_decode.c polarity wrappers).
 *
 * Waveforms are synthesised test-side from the upstream ProtoPirate
 * kia_v3_v4.c wire format (te 400/800, 12 preamble pairs, sync 400+1200,
 * 64 PWM data bits LSB-first on air + 4 CRC bits, end marker 800+800).
 * The KeeLoq hop word is encrypted with a synthetic manufacturer key via
 * the same Sub_Ghz/subghz_keeloq.c primitive the decoder links against,
 * giving a full encrypt -> wire -> decrypt round trip.
 *
 * This target does not link the keystore stub; the two Kia MF-key symbols
 * are defined here with synthetic key material.
 */

#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_keeloq.h"
#include "subghz_protopirate_keys_builtin.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

uint8_t subghz_decode_kia_v3(uint16_t protocol_index, uint16_t pulse_count);
uint8_t subghz_decode_kia_v4(uint16_t protocol_index, uint16_t pulse_count);

/* Pure parser exported by the shared core */
bool m1_kia_v3_v4_parse_frame(const uint8_t raw_bits[9], uint16_t raw_bit_count,
                              uint8_t version, uint64_t mf_key, bool key_available,
                              uint64_t *raw, uint32_t *serial,
                              uint8_t *button, uint16_t *counter);

/* Synthetic keystore symbols (this target does not link
 * tests/stubs/protopirate_keys_test_data.c). */
#define TEST_MF_KEY 0x0102030405060708ULL
const uint64_t m1_kia_mf_key_builtin = TEST_MF_KEY;
const bool m1_kia_mf_key_builtin_available = true;

#define TE_SHORT 400u
#define TE_LONG  800u
#define SYNC_US  1200u
#define END_US   800u
#define PREAMBLE_PAIRS 12u
#define DATA_BITS 64u

#define VERSION_V4 0u /* upstream: version 0 = V4 polarity */
#define VERSION_V3 1u /* upstream: version 1 = V3 polarity */

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
/* Test-side field -> wire builders (mirror of the upstream encoder    */
/* layout in kia_v3_v4.c)                                              */
/*--------------------------------------------------------------------*/

static uint8_t rev8(uint8_t x)
{
    x = (uint8_t)(((x & 0xF0u) >> 4) | ((x & 0x0Fu) << 4));
    x = (uint8_t)(((x & 0xCCu) >> 2) | ((x & 0x33u) << 2));
    x = (uint8_t)(((x & 0xAAu) >> 1) | ((x & 0x55u) << 1));
    return x;
}

/* Plaintext 0xBSSSCCCC: button nibble, serial & 0x3FF, 16-bit counter. */
static uint32_t kia_plaintext(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    return (uint32_t)cnt | ((serial & 0x3FFu) << 16) |
           ((uint32_t)(btn & 0x0Fu) << 28);
}

static uint32_t serial_btn_word(uint32_t serial, uint8_t btn)
{
    return (serial & 0x0FFFFFFFu) | ((uint32_t)(btn & 0x0Fu) << 28);
}

/* Collected on-air bytes b[0..8] exactly as the decoder packs them. */
static void build_wire_bytes(uint32_t encrypted, uint32_t serial, uint8_t btn,
                             uint8_t crc, uint8_t out[9])
{
    const uint32_t sb = serial_btn_word(serial, btn);
    out[0] = rev8((uint8_t)(encrypted & 0xFFu));
    out[1] = rev8((uint8_t)((encrypted >> 8) & 0xFFu));
    out[2] = rev8((uint8_t)((encrypted >> 16) & 0xFFu));
    out[3] = rev8((uint8_t)((encrypted >> 24) & 0xFFu));
    out[4] = rev8((uint8_t)(sb & 0xFFu));
    out[5] = rev8((uint8_t)((sb >> 8) & 0xFFu));
    out[6] = rev8((uint8_t)((sb >> 16) & 0xFFu));
    out[7] = rev8((uint8_t)((sb >> 24) & 0xFFu));
    out[8] = (uint8_t)((crc & 0x0Fu) << 4);
}

static uint64_t wire_word(const uint8_t b[9])
{
    uint64_t word = 0;
    for (uint8_t i = 0; i < 8u; i++)
        word = (word << 8) | b[i];
    return word;
}

static void emit_bit_pwm(bool bit, bool v4)
{
    const uint32_t first_us = bit ? TE_SHORT : TE_LONG;
    const uint32_t second_us = bit ? TE_LONG : TE_SHORT;
    if (v4)
    {
        emit_merge(false, first_us);
        emit_merge(true, second_us);
    }
    else
    {
        emit_merge(true, first_us);
        emit_merge(false, second_us);
    }
}

/* Emit one burst in the upstream encoder's layout.  data_bits may be
 * below 64 to synthesise truncated frames. */
static void emit_kia_frame(uint32_t encrypted, uint32_t serial, uint8_t btn,
                           uint8_t crc, bool v4, uint32_t preamble_pairs,
                           uint32_t sync_us, uint32_t data_bits)
{
    const uint64_t key =
        ((uint64_t)serial_btn_word(serial, btn) << 32) | encrypted;
    /* Reverse the full 64-bit key (LSB-first on air). */
    uint64_t tx = 0;
    for (int i = 0; i < 64; i++)
        tx = (tx << 1) | ((key >> i) & 1u);

    for (uint32_t p = 0; p < preamble_pairs; p++)
    {
        if (v4)
        {
            emit_merge(false, TE_SHORT);
            emit_merge(true, TE_SHORT);
        }
        else
        {
            emit_merge(true, TE_SHORT);
            emit_merge(false, TE_SHORT);
        }
    }
    if (v4)
    {
        emit_merge(false, TE_SHORT);
        emit_merge(true, sync_us);
    }
    else
    {
        emit_merge(true, TE_SHORT);
        emit_merge(false, sync_us);
    }

    for (int i = 63; i >= 64 - (int)data_bits; i--)
        emit_bit_pwm((bool)((tx >> i) & 1u), v4);
    if (data_bits == DATA_BITS)
        for (int b = 3; b >= 0; b--)
            emit_bit_pwm((bool)((crc >> b) & 1u), v4);

    if (v4)
    {
        emit_merge(false, END_US);
        emit_merge(true, END_US);
    }
    else
    {
        emit_merge(true, END_US);
        emit_merge(false, END_US);
    }
}

static uint32_t encrypt_frame(uint32_t serial, uint8_t btn, uint16_t cnt)
{
    return keeloq_encrypt(kia_plaintext(serial, btn, cnt), TEST_MF_KEY);
}

/*--------------------------------------------------------------------*/
/* V4 polarity                                                         */
/*--------------------------------------------------------------------*/

void test_kia_v4_frame_commits_with_mf_key(void)
{
    const uint32_t serial = 0x00ABCDE1u;
    const uint8_t btn = 0x02u;
    const uint16_t cnt = 0x1234u;
    const uint32_t encrypted = encrypt_frame(serial, btn, cnt);

    uint8_t b[9];
    build_wire_bytes(encrypted, serial, btn, 0x05u, b);

    emit_kia_frame(encrypted, serial, btn, 0x05u, true,
                   PREAMBLE_PAIRS, SYNC_US, DATA_BITS);
    emit_merge(false, 5000); /* inter-burst silence ends the capture */
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v4(KIA_V4, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(wire_word(b), subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(serial, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(cnt, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(btn, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(68, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodeddelay);
    TEST_ASSERT_EQUAL_UINT16(KIA_V4, subghz_decenc_ctl.ndecodedprotocol);
}

void test_kia_v4_frame_rejected_by_v3_wrapper(void)
{
    const uint32_t encrypted = encrypt_frame(0x00123456u, 0x01u, 0xBEEFu);

    emit_kia_frame(encrypted, 0x00123456u, 0x01u, 0x0Au, true,
                   PREAMBLE_PAIRS, SYNC_US, DATA_BITS);
    emit_merge(false, 5000);

    /* Polarity filter: the "Kia V3/V4" (KIA_V3) entry must not claim a
     * V4 frame. */
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v3(KIA_V3, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodedprotocol);
}

void test_kia_v4_packet_end_commit(void)
{
    const uint32_t serial = 0x00004242u;
    const uint8_t btn = 0x04u;
    const uint16_t cnt = 0x4711u;
    const uint32_t encrypted = encrypt_frame(serial, btn, cnt);

    /* No trailing gap: M1 may split it off; the buffer simply ends. */
    emit_kia_frame(encrypted, serial, btn, 0x00u, true,
                   PREAMBLE_PAIRS, SYNC_US, DATA_BITS);

    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v4(KIA_V4, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT32(serial, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(cnt, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(btn, subghz_decenc_ctl.n8_buttonid);
}

/*--------------------------------------------------------------------*/
/* V3 polarity                                                         */
/*--------------------------------------------------------------------*/

void test_kia_v3_frame_commits_with_mf_key(void)
{
    const uint32_t serial = 0x00556677u;
    const uint8_t btn = 0x03u;
    const uint16_t cnt = 0xABCDu;
    const uint32_t encrypted = encrypt_frame(serial, btn, cnt);

    uint8_t b[9];
    build_wire_bytes(encrypted, serial, btn, 0x0Fu, b);

    emit_kia_frame(encrypted, serial, btn, 0x0Fu, false,
                   PREAMBLE_PAIRS, SYNC_US, DATA_BITS);
    emit_merge(false, 5000);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(PACKET_PULSE_COUNT_MAX, s_pulse_count);

    /* V3 bytes are collected inverted and re-inverted by the parser, so
     * the committed raw word matches the V4 layout. */
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_kia_v3(KIA_V3, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT64(wire_word(b), subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_UINT32(serial, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_UINT32(cnt, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_UINT8(btn, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_UINT16(68, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(KIA_V3, subghz_decenc_ctl.ndecodedprotocol);
}

void test_kia_v3_frame_rejected_by_v4_wrapper(void)
{
    const uint32_t encrypted = encrypt_frame(0x00556677u, 0x03u, 0xABCDu);

    emit_kia_frame(encrypted, 0x00556677u, 0x03u, 0x0Fu, false,
                   PREAMBLE_PAIRS, SYNC_US, DATA_BITS);
    emit_merge(false, 5000);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v4(KIA_V4, s_pulse_count));
    TEST_ASSERT_EQUAL_UINT16(0, subghz_decenc_ctl.ndecodedprotocol);
}

/*--------------------------------------------------------------------*/
/* Decrypt validation policy (upstream: mismatch -> reject, no commit) */
/*--------------------------------------------------------------------*/

void test_kia_v4_decrypt_mismatch_rejected(void)
{
    const uint32_t serial = 0x00ABCDE1u;
    const uint16_t cnt = 0x1234u;
    /* Plaintext carries button 0x2 but the on-air serial/button field
     * carries 0x3: decryption validates structurally but the plaintext
     * cross-check fails. */
    const uint32_t encrypted = keeloq_encrypt(kia_plaintext(serial, 0x02u, cnt),
                                              TEST_MF_KEY);

    emit_kia_frame(encrypted, serial, 0x03u, 0x05u, true,
                   PREAMBLE_PAIRS, SYNC_US, DATA_BITS);
    emit_merge(false, 5000);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v4(KIA_V4, s_pulse_count));
}

/*--------------------------------------------------------------------*/
/* Pure parser gating branches                                         */
/*--------------------------------------------------------------------*/

void test_kia_v3_v4_parse_wrong_key_rejected(void)
{
    const uint32_t serial = 0x00ABCDE1u;
    const uint8_t btn = 0x02u;
    const uint32_t encrypted = encrypt_frame(serial, btn, 0x1234u);
    const uint64_t wrong_key = 0x1122334455667788ULL;

    uint8_t b[9];
    build_wire_bytes(encrypted, serial, btn, 0x05u, b);

    /* Precondition: the wrong key must actually fail the plaintext
     * cross-check, otherwise this test proves nothing. */
    const uint32_t dec = keeloq_decrypt(encrypted, wrong_key);
    TEST_ASSERT_TRUE(((dec >> 28) & 0x0Fu) != btn ||
                     ((dec >> 16) & 0xFFu) != (serial & 0xFFu));

    uint64_t raw = 0;
    uint32_t ser = 0;
    uint8_t button = 0;
    uint16_t counter = 0;
    TEST_ASSERT_FALSE(m1_kia_v3_v4_parse_frame(b, 68, VERSION_V4, wrong_key,
                                               true, &raw, &ser, &button,
                                               &counter));
}

void test_kia_v3_v4_parse_key_unavailable_commits_zeroed(void)
{
    const uint32_t serial = 0x00ABCDE1u;
    const uint8_t btn = 0x02u;
    const uint32_t encrypted = encrypt_frame(serial, btn, 0x1234u);

    uint8_t b[9];
    build_wire_bytes(encrypted, serial, btn, 0x05u, b);

    uint64_t raw = 0;
    uint32_t ser = 0xFFFFFFFFu;
    uint8_t button = 0xFFu;
    uint16_t counter = 0xFFFFu;
    TEST_ASSERT_TRUE(m1_kia_v3_v4_parse_frame(b, 68, VERSION_V4, 0, false,
                                              &raw, &ser, &button, &counter));
    TEST_ASSERT_EQUAL_UINT64(wire_word(b), raw);
    TEST_ASSERT_EQUAL_UINT32(0, ser);
    TEST_ASSERT_EQUAL_UINT8(0, button);
    TEST_ASSERT_EQUAL_UINT16(0, counter);

    /* V3: the parser inverts the collected bytes before parsing. */
    uint8_t inv[9];
    for (uint8_t i = 0; i < 9u; i++)
        inv[i] = (uint8_t)~b[i];
    ser = 0xFFFFFFFFu;
    button = 0xFFu;
    counter = 0xFFFFu;
    TEST_ASSERT_TRUE(m1_kia_v3_v4_parse_frame(inv, 68, VERSION_V3, 0, false,
                                              &raw, &ser, &button, &counter));
    TEST_ASSERT_EQUAL_UINT64(wire_word(b), raw);
    TEST_ASSERT_EQUAL_UINT32(0, ser);

    /* And with the key available the V3 inversion recovers the fields. */
    TEST_ASSERT_TRUE(m1_kia_v3_v4_parse_frame(inv, 68, VERSION_V3, TEST_MF_KEY,
                                              true, &raw, &ser, &button,
                                              &counter));
    TEST_ASSERT_EQUAL_UINT32(serial, ser);
    TEST_ASSERT_EQUAL_UINT8(btn, button);
    TEST_ASSERT_EQUAL_UINT16(0x1234u, counter);

    /* Fewer than 68 collected bits never parses. */
    TEST_ASSERT_FALSE(m1_kia_v3_v4_parse_frame(b, 67, VERSION_V4, TEST_MF_KEY,
                                               true, &raw, &ser, &button,
                                               &counter));
}

/*--------------------------------------------------------------------*/
/* Structural negatives                                                */
/*--------------------------------------------------------------------*/

void test_kia_v4_short_preamble_rejected(void)
{
    const uint32_t encrypted = encrypt_frame(0x00ABCDE1u, 0x02u, 0x1234u);

    emit_kia_frame(encrypted, 0x00ABCDE1u, 0x02u, 0x05u, true,
                   4u /* below the upstream >= 8 preamble threshold */,
                   SYNC_US, DATA_BITS);
    emit_merge(false, 5000);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v4(KIA_V4, s_pulse_count));
}

void test_kia_v4_bad_sync_rejected(void)
{
    const uint32_t encrypted = encrypt_frame(0x00ABCDE1u, 0x02u, 0x1234u);

    /* Sync long pulse 2000 us: outside the upstream 1000..1500 window. */
    emit_kia_frame(encrypted, 0x00ABCDE1u, 0x02u, 0x05u, true,
                   PREAMBLE_PAIRS, 2000u, DATA_BITS);
    emit_merge(false, 5000);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v4(KIA_V4, s_pulse_count));
}

void test_kia_v4_truncated_data_rejected(void)
{
    const uint32_t encrypted = encrypt_frame(0x00ABCDE1u, 0x02u, 0x1234u);

    emit_kia_frame(encrypted, 0x00ABCDE1u, 0x02u, 0x05u, true,
                   PREAMBLE_PAIRS, SYNC_US, 30u);
    emit_merge(false, 5000);

    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_kia_v4(KIA_V4, s_pulse_count));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kia_v4_frame_commits_with_mf_key);
    RUN_TEST(test_kia_v4_frame_rejected_by_v3_wrapper);
    RUN_TEST(test_kia_v4_packet_end_commit);
    RUN_TEST(test_kia_v3_frame_commits_with_mf_key);
    RUN_TEST(test_kia_v3_frame_rejected_by_v4_wrapper);
    RUN_TEST(test_kia_v4_decrypt_mismatch_rejected);
    RUN_TEST(test_kia_v3_v4_parse_wrong_key_rejected);
    RUN_TEST(test_kia_v3_v4_parse_key_unavailable_commits_zeroed);
    RUN_TEST(test_kia_v4_short_preamble_rejected);
    RUN_TEST(test_kia_v4_bad_sync_rejected);
    RUN_TEST(test_kia_v4_truncated_data_rejected);
    return UNITY_END();
}
