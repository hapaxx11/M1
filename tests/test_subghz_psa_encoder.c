/* See COPYING.txt for license details. */

/*
 * Host tests for the PSA encoder (TX after recovery).
 *
 * Cross-validation round-trips:
 *  - type 0x36: encode → the brute-force search must crack the frame and
 *    recover exactly the encoded serial/button/counter;
 *  - type 0x23: encode → the decoder's parse function must extract the
 *    encoded fields.
 * No real key material is used.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "subghz_psa_encoder.h"
#include "subghz_psa_bruteforce.h"

/* From the decoder (Sub_Ghz/protocols/m1_psa_decode.c). */
bool m1_psa_parse_frame(const uint8_t frame[10], uint32_t *serial,
                        uint8_t *button, uint16_t *counter);

void setUp(void) {}
void tearDown(void) {}

/* Marker-nibble-carrying wire bytes 0..1, as on a real capture. */
static const uint8_t PRESERVE01[2] = {0x12, 0x3A};

/*============================================================================*/
/* Type 0x36 (TEA) — encode → brute-force round-trip                           */
/*============================================================================*/

void test_encode36_bruteforce_recovers_fields(void)
{
    const uint32_t serial = 0x051309u;
    const uint32_t counter = 0x086F0123u;
    const uint8_t  button = 0x5u;

    uint8_t buffer[10];
    subghz_psa_encode_buffer36(serial, counter, button, PRESERVE01, buffer);

    /* Marker nibble preserved for decoder acceptance. */
    TEST_ASSERT_EQUAL_UINT8(0x12, buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0x3A, buffer[1]);

    uint64_t key1 = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key1 = (key1 << 8) | buffer[i];
    const uint16_t key2 = (uint16_t)(((uint16_t)buffer[8] << 8) | buffer[9]);

    /* The phase-1 candidate that must win is 0x23000000|serial. */
    const uint32_t cand = PSA_BF1_START | (serial & 0xFFFFFFu);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 3u));

    subghz_psa_bf_result_t r;
    memset(&r, 0, sizeof(r));
    TEST_ASSERT_TRUE(subghz_psa_bf_run_range(&job, key1, key2, PSA_BF_PHASE_1,
                                             cand - 1u, cand + 2u, &r));

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_HEX32(cand, r.candidate);
    TEST_ASSERT_EQUAL_HEX32(serial, r.serial);
    TEST_ASSERT_EQUAL_HEX32(counter & 0x0FFFFFFFu, r.counter);
    TEST_ASSERT_EQUAL_UINT8(button, r.button);
    TEST_ASSERT_EQUAL_UINT8(0x36u, r.type);
}

void test_encode36_counter_advance_produces_distinct_frames(void)
{
    uint8_t a[10], b[10];
    subghz_psa_encode_buffer36(0x051309u, 100u, 1u, PRESERVE01, a);
    subghz_psa_encode_buffer36(0x051309u, 101u, 1u, PRESERVE01, b);
    /* Identical except counter → ciphertext must differ. */
    TEST_ASSERT_TRUE(memcmp(a, b, 10) != 0);
}

/*============================================================================*/
/* Type 0x23 (direct XOR) — encode → decode round-trip                         */
/*============================================================================*/

void test_encode23_decodes_to_original_fields(void)
{
    const uint32_t serial = 0x051309u;
    const uint16_t counter = 0x86F0u;
    const uint8_t  button = 0x1u;

    uint8_t buffer[10];
    subghz_psa_encode_buffer23(serial, counter, 0x42u, button, 0x91AAu, true,
                               PRESERVE01, buffer);

    uint32_t dec_serial = 0;
    uint16_t dec_counter = 0;
    uint8_t  dec_button = 0;
    TEST_ASSERT_TRUE(m1_psa_parse_frame(buffer, &dec_serial, &dec_button,
                                        &dec_counter));
    TEST_ASSERT_EQUAL_HEX32(serial, dec_serial);
    TEST_ASSERT_EQUAL_HEX16(counter, dec_counter);
    TEST_ASSERT_EQUAL_UINT8(button, dec_button);
}

