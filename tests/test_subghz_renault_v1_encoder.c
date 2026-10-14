/* See COPYING.txt for license details. */

/*
 * Host tests for the Renault V1 seed-path encoder (TX after recovery).
 *
 * The central check is a round-trip cross-validation: a frame produced by
 * subghz_renault_v1_encrypt_frame() must be crackable by the ported
 * brute-force search, which must recover exactly the IV the encoder used.
 * No real key material is used.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "subghz_renault_v1_encoder.h"
#include "subghz_renault_v1_bruteforce.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* IV build                                                                    */
/*============================================================================*/

void test_build_iv_known_vector(void)
{
    uint8_t iv[4];
    subghz_renault_v1_build_iv(0xABCDu, 0x5u, 0x12345678u, iv);
    /* iv[0] = (cnt<<4 & 0xF0)|btn = 0xD0|0x5
     * iv[1] = cnt>>4 & 0xFF       = 0xBC
     * iv[2] = (seed>>8 & 0xF0)|(cnt>>12 & 0xF) = 0x50|0xA
     * iv[3] = seed & 0xFF         = 0x78 */
    const uint8_t want[4] = {0xD5, 0xBC, 0x5A, 0x78};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, iv, 4);
}

/*============================================================================*/
/* Frame encryption                                                            */
/*============================================================================*/

void test_encrypt_frame_layout_and_checksum(void)
{
    uint8_t out[11];
    uint8_t iv[4];
    subghz_renault_v1_encrypt_frame(0x11223344u, 0x0ABCu, 0x3u, 0x9ABCDEF0u,
                                    out, iv);

    /* Serial passes through big-endian. */
    TEST_ASSERT_EQUAL_UINT8(0x11, out[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22, out[1]);
    TEST_ASSERT_EQUAL_UINT8(0x33, out[2]);
    TEST_ASSERT_EQUAL_UINT8(0x44, out[3]);
    /* Button occupies the high nibble of byte 4. */
    TEST_ASSERT_EQUAL_UINT8(0x3u, (out[4] >> 4) & 0xFu);
    /* XOR8 checksum. */
    uint8_t x = 0;
    for (uint8_t i = 0; i < 10u; i++)
        x ^= out[i];
    TEST_ASSERT_EQUAL_UINT8(x, out[10]);
    /* IV output matches build_iv. */
    uint8_t want_iv[4];
    subghz_renault_v1_build_iv(0x0ABCu, 0x3u, 0x9ABCDEF0u, want_iv);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want_iv, iv, 4);
}

void test_encrypt_then_bruteforce_recovers_same_iv(void)
{
    const uint32_t serial = 0x0C0FFEE0u;
    const uint32_t cnt = 0x5A5Au;
    const uint8_t btn = 0x2u;
    const uint32_t seed = 0x1A2B3C4Du;

    uint8_t frame[11];
    uint8_t enc_iv[4];
    subghz_renault_v1_encrypt_frame(serial, cnt, btn, seed, frame, enc_iv);

    /* The full search must recover exactly the IV the encoder used —
     * cross-validates the encoder and the search against each other. */
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    uint8_t found_iv[4] = {0};
    TEST_ASSERT_TRUE(subghz_renault_v1_bf_run(&job, frame, found_iv));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(enc_iv, found_iv, 4);

    /* And the recovered seed feeds the counter reconstruction. */
    const uint32_t recovered_seed = subghz_renault_v1_bf_seed_from_iv(found_iv);
    const uint32_t recovered_cnt =
        subghz_renault_v1_bf_counter_from_seed(recovered_seed);
    TEST_ASSERT_EQUAL_HEX32(cnt & 0xFFFFu, recovered_cnt & 0xFFFFu);
}

/*============================================================================*/
/* Upload builder                                                              */
/*============================================================================*/

void test_upload_shape_and_timings(void)
{
    static renault_v1_upload_entry_t up[RENAULT_V1_UPLOAD_CAPACITY];
    const uint64_t key = 0x1122334455667788ULL;
    const uint32_t key2 = 0x99AA55u;

    const uint32_t n =
        subghz_renault_v1_build_upload(up, RENAULT_V1_UPLOAD_CAPACITY, key,
                                       key2);
    TEST_ASSERT_EQUAL_UINT32(RENAULT_V1_UPLOAD_CAPACITY, n);

    /* Preamble: 250 mark/space pairs at TE. */
    for (uint32_t i = 0; i < 2u * RENAULT_V1_PREAMBLE_PAIRS; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(RENAULT_V1_TE_US, up[i].duration);
        TEST_ASSERT_EQUAL(i % 2u == 0u, up[i].level);
    }

    /* First long frame header: 1500 µs low, 1000 µs high. */
    uint32_t idx = 2u * RENAULT_V1_PREAMBLE_PAIRS;
    TEST_ASSERT_EQUAL_UINT32(RENAULT_V1_HEADER_LOW_US, up[idx].duration);
    TEST_ASSERT_FALSE(up[idx].level);
    TEST_ASSERT_EQUAL_UINT32(RENAULT_V1_HEADER_HIGH_US, up[idx + 1u].duration);
    TEST_ASSERT_TRUE(up[idx + 1u].level);

    /* 16 header bits encode the value 1, MSB-first: fifteen 0 bits
     * (space/mark pairs) followed by one 1 bit (mark/space). */
    idx += 2;
    for (uint32_t i = 0; i < 30u; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(RENAULT_V1_TE_US, up[idx + i].duration);
        TEST_ASSERT_EQUAL(i % 2u == 1u, up[idx + i].level);
    }
    TEST_ASSERT_TRUE(up[idx + 30u].level);
    TEST_ASSERT_FALSE(up[idx + 31u].level);

    /* First key bit (MSB of key = 0) → space/mark. */
    idx += 32;
    TEST_ASSERT_FALSE(up[idx].level);
    TEST_ASSERT_TRUE(up[idx + 1u].level);

    /* Last entry: the 21.5 ms low gap closing the third short frame. */
    TEST_ASSERT_EQUAL_UINT32(RENAULT_V1_SHORT_GAP_US, up[n - 1u].duration);
    TEST_ASSERT_FALSE(up[n - 1u].level);
}

void test_upload_capacity_guard(void)
{
    static renault_v1_upload_entry_t up[RENAULT_V1_UPLOAD_CAPACITY];
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_renault_v1_build_upload(up, RENAULT_V1_UPLOAD_CAPACITY - 1u,
                                           0x1122u, 0x33u));
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_renault_v1_build_upload(NULL, RENAULT_V1_UPLOAD_CAPACITY,
                                           0x1122u, 0x33u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_iv_known_vector);
    RUN_TEST(test_encrypt_frame_layout_and_checksum);
    RUN_TEST(test_encrypt_then_bruteforce_recovers_same_iv);
    RUN_TEST(test_upload_shape_and_timings);
    RUN_TEST(test_upload_capacity_guard);
    return UNITY_END();
}
