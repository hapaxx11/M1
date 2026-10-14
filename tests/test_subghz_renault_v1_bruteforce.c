/* See COPYING.txt for license details. */

/*
 * Host tests for the Renault V1 Hitag2 IV brute-force core.
 *
 * Synthetic captures are fabricated around a chosen candidate: the shuffle
 * applied by the prepare step is a bijection on frame[4..9] except for
 * frame[9] bits 0-1 (provably dropped), so the test fixes the free input
 * bytes, forward-clocks the cipher to obtain the keystream the candidate
 * produces, then solves the frame bytes that make the frame's embedded hop
 * equal that keystream.  No real key material is used.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "subghz_renault_v1_bruteforce.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Frame fabricator                                                            */
/*============================================================================*/

/*
 * Build an 11-byte capture frame whose embedded hop equals the keystream
 * that `cand` produces, given the free frame bytes:
 *   frame[0..3] = serial, frame[4] = a, frame[5] bits 2..7 = b_hi6,
 *   frame[9] bits 0..1 = f_low2 (both are dropped by the shuffle).
 */
static void fabricate_frame(const uint8_t serial[4], uint8_t a, uint8_t b_hi6,
                            uint8_t f_low2, uint32_t cand,
                            uint8_t frame[11], uint8_t iv_out[4])
{
    uint8_t perm[6];
    subghz_hitag2_serial_permute(serial, perm);

    /* Fixed IV prefix bytes from the free inputs. */
    const uint8_t dest5 = (uint8_t)(((a & 3u) << 6) | (b_hi6 & 0x3Fu));
    const uint8_t dest6 = (uint8_t)(((f_low2 & 1u) << 7) | (a >> 2));
    const uint8_t iv0 = (uint8_t)((a >> 4) | (dest5 << 4));
    const uint8_t fp = (uint8_t)((dest5 >> 4) | ((dest6 & 3u) << 4));

    uint8_t iv[4];
    iv[0] = iv0;
    iv[1] = (uint8_t)(fp | ((cand & 3u) << 6));
    iv[2] = (uint8_t)(cand >> 2);
    iv[3] = (uint8_t)(cand >> 10);
    if (iv_out != NULL)
        memcpy(iv_out, iv, 4);

    /* Keystream this candidate must reproduce = required hop target. */
    uint8_t h[4];
    subghz_hitag2_keystream(serial, perm, iv, h);

    /* Solve frame bytes so the shuffled hop equals h. */
    const uint8_t b = (uint8_t)((b_hi6 << 2) | ((h[3] >> 6) & 3u));
    const uint8_t c = (uint8_t)(((h[3] & 0x3Fu) << 2) | ((h[2] >> 6) & 3u));
    const uint8_t d = (uint8_t)(((h[2] & 0x3Fu) << 2) | ((h[1] >> 6) & 3u));
    const uint8_t e = (uint8_t)(((h[1] & 0x3Fu) << 2) | ((h[0] >> 6) & 3u));
    const uint8_t f = (uint8_t)(((h[0] & 0x3Fu) << 2) | (f_low2 & 3u));

    frame[0] = serial[0];
    frame[1] = serial[1];
    frame[2] = serial[2];
    frame[3] = serial[3];
    frame[4] = a;
    frame[5] = b;
    frame[6] = c;
    frame[7] = d;
    frame[8] = e;
    frame[9] = f;
    /* XOR8 checksum — not consulted by the search, kept for realism. */
    uint8_t x = 0;
    for (uint8_t i = 0; i < 10u; i++)
        x ^= frame[i];
    frame[10] = x;
}

/*============================================================================*/
/* Primitive tests                                                             */
/*============================================================================*/

void test_frame_from_capture_packing(void)
{
    uint8_t frame[11];
    subghz_renault_v1_bf_frame_from_capture(frame, 0x1122334455667788u,
                                            0x99AAu);
    /* key2_low24 = 0x0099AA → frame[8]=0x00, frame[9]=0x99, frame[10]=0xAA */
    const uint8_t want[11] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                              0x77, 0x88, 0x00, 0x99, 0xAA};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, frame, 11);
}

void test_prepare_is_deterministic_and_keeps_serial(void)
{
    const uint8_t frame[11] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x23,
                               0x45, 0x67, 0x89, 0xAB, 0xCD};
    uint8_t s1[4], p1[6], h1[4], s2[4], p2[6], h2[4];
    uint8_t iv0_1, fp1, iv0_2, fp2;

    subghz_renault_v1_bf_prepare(frame, s1, p1, h1, &iv0_1, &fp1);
    subghz_renault_v1_bf_prepare(frame, s2, p2, h2, &iv0_2, &fp2);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(s1, s2, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p1, p2, 6);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(h1, h2, 4);
    TEST_ASSERT_EQUAL_UINT8(iv0_1, iv0_2);
    TEST_ASSERT_EQUAL_UINT8(fp1, fp2);
    /* Serial is the first 4 frame bytes, unshuffled. */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame, s1, 4);
}

