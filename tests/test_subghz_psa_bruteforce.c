/* See COPYING.txt for license details. */

/*
 * Host tests for the PSA two-phase TEA brute-force core.
 *
 * Synthetic captures are built by forward-encrypting fabricated plaintext
 * with the working key of a chosen candidate, then packing the ciphertext
 * into Key/Key_2 capture fields via the inverse of the module's own packing
 * helpers.  No real key material is used.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "subghz_psa_bruteforce.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Test helpers                                                                */
/*============================================================================*/

/* Inverse of subghz_psa_bf_setup_byte_buffer + prepare_tea_data: given the
 * TEA ciphertext words (w0, w1) recover capture fields (key1, key2).
 * w0/w1 map to buffer[2..9]; buffer[0..1] are unconstrained (fixed marker
 * values are used for realism). */
static void make_capture(uint32_t w0, uint32_t w1, uint64_t *key1,
                         uint16_t *key2)
{
    uint8_t b[10];
    b[0] = 0x12u;
    b[1] = 0x3Au; /* low nibble 0xA = PSA acceptance marker */
    b[2] = (uint8_t)(w0 >> 24);
    b[3] = (uint8_t)((w0 >> 16) & 0xFFu);
    b[4] = (uint8_t)((w0 >> 8) & 0xFFu);
    b[5] = (uint8_t)(w0 & 0xFFu);
    b[6] = (uint8_t)(w1 >> 24);
    b[7] = (uint8_t)((w1 >> 16) & 0xFFu);
    b[8] = (uint8_t)((w1 >> 8) & 0xFFu);
    b[9] = (uint8_t)(w1 & 0xFFu);

    uint64_t k = 0;
    for (uint8_t i = 0; i < 8u; i++)
        k = (k << 8) | b[i];
    *key1 = k;
    *key2 = (uint16_t)(((uint16_t)b[8] << 8) | b[9]);
}

/* Expected field extraction, mirroring the module contract, so the tests can
 * assert results independently of the implementation's expression of them. */
static void expect_fields(uint32_t v0, uint32_t v1,
                          const subghz_psa_bf_result_t *r)
{
    const uint8_t b2 = (uint8_t)(v0 >> 24);
    const uint8_t b3 = (uint8_t)((v0 >> 16) & 0xFFu);
    const uint8_t b4 = (uint8_t)((v0 >> 8) & 0xFFu);
    const uint8_t b5 = (uint8_t)(v0 & 0xFFu);
    const uint8_t b6 = (uint8_t)(v1 >> 24);
    const uint8_t b7 = (uint8_t)((v1 >> 16) & 0xFFu);
    const uint8_t b8 = (uint8_t)((v1 >> 8) & 0xFFu);
    const uint8_t b9 = (uint8_t)(v1 & 0xFFu);

    TEST_ASSERT_EQUAL_UINT8((b5 >> 4) & 0x0Fu, r->button);
    TEST_ASSERT_EQUAL_HEX32(((uint32_t)b2 << 16) | ((uint32_t)b3 << 8) | b4,
                            r->serial);
    TEST_ASSERT_EQUAL_HEX32((((uint32_t)b5 & 0x0Fu) << 24) |
                                ((uint32_t)b6 << 16) | ((uint32_t)b7 << 8) | b8,
                            r->counter);
    TEST_ASSERT_EQUAL_UINT16(b9, r->crc);
    TEST_ASSERT_EQUAL_HEX32(r->serial, r->seed);
    TEST_ASSERT_EQUAL_UINT8(0x36u, r->type);
}

/*============================================================================*/
/* Primitive tests                                                             */
/*============================================================================*/

void test_tea_encrypt_decrypt_roundtrip(void)
{
    const uint32_t key[4] = {0x01234567u, 0x89ABCDEFu, 0xFEDCBA98u,
                             0x76543210u};
    uint32_t v0 = 0xDEADBEEFu, v1 = 0x0BADF00Du;
    subghz_psa_tea_encrypt(&v0, &v1, key);
    TEST_ASSERT_NOT_EQUAL(0xDEADBEEFu, v0);
    subghz_psa_tea_decrypt(&v0, &v1, key);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, v0);
    TEST_ASSERT_EQUAL_HEX32(0x0BADF00Du, v1);
}

void test_scheduled_encrypt_matches_plain_encrypt(void)
{
    const uint32_t key[4] = {0x11111111u, 0x22222222u, 0x33333333u,
                             0x44444444u};
    uint32_t a0 = 0x12345678u, a1 = 0x9ABCDEF0u;
    uint32_t b0 = a0, b1 = a1;

    subghz_psa_tea_schedule_t sched;
    subghz_psa_tea_build_schedule(key, &sched);
    subghz_psa_tea_encrypt_scheduled(&a0, &a1, &sched);
    subghz_psa_tea_encrypt(&b0, &b1, key);

    TEST_ASSERT_EQUAL_HEX32(b0, a0);
    TEST_ASSERT_EQUAL_HEX32(b1, a1);
}

