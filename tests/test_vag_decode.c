/* See COPYING.txt for license details. */

/*
 * test_vag_decode.c
 *
 * Host unit tests for Sub_Ghz/protocols/m1_vag_decode.c — the ported
 * ProtoPirate "VAG" automotive keyfob *identify* decoder (T12 format).
 *
 *   1. m1_vag_t12_parse() — pure prefix/type + dispatch-byte(button) validation,
 *      with the payload one's-complement applied.
 *   2. subghz_decode_vag() — full 95-bit Manchester pulse-array decode.
 */

#include <string.h>
#include "unity.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_protopirate_keys_builtin.h"

#define VAG_BITS 95u
#define VAG_TE_SHORT 300u
#define VAG_TE_LONG  600u
#define VAG_PREFIX_T1 0x2F3Fu
#define VAG_PREFIX_T2 0x2F1Cu
#define VAG_BTN_UNLOCK 0x1u
#define VAG_BTN_LOCK   0x2u
#define VAG_BTN_BOOT   0x4u

bool m1_vag_t12_parse(uint16_t prefix, uint64_t key1_raw, uint16_t key2_raw,
                      uint8_t *type, uint8_t *button, uint64_t *key1_out);
bool m1_vag_t12_decrypt(uint8_t type, uint64_t key1, uint16_t key2,
                        const uint8_t packed_keys[M1_VAG_AUT64_KEY_BYTES],
                        bool keys_available, uint32_t *serial,
                        uint32_t *counter, uint8_t *button);
uint8_t subghz_decode_vag(uint16_t p, uint16_t pulsecount);

void setUp(void) { memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl)); }
void tearDown(void) {}

/*
 * Build a 95-bit VAG T12 waveform into pulse_times[] from the three raw wire
 * fields (as received, before the decoder applies the one's-complement).
 */
static uint16_t build_vag_pulses(uint16_t prefix, uint64_t key1_raw,
                                 uint16_t key2_raw)
{
    uint8_t bits[VAG_BITS];
    uint16_t nb = 0;
    for (int i = 14; i >= 0; i--)  bits[nb++] = (prefix >> i) & 1u;      /* 15 */
    for (int i = 63; i >= 0; i--)  bits[nb++] = (key1_raw >> i) & 1u;    /* 64 */
    for (int i = 15; i >= 0; i--)  bits[nb++] = (key2_raw >> i) & 1u;    /* 16 */

    uint8_t cells[VAG_BITS * 2u];
    uint16_t nc = 0;
    for (uint16_t b = 0; b < VAG_BITS; b++)
    {
        if (bits[b]) { cells[nc++] = 1; cells[nc++] = 0; }
        else         { cells[nc++] = 0; cells[nc++] = 1; }
    }
    uint16_t np = 0, i = 0;
    while (i < nc)
    {
        uint16_t run = 1;
        while (i + run < nc && cells[i + run] == cells[i]) run++;
        subghz_decenc_ctl.pulse_times[np++] = (run >= 2) ? VAG_TE_LONG : VAG_TE_SHORT;
        i += run;
    }
    return np;
}

static void tea_encrypt(uint32_t *left, uint32_t *right)
{
    static const uint32_t key[4] = {
        0x0B46502Du, 0x5E253718u, 0x2BF93A19u, 0x622C1206u,
    };
    uint32_t sum = 0;
    for (uint8_t round = 0; round < 32u; round++) {
        *left += (((*right << 4) ^ (*right >> 5)) + *right) ^
                 (sum + key[sum & 3u]);
        sum += 0x9E3779B9u;
        *right += (((*left << 4) ^ (*left >> 5)) + *left) ^
                  (sum + key[(sum >> 11) & 3u]);
    }
}

/* desired key1 = 0xAABBCCDD11223344, dispatch 0x2A (Lock). */
#define TEST_KEY1    0xAABBCCDD11223344ull
#define TEST_KEY1RAW (~TEST_KEY1)
#define TEST_KEY2    0x002Au                 /* high=0, low=dispatch 0x2A */
#define TEST_KEY2RAW ((uint16_t)~TEST_KEY2)

void test_parse_type1_lock(void)
{
    uint8_t type = 0, btn = 0; uint64_t key1 = 0;
    TEST_ASSERT_TRUE(m1_vag_t12_parse(VAG_PREFIX_T1, TEST_KEY1RAW, TEST_KEY2RAW,
                                      &type, &btn, &key1));
    TEST_ASSERT_EQUAL_UINT8(1u, type);
    TEST_ASSERT_EQUAL_HEX8(VAG_BTN_LOCK, btn);
    TEST_ASSERT_EQUAL_HEX64(TEST_KEY1, key1);
}

void test_parse_type2_unlock(void)
{
    uint8_t type = 0, btn = 0;
    uint16_t k2raw = (uint16_t)~0x001Cu;  /* dispatch 0x1C = Unlock */
    TEST_ASSERT_TRUE(m1_vag_t12_parse(VAG_PREFIX_T2, TEST_KEY1RAW, k2raw,
                                      &type, &btn, NULL));
    TEST_ASSERT_EQUAL_UINT8(2u, type);
    TEST_ASSERT_EQUAL_HEX8(VAG_BTN_UNLOCK, btn);
}

