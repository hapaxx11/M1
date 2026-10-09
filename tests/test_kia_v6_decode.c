/* See COPYING.txt for license details. */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_kia_v6_decode.h"
#include "subghz_protopirate_keys_builtin.h"
#include "tiny_aes.h"

const uint64_t m1_kia_v6_keys_builtin[M1_KIA_V6_KEY_COUNT] = {
    0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL,
};
const bool m1_kia_v6_keys_builtin_available = true;
const uint8_t m1_vag_aut64_keys_builtin[M1_VAG_AUT64_KEY_BYTES] = {0};
const bool m1_vag_aut64_keys_builtin_available = false;

static uint8_t crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFFu;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            const bool high_bit = (crc & 0x80u) != 0u;
            crc = (uint8_t)(crc << 1);
            if (high_bit)
                crc ^= 0x07u;
        }
    }
    return crc;
}

static void build_frame(const uint64_t keys[2], uint32_t counter,
                        uint8_t bits[M1_KIA_V6_FRAME_BITS])
{
    uint8_t plain[16] = {0};
    plain[0] = 0x34u;
    plain[4] = 0x12u;
    plain[5] = 0x34u;
    plain[6] = 0x56u;
    plain[7] = 0x02u;
    plain[8] = (uint8_t)(counter >> 24);
    plain[9] = (uint8_t)(counter >> 16);
    plain[10] = (uint8_t)(counter >> 8);
    plain[11] = (uint8_t)counter;
    plain[15] = crc8(plain, 15);

    uint8_t aes_key[16];
    for (uint8_t part = 0; part < 2; part++) {
        uint32_t high = (uint32_t)(keys[part] >> 32) ^ 0x638766ABu;
        uint32_t low = (uint32_t)keys[part] ^ 0x84AF25FBu;
        for (uint8_t i = 0; i < 4; i++) {
            aes_key[part * 8u + i] = (uint8_t)(high >> (24u - 8u * i));
            aes_key[part * 8u + 4u + i] = (uint8_t)(low >> (24u - 8u * i));
        }
    }
    struct AES_ctx aes;
    AES_init_ctx(&aes, aes_key);
    AES_ECB_encrypt(&aes, plain);

    uint8_t wire[18] = {0x23u, 0x04u};
    memcpy(&wire[2], plain, sizeof(plain));
    for (uint16_t i = 0; i < M1_KIA_V6_FRAME_BITS; i++)
        bits[i] = (uint8_t)((~wire[i / 8u] >> (7u - (i % 8u))) & 1u);
}

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

static uint16_t build_waveform(const uint8_t bits[M1_KIA_V6_FRAME_BITS])
{
    uint16_t count = 0;
    bool have_level = false, last_level = false;
    for (uint8_t i = 0; i < 38u; i++) {
        TEST_ASSERT_TRUE(append_pulse(true, 200u, &count, &have_level, &last_level));
        TEST_ASSERT_TRUE(append_pulse(false, 200u, &count, &have_level, &last_level));
    }
    TEST_ASSERT_TRUE(append_pulse(false, 200u, &count, &have_level, &last_level));
    TEST_ASSERT_TRUE(append_pulse(true, 400u, &count, &have_level, &last_level));
    TEST_ASSERT_TRUE(append_pulse(false, 200u, &count, &have_level, &last_level));

    for (uint16_t i = 3; i < M1_KIA_V6_FRAME_BITS; i++) {
        const bool bit = bits[i] != 0u;
        const bool first = !bit;
        TEST_ASSERT_TRUE(append_pulse(first, 200u, &count, &have_level, &last_level));
        TEST_ASSERT_TRUE(append_pulse(!first, 200u, &count, &have_level, &last_level));
    }
    return count;
}

void setUp(void) { memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl)); }
void tearDown(void) {}

void test_parse_decrypts_fields_and_checks_crc(void)
{
    uint8_t bits[M1_KIA_V6_FRAME_BITS];
    m1_kia_v6_data_t data = {0};
    build_frame(m1_kia_v6_keys_builtin, 0x01020304u, bits);

    TEST_ASSERT_TRUE(m1_kia_v6_parse(bits, m1_kia_v6_keys_builtin, &data));
    TEST_ASSERT_EQUAL_HEX32(0x123456u, data.serial);
    TEST_ASSERT_EQUAL_HEX32(0x01020304u, data.counter);
    TEST_ASSERT_EQUAL_HEX8(2u, data.button);
    TEST_ASSERT_EQUAL_HEX8(0x34u, data.fx);

    bits[16] ^= 1u;
    TEST_ASSERT_FALSE(m1_kia_v6_parse(bits, m1_kia_v6_keys_builtin, &data));
}

void test_parse_rejects_invalid_metadata_and_nulls(void)
{
    uint8_t bits[M1_KIA_V6_FRAME_BITS] = {0};
    m1_kia_v6_data_t data;
    TEST_ASSERT_FALSE(m1_kia_v6_parse(bits, m1_kia_v6_keys_builtin, &data));
    TEST_ASSERT_FALSE(m1_kia_v6_parse(bits, NULL, &data));
    TEST_ASSERT_FALSE(m1_kia_v6_parse(bits, m1_kia_v6_keys_builtin, NULL));
}

void test_decoder_rejects_noise(void)
{
    for (uint16_t i = 0; i < 32u; i++)
        subghz_decenc_ctl.pulse_times[i] = (uint16_t)(1200u + i * 17u);
    TEST_ASSERT_EQUAL_UINT8(1u, subghz_decode_kia_v6(KIA_V6, 32u));
}

void test_decoder_decodes_manchester_waveform(void)
{
    uint8_t bits[M1_KIA_V6_FRAME_BITS];
    build_frame(m1_kia_v6_keys_builtin, 0x01020304u, bits);
    const uint16_t count = build_waveform(bits);
    TEST_ASSERT_LESS_THAN_UINT16(PACKET_PULSE_COUNT_MAX, count);

    TEST_ASSERT_EQUAL_UINT8(0u, subghz_decode_kia_v6(KIA_V6, count));
    TEST_ASSERT_EQUAL_UINT16(M1_KIA_V6_FRAME_BITS,
                             subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(KIA_V6, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_HEX32(0x123456u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX32(0x01020304u, subghz_decenc_ctl.n32_rollingcode);
    TEST_ASSERT_EQUAL_HEX8(2u, subghz_decenc_ctl.n8_buttonid);
}

void test_decoder_accepts_321_pulse_kia_frame(void)
{
    uint8_t bits[M1_KIA_V6_FRAME_BITS];
    build_frame(m1_kia_v6_keys_builtin, 0x003CF089u, bits);
    const uint16_t count = build_waveform(bits);

    TEST_ASSERT_EQUAL_UINT16(321u, count);
    TEST_ASSERT_EQUAL_UINT8(0u, subghz_decode_kia_v6(KIA_V6, count));
    TEST_ASSERT_EQUAL_HEX32(0x123456u, subghz_decenc_ctl.n32_serialnumber);
    TEST_ASSERT_EQUAL_HEX32(0x003CF089u, subghz_decenc_ctl.n32_rollingcode);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_decrypts_fields_and_checks_crc);
    RUN_TEST(test_parse_rejects_invalid_metadata_and_nulls);
    RUN_TEST(test_decoder_rejects_noise);
    RUN_TEST(test_decoder_decodes_manchester_waveform);
    RUN_TEST(test_decoder_accepts_321_pulse_kia_frame);
    return UNITY_END();
}
