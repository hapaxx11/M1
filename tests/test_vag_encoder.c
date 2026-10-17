/* See COPYING.txt for license details. */

/*
 * Host tests for the VAG encoder (TX path) and the AUT64 encrypt port.
 *
 * AUT64 encrypt is validated against M1's decrypt (known to match the
 * reference) via round-trip and against the existing decode test vector.
 * The type-2 path is additionally validated end-to-end through the real
 * subghz_decode_vag() waveform decoder.  All key material is synthetic.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_aut64.h"
#include "m1_vag_decode.h"
#include "subghz_vag_encoder.h"

uint8_t subghz_decode_vag(uint16_t p, uint16_t pulsecount);

void setUp(void) { memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl)); }
void tearDown(void) {}

/* Synthetic AUT64 key (same record as test_vag_decode.c). */
static const uint8_t PACKED_KEY[16] = {
    0x01, 0x12, 0x34, 0x56, 0x78, 0x05, 0x39, 0x77,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
};

/*============================================================================*/
/* AUT64 encrypt                                                               */
/*============================================================================*/

void test_aut64_encrypt_decrypt_roundtrip(void)
{
    m1_aut64_key_t key;
    TEST_ASSERT_TRUE(m1_aut64_unpack(&key, PACKED_KEY));

    const uint8_t plain[8] = {0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x10};
    uint8_t block[8];
    memcpy(block, plain, 8);
    m1_aut64_encrypt(&key, block);
    TEST_ASSERT_TRUE(memcmp(block, plain, 8) != 0);  /* actually encrypted */

    m1_aut64_decrypt(&key, block);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(plain, block, 8);
}

void test_aut64_encrypt_matches_existing_decode_vector(void)
{
    /* Existing decode vector (test_vag_decode.c): ciphertext
     * {12 34 56 78 12 34 56 4E} decrypts under PACKED_KEY to serial
     * 0x12345678, counter 0x563412 and a top-nibble-1 button byte.
     * Re-encrypting the recovered plaintext must reproduce the ciphertext. */
    m1_aut64_key_t key;
    TEST_ASSERT_TRUE(m1_aut64_unpack(&key, PACKED_KEY));

    uint8_t block[8] = {0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x4E};
    m1_aut64_decrypt(&key, block);  /* recover the exact plaintext */

    uint8_t reenc[8];
    memcpy(reenc, block, 8);
    m1_aut64_encrypt(&key, reenc);
    const uint8_t want[8] = {0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x4E};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, reenc, 8);
}

/*============================================================================*/
/* Field mapping                                                               */
/*============================================================================*/

void test_button_and_dispatch_bytes(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x10u, subghz_vag_button_byte(1u));
    TEST_ASSERT_EQUAL_HEX8(0x20u, subghz_vag_button_byte(2u));
    TEST_ASSERT_EQUAL_HEX8(0x40u, subghz_vag_button_byte(4u));

    TEST_ASSERT_EQUAL_HEX8(0x1Cu, subghz_vag_dispatch_byte(0x10u, 1u));
    TEST_ASSERT_EQUAL_HEX8(0x2Au, subghz_vag_dispatch_byte(0x20u, 1u));
    TEST_ASSERT_EQUAL_HEX8(0x46u, subghz_vag_dispatch_byte(0x40u, 2u));
    TEST_ASSERT_EQUAL_HEX8(0x1Du, subghz_vag_dispatch_byte(0x10u, 3u));
    TEST_ASSERT_EQUAL_HEX8(0x2Bu, subghz_vag_dispatch_byte(0x20u, 4u));
    TEST_ASSERT_EQUAL_HEX8(0x47u, subghz_vag_dispatch_byte(0x40u, 4u));
}

/*============================================================================*/
/* Type 2 (TEA) — full chain through the real waveform decoder                 */
/*============================================================================*/