void test_seed_and_counter_formulas(void)
{
    const uint8_t iv[4] = {0x12, 0x34, 0x56, 0x78};
    const uint32_t seed = subghz_renault_v1_bf_seed_from_iv(iv);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, seed);
    /* cnt = ((seed>>12)&0xFF0) | ((seed<<4)&0xF000) | (seed>>28)
     *      = 0x340 | 0x6000 | 0x1 = 0x6341 */
    TEST_ASSERT_EQUAL_HEX32(0x6341u,
                            subghz_renault_v1_bf_counter_from_seed(seed));
}

/*============================================================================*/
/* Search tests                                                                */
/*============================================================================*/

void test_finds_fabricated_candidate_in_range(void)
{
    const uint8_t serial[4] = {0x11, 0x22, 0x33, 0x44};
    const uint32_t cand = 0x12345u;
    uint8_t frame[11];
    uint8_t want_iv[4];
    fabricate_frame(serial, 0xA5u, 0x2Bu, 0x1u, cand, frame, want_iv);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 5u));

    uint8_t iv[4] = {0};
    TEST_ASSERT_TRUE(subghz_renault_v1_bf_run_range(&job, frame, cand - 2u,
                                                    cand + 3u, iv));
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_HEX32(cand, job.result);
    TEST_ASSERT_EQUAL_UINT32(3u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want_iv, iv, 4);
}

void test_full_run_finds_fabricated_candidate(void)
{
    const uint8_t serial[4] = {0xCA, 0xFE, 0xBA, 0xBE};
    const uint32_t cand = 0x2ABCDu;
    uint8_t frame[11];
    uint8_t want_iv[4];
    fabricate_frame(serial, 0x5Au, 0x11u, 0x2u, cand, frame, want_iv);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);

    uint8_t iv[4] = {0};
    TEST_ASSERT_TRUE(subghz_renault_v1_bf_run(&job, frame, iv));
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_UINT32(RENAULT_V1_BF_CANDIDATES, job.progress_total);
    TEST_ASSERT_EQUAL_UINT32(cand + 1u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want_iv, iv, 4);
}

void test_exhaustion_reports_not_found(void)
{
    const uint8_t serial[4] = {0x11, 0x22, 0x33, 0x44};
    const uint32_t cand = 0x12345u;
    uint8_t frame[11];
    fabricate_frame(serial, 0xA5u, 0x2Bu, 0x1u, cand, frame, NULL);

    /* Search a range that excludes the winning candidate. */
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 16u));
    TEST_ASSERT_FALSE(subghz_renault_v1_bf_run_range(&job, frame, cand + 8u,
                                                     cand + 24u, NULL));
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND, job.status);
    TEST_ASSERT_EQUAL_UINT32(16u, job.progress_current);
}

void test_hit_at_candidate_zero(void)
{
    const uint8_t serial[4] = {0x00, 0x11, 0x22, 0x33};
    uint8_t frame[11];
    uint8_t want_iv[4];
    fabricate_frame(serial, 0x00u, 0x00u, 0x0u, 0u, frame, want_iv);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 4u));

    uint8_t iv[4] = {0};
    TEST_ASSERT_TRUE(subghz_renault_v1_bf_run_range(&job, frame, 0u, 4u, iv));
    TEST_ASSERT_EQUAL_UINT32(1u, job.progress_current);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want_iv, iv, 4);
}

void test_cancel_is_honoured_immediately(void)
{
    const uint8_t serial[4] = {0x11, 0x22, 0x33, 0x44};
    uint8_t frame[11];
    fabricate_frame(serial, 0xA5u, 0x2Bu, 0x1u, 0x12345u, frame, NULL);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 100u));
    subghz_bruteforce_job_cancel(&job);

    TEST_ASSERT_FALSE(
        subghz_renault_v1_bf_run_range(&job, frame, 0u, 100u, NULL));
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_CANCELLED, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);
}

void test_run_range_rejects_invalid_arguments(void)
{
    uint8_t frame[11] = {0};
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 10u));

    TEST_ASSERT_FALSE(subghz_renault_v1_bf_run_range(NULL, frame, 0, 10, NULL));
    TEST_ASSERT_FALSE(
        subghz_renault_v1_bf_run_range(&job, NULL, 0, 10, NULL));
    TEST_ASSERT_FALSE(
        subghz_renault_v1_bf_run_range(&job, frame, 10, 5, NULL));
    TEST_ASSERT_FALSE(subghz_renault_v1_bf_run_range(&job, frame, 0,
                                                     RENAULT_V1_BF_CANDIDATES + 1u,
                                                     NULL));
    /* The invalid calls must not have consumed progress. */
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_RUNNING, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);

    TEST_ASSERT_FALSE(subghz_renault_v1_bf_run(NULL, frame, NULL));
    TEST_ASSERT_FALSE(subghz_renault_v1_bf_run(&job, NULL, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_frame_from_capture_packing);
    RUN_TEST(test_prepare_is_deterministic_and_keeps_serial);
    RUN_TEST(test_seed_and_counter_formulas);
    RUN_TEST(test_finds_fabricated_candidate_in_range);
    RUN_TEST(test_full_run_finds_fabricated_candidate);
    RUN_TEST(test_exhaustion_reports_not_found);
    RUN_TEST(test_hit_at_candidate_zero);
    RUN_TEST(test_cancel_is_honoured_immediately);
    RUN_TEST(test_run_range_rejects_invalid_arguments);
    return UNITY_END();
}