void test_parse_rejects_bad_prefix(void)
{
    TEST_ASSERT_FALSE(m1_vag_t12_parse(0x1234u, TEST_KEY1RAW, TEST_KEY2RAW,
                                       NULL, NULL, NULL));
}

void test_parse_rejects_bad_dispatch(void)
{
    uint16_t k2raw = (uint16_t)~0x0099u;  /* 0x99 not a valid dispatch */
    TEST_ASSERT_FALSE(m1_vag_t12_parse(VAG_PREFIX_T1, TEST_KEY1RAW, k2raw,
                                       NULL, NULL, NULL));
}

void test_type2_decrypts_serial_counter_and_button(void)
{
    uint8_t plain[8] = {0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x10};
    uint32_t left = ((uint32_t)plain[0] << 24) | ((uint32_t)plain[1] << 16) |
                    ((uint32_t)plain[2] << 8) | plain[3];
    uint32_t right = ((uint32_t)plain[4] << 24) | ((uint32_t)plain[5] << 16) |
                     ((uint32_t)plain[6] << 8) | plain[7];
    tea_encrypt(&left, &right);
    uint8_t encrypted[8] = {
        (uint8_t)(left >> 24), (uint8_t)(left >> 16), (uint8_t)(left >> 8), (uint8_t)left,
        (uint8_t)(right >> 24), (uint8_t)(right >> 16), (uint8_t)(right >> 8), (uint8_t)right,
    };
    const uint64_t key1 = 0xAA00000000000000ull |
        ((uint64_t)encrypted[0] << 48) | ((uint64_t)encrypted[1] << 40) |
        ((uint64_t)encrypted[2] << 32) | ((uint64_t)encrypted[3] << 24) |
        ((uint64_t)encrypted[4] << 16) | ((uint64_t)encrypted[5] << 8) |
        encrypted[6];
    const uint16_t key2 = (uint16_t)(((uint16_t)encrypted[7] << 8) | 0x1Cu);
    uint32_t serial = 0, counter = 0;
    uint8_t button = 0;

    TEST_ASSERT_TRUE(m1_vag_t12_decrypt(2u, key1, key2, NULL, false,
                                        &serial, &counter, &button));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, serial);
    TEST_ASSERT_EQUAL_HEX32(0x00563412u, counter);
    TEST_ASSERT_EQUAL_HEX8(VAG_BTN_UNLOCK, button);
}

void test_type1_requires_packed_keys(void)
{
    uint32_t serial = 0, counter = 0;
    uint8_t button = 0;
    TEST_ASSERT_FALSE(m1_vag_t12_decrypt(1u, TEST_KEY1, TEST_KEY2, NULL, false,
                                         &serial, &counter, &button));
}

void test_type1_aut64_decrypts_serial_counter_and_button(void)
{
    static const uint8_t packed_key[16] = {
        0x01, 0x12, 0x34, 0x56, 0x78, 0x05, 0x39, 0x77,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    };
    uint8_t packed_keys[M1_VAG_AUT64_KEY_BYTES] = {0};
    memcpy(packed_keys, packed_key, sizeof(packed_key));
    const uint64_t key1 = 0xAA12345678123456ull;
    const uint16_t key2 = 0x4E1Cu;
    uint32_t serial = 0, counter = 0;
    uint8_t button = 0;

    TEST_ASSERT_TRUE(m1_vag_t12_decrypt(1u, key1, key2, packed_keys, true,
                                        &serial, &counter, &button));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, serial);
    TEST_ASSERT_EQUAL_HEX32(0x00563412u, counter);
    TEST_ASSERT_EQUAL_HEX8(VAG_BTN_UNLOCK, button);
}

void test_decode_waveform(void)
{
    uint16_t n = build_vag_pulses(VAG_PREFIX_T1, TEST_KEY1RAW, TEST_KEY2RAW);
    TEST_ASSERT_EQUAL_UINT8(0, subghz_decode_vag(VAG, n));
    TEST_ASSERT_EQUAL_UINT16(VAG_BITS, subghz_decenc_ctl.ndecodedbitlength);
    TEST_ASSERT_EQUAL_UINT16(VAG, subghz_decenc_ctl.ndecodedprotocol);
    TEST_ASSERT_EQUAL_HEX8(VAG_BTN_LOCK, subghz_decenc_ctl.n8_buttonid);
    TEST_ASSERT_EQUAL_HEX64(TEST_KEY1, subghz_decenc_ctl.n64_decodedvalue);
    TEST_ASSERT_EQUAL_HEX32(0xAABBCCDDu, subghz_decenc_ctl.n32_serialnumber);
}

void test_decode_rejects_noise(void)
{
    for (uint16_t i = 0; i < 64; i++)
        subghz_decenc_ctl.pulse_times[i] = (uint16_t)(2000 + i * 13);
    TEST_ASSERT_EQUAL_UINT8(1, subghz_decode_vag(VAG, 64));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_type1_lock);
    RUN_TEST(test_parse_type2_unlock);
    RUN_TEST(test_parse_rejects_bad_prefix);
    RUN_TEST(test_parse_rejects_bad_dispatch);
    RUN_TEST(test_type2_decrypts_serial_counter_and_button);
    RUN_TEST(test_type1_requires_packed_keys);
    RUN_TEST(test_type1_aut64_decrypts_serial_counter_and_button);
    RUN_TEST(test_decode_waveform);
    RUN_TEST(test_decode_rejects_noise);
    return UNITY_END();
}
