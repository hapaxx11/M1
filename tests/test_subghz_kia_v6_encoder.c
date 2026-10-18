/* See COPYING.txt for license details. */

/*
 * Host tests for the Kia V6 encoder (TX path).
 *
 * The central check is a full round-trip through the real decoder: an
 * encrypted frame is turned into an upload waveform, merged into pulse
 * durations, and decoded by subghz_decode_kia_v6() — fields must come back
 * intact.  No real key material is used.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_kia_v6_decode.h"
#include "subghz_kia_v6_encoder.h"
#include "subghz_protopirate_keys_builtin.h"

/* Synthetic keystore (test-local, mirrors test_kia_v6_decode.c). */
const uint64_t m1_kia_v6_keys_builtin[M1_KIA_V6_KEY_COUNT] = {
    0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL,
};
const bool m1_kia_v6_keys_builtin_available = true;
const uint8_t m1_vag_aut64_keys_builtin[M1_VAG_AUT64_KEY_BYTES] = {0};
const bool m1_vag_aut64_keys_builtin_available = false;

void setUp(void) { memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl)); }
void tearDown(void) {}

/*============================================================================*/
/* AES S-box                                                                   */
/*============================================================================*/

void test_aes_sbox_matches_canonical_vectors(void)
{
    /* Canonical AES S-box values. */
    TEST_ASSERT_EQUAL_HEX8(0x63u, subghz_kia_v6_aes_sbox(0x00u));
    TEST_ASSERT_EQUAL_HEX8(0x7Cu, subghz_kia_v6_aes_sbox(0x01u));
    TEST_ASSERT_EQUAL_HEX8(0xEDu, subghz_kia_v6_aes_sbox(0x53u));
    TEST_ASSERT_EQUAL_HEX8(0x16u, subghz_kia_v6_aes_sbox(0xFFu));
    TEST_ASSERT_EQUAL_HEX8(0x00u, subghz_kia_v6_aes_sbox(0x52u));
}

void test_aes_key_layout_matches_decoder(void)
{
    uint8_t enc_key[16];
    subghz_kia_v6_aes_key(m1_kia_v6_keys_builtin, enc_key);

    /* Same derivation as the decoder (m1_kia_v6_decode.c). */
    uint8_t dec_key[16];
    for (uint8_t part = 0; part < 2u; part++) {
        const uint32_t high =
            (uint32_t)(m1_kia_v6_keys_builtin[part] >> 32) ^ 0x638766ABu;
        const uint32_t low =
            (uint32_t)m1_kia_v6_keys_builtin[part] ^ 0x84AF25FBu;
        for (uint8_t i = 0; i < 4u; i++) {
            dec_key[part * 8u + i] = (uint8_t)(high >> (24u - 8u * i));
            dec_key[part * 8u + 4u + i] = (uint8_t)(low >> (24u - 8u * i));
        }
    }
    TEST_ASSERT_EQUAL_UINT8_ARRAY(dec_key, enc_key, 16);
}

/*============================================================================*/
/* Upload builder                                                              */
/*============================================================================*/

void test_upload_shape_and_timings(void)
{
    static subghz_upload_entry_t up[KIA_V6_ENC_UPLOAD_SIZE];
    uint8_t frame[18];
    subghz_kia_v6_encrypt_frame(0x34u, 0x123456u, 0x2u, 0x01020304u,
                                m1_kia_v6_keys_builtin, frame);

    const uint32_t n =
        subghz_kia_v6_build_upload(up, KIA_V6_ENC_UPLOAD_SIZE, frame);
    TEST_ASSERT_EQUAL_UINT32(KIA_V6_ENC_UPLOAD_SIZE, n);

    /* First message preamble: 640 mark/space pairs at 200 µs. */
    for (uint32_t i = 0; i < 1280u; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(200u, up[i].duration);
        TEST_ASSERT_EQUAL(i % 2u == 0u, up[i].level);
    }

    /* Sync: low 200, high 400, low 200. */
    TEST_ASSERT_EQUAL_UINT32(200u, up[1280].duration);
    TEST_ASSERT_FALSE(up[1280].level);
    TEST_ASSERT_EQUAL_UINT32(400u, up[1281].duration);
    TEST_ASSERT_TRUE(up[1281].level);
    TEST_ASSERT_EQUAL_UINT32(200u, up[1282].duration);
    TEST_ASSERT_FALSE(up[1282].level);

    /* First data bit: frame[0] = 0x23 → part1_hi bit 28 = 0 → inverted to
     * 1 → (space, mark). */
    TEST_ASSERT_FALSE(up[1283].level);
    TEST_ASSERT_TRUE(up[1284].level);
    TEST_ASSERT_EQUAL_UINT32(200u, up[1283].duration);

    /* Separator between the two messages: 400 µs low. */
    const uint32_t msg1_end = 1280u + 3u + (61u + 64u + 16u) * 2u;
    TEST_ASSERT_EQUAL_UINT32(400u, up[msg1_end].duration);
    TEST_ASSERT_FALSE(up[msg1_end].level);

    /* Second message preamble: 38 pairs. */
    for (uint32_t i = 0; i < 76u; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(200u, up[msg1_end + 1u + i].duration);
    }

    /* Final entry: 400 µs low. */
    TEST_ASSERT_EQUAL_UINT32(400u, up[n - 1u].duration);
    TEST_ASSERT_FALSE(up[n - 1u].level);
}