void test_capture_packing_known_vector(void)
{
    uint8_t buf[10];
    subghz_psa_bf_setup_byte_buffer(buf, 0x55667788u, 0x11223344u, 0x99AAu);

    const uint8_t want[10] = {0x11, 0x22, 0x33, 0x44, 0x55,
                              0x66, 0x77, 0x88, 0x99, 0xAA};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, buf, 10);

    uint32_t w0, w1;
    subghz_psa_bf_prepare_tea_data(buf, &w0, &w1);
    TEST_ASSERT_EQUAL_HEX32(0x33445566u, w0);
    TEST_ASSERT_EQUAL_HEX32(0x778899AAu, w1);
}

void test_tea_crc_known_vector(void)
{
    /* 01+02+03+04 + 05+06+07 = 0x1C (v1 low byte excluded). */
    TEST_ASSERT_EQUAL_UINT8(0x1Cu,
                            subghz_psa_bf_tea_crc(0x01020304u, 0x05060708u));
}

void test_crc16_known_vector(void)
{
    /* CRC-16/BUYPASS (poly 0x8005, init 0, no reflection) of "123456789". */
    const uint8_t s[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX16(0xFEE8u, subghz_psa_bf_crc16(s, 9));
}

void test_extract_result_maps_fields(void)
{
    subghz_psa_bf_result_t r;
    memset(&r, 0, sizeof(r));
    subghz_psa_bf_extract_result(0xABCD0152u, 0x12345678u, 0x23ABCD01u, &r);

    expect_fields(0xABCD0152u, 0x12345678u, &r);
    TEST_ASSERT_EQUAL_HEX32(0x23ABCD01u, r.candidate);
}

/*============================================================================*/
/* Phase-1 search                                                              */
/*============================================================================*/

/* Fabricate a phase-1 capture that yields plaintext (v0, v1) under
 * candidate `cand`. */
static void fabricate_phase1(uint32_t cand, uint32_t v0, uint32_t v1,
                             uint64_t *key1, uint16_t *key2)
{
    subghz_psa_tea_schedule_t sched;
    subghz_psa_tea_build_schedule(subghz_psa_bf1_key_schedule, &sched);

    uint32_t wk2 = 0x0E0F5C41u, wk3 = cand;
    subghz_psa_tea_encrypt_scheduled(&wk2, &wk3, &sched);
    uint32_t wk0 = (cand << 8) | 0x0Eu, wk1 = 0x0F5C4123u;
    subghz_psa_tea_encrypt_scheduled(&wk0, &wk1, &sched);
    const uint32_t working_key[4] = {wk0, wk1, wk2, wk3};

    uint32_t w0 = v0, w1 = v1;
    subghz_psa_tea_encrypt(&w0, &w1, working_key);
    make_capture(w0, w1, key1, key2);
}

void test_phase1_finds_fabricated_capture(void)
{
    const uint32_t cand = 0x23ABCD01u;
    /* Plaintext: top 24 bits echo the candidate; low byte free (button in the
     * high nibble).  v1 low byte must equal the additive CRC. */
    uint32_t v0 = ((cand & 0xFFFFFFu) << 8) | 0x52u;
    uint32_t v1 = 0x12345600u;
    v1 |= subghz_psa_bf_tea_crc(v0, v1);

    uint64_t key1;
    uint16_t key2;
    fabricate_phase1(cand, v0, v1, &key1, &key2);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 5u));

    subghz_psa_bf_result_t r;
    memset(&r, 0, sizeof(r));
    TEST_ASSERT_TRUE(subghz_psa_bf_run_range(&job, key1, key2, PSA_BF_PHASE_1,
                                             cand - 2u, cand + 3u, &r));

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_HEX32(cand, job.result);
    TEST_ASSERT_EQUAL_UINT32(3u, job.progress_current);
    TEST_ASSERT_EQUAL_HEX32(cand, r.candidate);
    expect_fields(v0, v1, &r);
}

void test_phase1_hit_at_range_start(void)
{
    const uint32_t cand = PSA_BF1_START + 7u;
    uint32_t v0 = ((cand & 0xFFFFFFu) << 8) | 0x14u;
    uint32_t v1 = 0x00ABCD00u;
    v1 |= subghz_psa_bf_tea_crc(v0, v1);

    uint64_t key1;
    uint16_t key2;
    fabricate_phase1(cand, v0, v1, &key1, &key2);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 1u));

    subghz_psa_bf_result_t r;
    TEST_ASSERT_TRUE(subghz_psa_bf_run_range(&job, key1, key2, PSA_BF_PHASE_1,
                                             cand, cand + 1u, &r));
    TEST_ASSERT_EQUAL_HEX32(cand, r.candidate);
    TEST_ASSERT_EQUAL_UINT32(1u, job.progress_current);
}