void test_encode23_without_key2_uses_default_and_decodes(void)
{
    uint8_t buffer[10];
    subghz_psa_encode_buffer23(0x0ABCDEu, 0x1234u, 0x00u, 0x2u, 0, false,
                               PRESERVE01, buffer);

    uint32_t dec_serial = 0;
    uint16_t dec_counter = 0;
    uint8_t  dec_button = 0;
    TEST_ASSERT_TRUE(m1_psa_parse_frame(buffer, &dec_serial, &dec_button,
                                        &dec_counter));
    TEST_ASSERT_EQUAL_HEX32(0x0ABCDEu, dec_serial);
    TEST_ASSERT_EQUAL_HEX16(0x1234u, dec_counter);
    TEST_ASSERT_EQUAL_UINT8(0x2u, dec_button);
}

/*============================================================================*/
/* Upload builder                                                              */
/*============================================================================*/

void test_upload_shape_and_timings(void)
{
    static psa_upload_entry_t up[PSA_ENC_UPLOAD_CAPACITY];
    const uint8_t buffer[10] = {0x80, 0x3A, 0x01, 0x02, 0x03,
                                0x04, 0x05, 0x06, 0x07, 0x08};

    const uint32_t n =
        subghz_psa_build_upload(up, PSA_ENC_UPLOAD_CAPACITY, buffer);
    TEST_ASSERT_EQUAL_UINT32(PSA_ENC_UPLOAD_CAPACITY, n);

    /* Preamble: 80 mark/space pairs at 250 µs. */
    for (uint32_t i = 0; i < 160u; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(250u, up[i].duration);
        TEST_ASSERT_EQUAL(i % 2u == 0u, up[i].level);
    }

    /* Transition: low 250, high 500, low 250. */
    TEST_ASSERT_EQUAL_UINT32(250u, up[160].duration);
    TEST_ASSERT_FALSE(up[160].level);
    TEST_ASSERT_EQUAL_UINT32(500u, up[161].duration);
    TEST_ASSERT_TRUE(up[161].level);
    TEST_ASSERT_EQUAL_UINT32(250u, up[162].duration);
    TEST_ASSERT_FALSE(up[162].level);

    /* First Key1 bit: buffer[0] MSB = 1 → mark,space. */
    TEST_ASSERT_TRUE(up[163].level);
    TEST_ASSERT_EQUAL_UINT32(250u, up[163].duration);
    TEST_ASSERT_FALSE(up[164].level);
    /* Second Key1 bit (0x80 bit 6) = 0 → space,mark. */
    TEST_ASSERT_FALSE(up[165].level);
    TEST_ASSERT_TRUE(up[166].level);

    /* End: high 1000, low 1000. */
    TEST_ASSERT_EQUAL_UINT32(1000u, up[n - 2u].duration);
    TEST_ASSERT_TRUE(up[n - 2u].level);
    TEST_ASSERT_EQUAL_UINT32(1000u, up[n - 1u].duration);
    TEST_ASSERT_FALSE(up[n - 1u].level);
}

void test_upload_capacity_guard(void)
{
    static psa_upload_entry_t up[PSA_ENC_UPLOAD_CAPACITY];
    const uint8_t buffer[10] = {0};
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_psa_build_upload(up, PSA_ENC_UPLOAD_CAPACITY - 1u, buffer));
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_psa_build_upload(NULL, PSA_ENC_UPLOAD_CAPACITY, buffer));
    TEST_ASSERT_EQUAL_UINT32(
        0u, subghz_psa_build_upload(up, PSA_ENC_UPLOAD_CAPACITY, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode36_bruteforce_recovers_fields);
    RUN_TEST(test_encode36_counter_advance_produces_distinct_frames);
    RUN_TEST(test_encode23_decodes_to_original_fields);
    RUN_TEST(test_encode23_without_key2_uses_default_and_decodes);
    RUN_TEST(test_upload_shape_and_timings);
    RUN_TEST(test_upload_capacity_guard);
    return UNITY_END();
}