void test_upload_capacity_guard(void)
{
    static subghz_upload_entry_t up[KIA_V6_ENC_UPLOAD_SIZE];
    uint8_t frame[18] = {0};
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_kia_v6_build_upload(up, KIA_V6_ENC_UPLOAD_SIZE - 1u, frame));
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_kia_v6_build_upload(NULL, KIA_V6_ENC_UPLOAD_SIZE, frame));
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_kia_v6_build_upload(up, KIA_V6_ENC_UPLOAD_SIZE, NULL));
}

/*============================================================================*/
/* Full round-trip: encrypt → upload → merge → real decoder                    */
/*============================================================================*/

static bool append_pulse(bool level, uint16_t duration, uint16_t *count,
                         bool *have_level, bool *last_level)
{
    if (*have_level && *last_level == level) {
        subghz_decenc_ctl.pulse_times[*count - 1u] += duration;
    } else {
        if (*count >= PACKET_PULSE_COUNT_MAX)
            return false;
        subghz_decenc_ctl.pulse_times[(*count)++] = duration;
    }
    *have_level = true;
    *last_level = level;
    return true;
}

void test_encrypt_upload_decode_roundtrip(void)
{
    const uint8_t  fx = 0x34u;
    const uint32_t serial = 0x123456u;
    const uint8_t  btn = 0x2u;
    const uint32_t cnt = 0x003CF089u;

    uint8_t frame[18];
    subghz_kia_v6_encrypt_frame(fx, serial, btn, cnt, m1_kia_v6_keys_builtin,
                                frame);

    static subghz_upload_entry_t up[KIA_V6_ENC_UPLOAD_SIZE];
    const uint32_t n =
        subghz_kia_v6_build_upload(up, KIA_V6_ENC_UPLOAD_SIZE, frame);
    TEST_ASSERT_EQUAL_UINT32(KIA_V6_ENC_UPLOAD_SIZE, n);

    /* Merge the (level, duration) list into alternating pulse durations,
     * same convention as the capture ISR (durations only, levels implicit).
     * Feed only the second (38-pair-preamble) message, excluding the
     * trailing 400 µs separator: the full two-message burst exceeds the
     * decoder's PACKET_PULSE_COUNT_MAX window, and a trailing separator can
     * merge with a low final data pulse into an unclassifiable 600 µs pulse
     * (on real hardware the packetizer's gap detector likewise ends the
     * packet before it). */
    const uint32_t msg2_start = 1280u + 3u + (61u + 64u + 16u) * 2u + 1u;
    uint16_t count = 0;
    bool have_level = false, last_level = false;
    for (uint32_t i = msg2_start; i + 1u < n; i++)
    {
        TEST_ASSERT_TRUE(append_pulse(up[i].level,
                                      (uint16_t)up[i].duration, &count,
                                      &have_level, &last_level));
    }

    TEST_ASSERT_EQUAL_UINT8(0u, subghz_decode_kia_v6(KIA_V6, count));
    TEST_ASSERT_EQUAL_HEX32(serial, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX32(cnt, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_HEX8(btn, subghz_decenc_ctl.n8_buttonid);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_aes_sbox_matches_canonical_vectors);
    RUN_TEST(test_aes_key_layout_matches_decoder);
    RUN_TEST(test_upload_shape_and_timings);
    RUN_TEST(test_upload_capacity_guard);
    RUN_TEST(test_encrypt_upload_decode_roundtrip);
    return UNITY_END();
}