void test_phase1_exhaustion_reports_not_found(void)
{
    const uint32_t cand = 0x23ABCD01u;
    uint32_t v0 = ((cand & 0xFFFFFFu) << 8) | 0x52u;
    uint32_t v1 = 0x12345600u;
    v1 |= subghz_psa_bf_tea_crc(v0, v1);

    uint64_t key1;
    uint16_t key2;
    fabricate_phase1(cand, v0, v1, &key1, &key2);

    /* Search a range that does not contain the winning candidate. */
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 8u));

    subghz_psa_bf_result_t r;
    TEST_ASSERT_FALSE(subghz_psa_bf_run_range(&job, key1, key2, PSA_BF_PHASE_1,
                                              cand + 10u, cand + 18u, &r));
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND, job.status);
    TEST_ASSERT_EQUAL_UINT32(8u, job.progress_current);
}

/*============================================================================*/
/* Phase-2 search                                                              */
/*============================================================================*/

static void fabricate_phase2(uint32_t cand, uint32_t v0, uint32_t v1,
                             uint64_t *key1, uint16_t *key2)
{
    const uint32_t working_key[4] = {
        subghz_psa_bf2_key_schedule[0] ^ cand,
        subghz_psa_bf2_key_schedule[1] ^ cand,
        subghz_psa_bf2_key_schedule[2] ^ cand,
        subghz_psa_bf2_key_schedule[3] ^ cand,
    };

    uint32_t w0 = v0, w1 = v1;
    subghz_psa_tea_encrypt(&w0, &w1, working_key);
    make_capture(w0, w1, key1, key2);
}

void test_phase2_finds_fabricated_capture(void)
{
    const uint32_t cand = 0xF3123456u;
    uint32_t v0 = ((cand & 0xFFFFFFu) << 8) | 0x27u;
    uint32_t v1 = 0x9ABC0000u;

    /* The CRC-16 covers bytes [v0>>24, (v0>>8), (v0>>16), v0, v1>>24,
     * (v1>>16)] and must equal the low 16 bits of v1. */
    const uint8_t crc_buf[6] = {
        (uint8_t)(v0 >> 24), (uint8_t)((v0 >> 8) & 0xFFu),
        (uint8_t)((v0 >> 16) & 0xFFu), (uint8_t)(v0 & 0xFFu),
        (uint8_t)(v1 >> 24), (uint8_t)((v1 >> 16) & 0xFFu),
    };
    v1 |= subghz_psa_bf_crc16(crc_buf, sizeof(crc_buf));

    uint64_t key1;
    uint16_t key2;
    fabricate_phase2(cand, v0, v1, &key1, &key2);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 5u));

    subghz_psa_bf_result_t r;
    memset(&r, 0, sizeof(r));
    TEST_ASSERT_TRUE(subghz_psa_bf_run_range(&job, key1, key2, PSA_BF_PHASE_2,
                                             cand - 2u, cand + 3u, &r));

    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_FOUND, job.status);
    TEST_ASSERT_EQUAL_HEX32(cand, job.result);
    TEST_ASSERT_EQUAL_HEX32(cand, r.candidate);
    expect_fields(v0, v1, &r);
}

void test_phase2_rejects_phase1_fabrication_and_vice_versa(void)
{
    /* A capture fabricated for one phase must not produce a hit in the other
     * phase over a range covering the same low-24-bit candidate value — the
     * two verification paths are independent. */
    const uint32_t cand2 = 0xF3123456u;
    uint32_t v0 = ((cand2 & 0xFFFFFFu) << 8) | 0x27u;
    uint32_t v1 = 0x9ABC0000u;
    const uint8_t crc_buf[6] = {
        (uint8_t)(v0 >> 24), (uint8_t)((v0 >> 8) & 0xFFu),
        (uint8_t)((v0 >> 16) & 0xFFu), (uint8_t)(v0 & 0xFFu),
        (uint8_t)(v1 >> 24), (uint8_t)((v1 >> 16) & 0xFFu),
    };
    v1 |= subghz_psa_bf_crc16(crc_buf, sizeof(crc_buf));

    uint64_t key1;
    uint16_t key2;
    fabricate_phase2(cand2, v0, v1, &key1, &key2);

    /* Same low 24 bits inside the phase-1 range. */
    const uint32_t cand1 = PSA_BF1_START | (cand2 & 0xFFFFFFu);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 3u));
    TEST_ASSERT_FALSE(subghz_psa_bf_run_range(&job, key1, key2, PSA_BF_PHASE_1,
                                              cand1 - 1u, cand1 + 2u, NULL));
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_NOT_FOUND, job.status);
}