void test_type2_encrypt_upload_decode_roundtrip(void)
{
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    TEST_ASSERT_TRUE(subghz_vag_encrypt_frame(
        0x12345678u, 0x005634u, 1u /*unlock*/, 2u, 0xC0u,
        VAG_ENC_KEY_AUTO, 0, 0, NULL, false, &key1, &key2));
    TEST_ASSERT_EQUAL_HEX8(0xC0u, (uint8_t)(key1 >> 56));

    static subghz_upload_entry_t up[VAG_ENC_UPLOAD_T12];
    const uint32_t n =
        subghz_vag_build_upload(up, VAG_ENC_UPLOAD_T12, 2u, key1, key2);
    TEST_ASSERT_EQUAL_UINT32(VAG_ENC_UPLOAD_T12, n);

    /* Upload entries → merged duration pulses (capture convention).  Feed
     * only the last 20 preamble pairs onward (the full 220-pair wakeup
     * preamble exceeds the decoder's PACKET_PULSE_COUNT_MAX window) and
     * exclude the trailing 6000 µs gap: on air it merges with the final
     * space, but the capture packetizer closes the packet at the gap
     * threshold without stretching the last data pulse. */
    uint16_t count = 0;
    bool have = false, last = false;
    for (uint32_t i = 400u; i + 1u < n; i++)
    {
        if (have && last == up[i].level)
            subghz_decenc_ctl.pulse_times[count - 1] += (uint16_t)up[i].duration;
        else
        {
            TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, count);
            subghz_decenc_ctl.pulse_times[count++] = (uint16_t)up[i].duration;
        }
        have = true;
        last = up[i].level;
    }

    TEST_ASSERT_EQUAL_UINT8(0u, subghz_decode_vag(VAG, count));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX32(0x005634u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_HEX8(1u, subghz_decenc_ctl.n8_buttonid);
}

void test_type1_passat_converts_to_type2(void)
{
    /* Type byte 0x00 (Passat) on a type-1 input must go out as type 2
     * (TEA schedule), so no AUT64 keystore is needed. */
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    TEST_ASSERT_TRUE(subghz_vag_encrypt_frame(
        0x00ABCDEFu, 7u, 2u /*lock*/, 1u, 0x00u,
        VAG_ENC_KEY_AUTO, 0, 0, NULL, false, &key1, &key2));
    TEST_ASSERT_EQUAL_HEX8(0x00u, (uint8_t)(key1 >> 56));
    TEST_ASSERT_EQUAL_HEX8(0x2Au, (uint8_t)(key2 & 0xFFu));  /* lock dispatch */
}

/*============================================================================*/
/* Type 1 (AUT64, synthetic keystore)                                          */
/*============================================================================*/

void test_type1_encrypt_decrypts_back(void)
{
    uint8_t keys48[48] = {0};
    memcpy(keys48, PACKED_KEY, 16);

    const uint32_t serial = 0x00ABCDEFu;
    const uint32_t cnt = 0x004242u;
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    TEST_ASSERT_TRUE(subghz_vag_encrypt_frame(
        serial, cnt, 4u /*boot*/, 1u, 0xC1u,
        0u /*explicit key index*/, 0, 0, keys48, true, &key1, &key2));

    uint32_t dec_serial = 0, dec_cnt = 0;
    uint8_t dec_btn = 0;
    TEST_ASSERT_TRUE(m1_vag_t12_decrypt(1u, key1, key2, keys48, true,
                                        &dec_serial, &dec_cnt, &dec_btn));
    TEST_ASSERT_EQUAL_HEX32(serial, dec_serial);
    TEST_ASSERT_EQUAL_HEX32(cnt, dec_cnt);
    TEST_ASSERT_EQUAL_HEX8(4u, dec_btn);
}

void test_type1_auto_key_selects_by_serial_match(void)
{
    uint8_t keys48[48] = {0};
    memcpy(keys48, PACKED_KEY, 16);

    /* Encrypt with explicit key 0, then re-encrypt the "next" frame with
     * VAG_ENC_KEY_AUTO: the trial decrypt of the first frame must re-select
     * key 0 and produce a frame that decrypts to the new fields. */
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    TEST_ASSERT_TRUE(subghz_vag_encrypt_frame(
        0x00ABCDEFu, 1u, 2u, 1u, 0xC1u, 0u, 0, 0, keys48, true,
        &key1, &key2));

    uint64_t key1b = 0;
    uint16_t key2b = 0;
    TEST_ASSERT_TRUE(subghz_vag_encrypt_frame(
        0x00ABCDEFu, 2u, 2u, 1u, 0xC1u, VAG_ENC_KEY_AUTO, key1, key2,
        keys48, true, &key1b, &key2b));

    uint32_t dec_cnt = 0;
    TEST_ASSERT_TRUE(m1_vag_t12_decrypt(1u, key1b, key2b, keys48, true,
                                        NULL, &dec_cnt, NULL));
    TEST_ASSERT_EQUAL_HEX32(2u, dec_cnt);
}