/*============================================================================*/
/* Job integration                                                             */
/*============================================================================*/

void test_cancel_is_honoured_immediately(void)
{
    const uint32_t cand = 0x23ABCD01u;
    uint32_t v0 = ((cand & 0xFFFFFFu) << 8) | 0x52u;
    uint32_t v1 = 0x12345600u;
    v1 |= subghz_psa_bf_tea_crc(v0, v1);

    uint64_t key1;
    uint16_t key2;
    fabricate_phase1(cand, v0, v1, &key1, &key2);

    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 100u));
    subghz_bruteforce_job_cancel(&job);

    /* The very first candidate evaluation observes the cancel request. */
    TEST_ASSERT_FALSE(subghz_psa_bf_run_range(&job, key1, key2, PSA_BF_PHASE_1,
                                              cand - 2u, cand + 98u, NULL));
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_CANCELLED, job.status);
    TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);
}

void test_run_range_rejects_invalid_arguments(void)
{
    subghz_bruteforce_job_t job;
    subghz_bruteforce_job_init(&job);
    TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 10u));

    TEST_ASSERT_FALSE(
        subghz_psa_bf_run_range(NULL, 0, 0, PSA_BF_PHASE_1, 0, 10, NULL));
    TEST_ASSERT_FALSE(subghz_psa_bf_run_range(&job, 0, 0, (subghz_psa_bf_phase_t)0,
                                              0, 10, NULL));
    TEST_ASSERT_FALSE(subghz_psa_bf_run_range(&job, 0, 0, PSA_BF_PHASE_1,
                                              10, 5, NULL));
    /* NULL out is legal (no result extraction on hit). */
    TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_RUNNING, job.status);
}

void test_run_range_rejects_candidates_outside_phase_bounds(void)
{
    const struct {
        subghz_psa_bf_phase_t phase;
        uint32_t start;
        uint32_t end;
    } invalid_ranges[] = {
        {PSA_BF_PHASE_1, PSA_BF1_START - 1u, PSA_BF1_START},
        {PSA_BF_PHASE_1, PSA_BF1_END, PSA_BF1_END + 1u},
        {PSA_BF_PHASE_2, PSA_BF2_START - 1u, PSA_BF2_START},
        {PSA_BF_PHASE_2, PSA_BF2_END, PSA_BF2_END + 1u},
    };

    for (size_t i = 0; i < sizeof(invalid_ranges) / sizeof(invalid_ranges[0]); i++)
    {
        subghz_bruteforce_job_t job;
        subghz_bruteforce_job_init(&job);
        TEST_ASSERT_TRUE(subghz_bruteforce_job_begin(&job, 10u));
        TEST_ASSERT_FALSE(subghz_psa_bf_run_range(
            &job, 0, 0, invalid_ranges[i].phase,
            invalid_ranges[i].start, invalid_ranges[i].end, NULL));
        TEST_ASSERT_EQUAL(SUBGHZ_BRUTEFORCE_JOB_RUNNING, job.status);
        TEST_ASSERT_EQUAL_UINT32(0u, job.progress_current);
    }
}

void test_full_run_arms_total_candidate_count(void)
{
    /* The full two-phase search covers 2 × 2^24 candidates — too slow to
     * execute on the host, so only the contract edges are checked here:
     * the total constant and argument validation.  Cancellation and
     * per-phase behaviour are covered by the ranged tests above. */
    TEST_ASSERT_EQUAL_UINT32(2u * 0x01000000u, PSA_BF_TOTAL);

    TEST_ASSERT_FALSE(subghz_psa_bf_run(NULL, 0x1122334455667788u, 0x99AAu,
                                        NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tea_encrypt_decrypt_roundtrip);
    RUN_TEST(test_scheduled_encrypt_matches_plain_encrypt);
    RUN_TEST(test_capture_packing_known_vector);
    RUN_TEST(test_tea_crc_known_vector);
    RUN_TEST(test_crc16_known_vector);
    RUN_TEST(test_extract_result_maps_fields);
    RUN_TEST(test_phase1_finds_fabricated_capture);
    RUN_TEST(test_phase1_hit_at_range_start);
    RUN_TEST(test_phase1_exhaustion_reports_not_found);
    RUN_TEST(test_phase2_finds_fabricated_capture);
    RUN_TEST(test_phase2_rejects_phase1_fabrication_and_vice_versa);
    RUN_TEST(test_cancel_is_honoured_immediately);
    RUN_TEST(test_run_range_rejects_invalid_arguments);
    RUN_TEST(test_run_range_rejects_candidates_outside_phase_bounds);
    RUN_TEST(test_full_run_arms_total_candidate_count);
    return UNITY_END();
}