void test_type1_without_keys_fails(void)
{
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    TEST_ASSERT_FALSE(subghz_vag_encrypt_frame(
        0x00ABCDEFu, 1u, 2u, 1u, 0xC1u, VAG_ENC_KEY_AUTO, 0, 0, NULL, false,
        &key1, &key2));
}

/*============================================================================*/
/* Upload shapes                                                               */
/*============================================================================*/

void test_upload_t12_shape(void)
{
    static subghz_upload_entry_t up[VAG_ENC_UPLOAD_T12];
    const uint32_t n =
        subghz_vag_build_upload(up, VAG_ENC_UPLOAD_T12, 1u,
                                0xC1123456789ABCD0ULL, 0x4E2Au);
    TEST_ASSERT_EQUAL_UINT32(VAG_ENC_UPLOAD_T12, n);

    /* Preamble: 220 mark/space pairs @300 µs. */
    TEST_ASSERT_TRUE(up[0].level);
    TEST_ASSERT_EQUAL_UINT32(300u, up[0].duration);
    TEST_ASSERT_EQUAL_UINT32(300u, up[439].duration);

    /* Sync: low 300, high 300. */
    TEST_ASSERT_FALSE(up[440].level);
    TEST_ASSERT_TRUE(up[441].level);

    /* Prefix 0xAF3F MSB-first = 1 → (mark, space). */
    TEST_ASSERT_TRUE(up[442].level);
    TEST_ASSERT_FALSE(up[443].level);

    /* Gap: 6000 µs low at the end. */
    TEST_ASSERT_EQUAL_UINT32(6000u, up[n - 1u].duration);
    TEST_ASSERT_FALSE(up[n - 1u].level);
}

void test_upload_t34_shape(void)
{
    static subghz_upload_entry_t up[VAG_ENC_UPLOAD_T34];
    const uint32_t n =
        subghz_vag_build_upload(up, VAG_ENC_UPLOAD_T34, 3u,
                                0xC1123456789ABCD0ULL, 0x4E2Bu);
    TEST_ASSERT_EQUAL_UINT32(VAG_ENC_UPLOAD_T34, n);

    /* 45 preamble pairs @500, sync high 1000 / low 500, 3 pairs @750. */
    TEST_ASSERT_EQUAL_UINT32(500u, up[0].duration);
    TEST_ASSERT_TRUE(up[90].level);
    TEST_ASSERT_EQUAL_UINT32(1000u, up[90].duration);
    TEST_ASSERT_EQUAL_UINT32(500u, up[91].duration);
    TEST_ASSERT_EQUAL_UINT32(750u, up[92].duration);

    /* Two repetitions, 10000 µs low gap closing each. */
    TEST_ASSERT_EQUAL_UINT32(10000u, up[258].duration);
    TEST_ASSERT_FALSE(up[258].level);
    TEST_ASSERT_EQUAL_UINT32(10000u, up[n - 1u].duration);
}

void test_upload_capacity_guard(void)
{
    static subghz_upload_entry_t up[VAG_ENC_UPLOAD_T12];
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_vag_build_upload(up, VAG_ENC_UPLOAD_T12 - 1u, 1u, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_vag_build_upload(NULL, VAG_ENC_UPLOAD_T12, 1u, 0, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_aut64_encrypt_decrypt_roundtrip);
    RUN_TEST(test_aut64_encrypt_matches_existing_decode_vector);
    RUN_TEST(test_button_and_dispatch_bytes);
    RUN_TEST(test_type2_encrypt_upload_decode_roundtrip);
    RUN_TEST(test_type1_passat_converts_to_type2);
    RUN_TEST(test_type1_encrypt_decrypts_back);
    RUN_TEST(test_type1_auto_key_selects_by_serial_match);
    RUN_TEST(test_type1_without_keys_fails);
    RUN_TEST(test_upload_t12_shape);
    RUN_TEST(test_upload_t34_shape);
    RUN_TEST(test_upload_capacity_guard);
    return UNITY_END();
}
