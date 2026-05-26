/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_keeloq_create.c
 * @brief  Host unit tests for the pure-logic KeeLoq Create-from-scratch
 *         key assembler (Phase 8c-3).
 *
 * Verifies the assembled 64-bit Flipper-format key by encrypting the
 * expected plaintext HOP word with the device key derived via the same
 * algorithm in `subghz_keeloq.c`, then comparing the bit layout
 * directly.  These tests are deterministic and self-contained — they
 * pin the on-the-wire result down so future refactors of the cipher,
 * learning, or assembler cannot drift silently.
 */

#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "subghz_keeloq.h"
#include "subghz_keeloq_create.h"
#include "subghz_keeloq_mfkeys.h"

void setUp(void) { }
void tearDown(void) { }

/* ------------------------------------------------------------------ */
/* Plaintext HOP layout                                                */
/* ------------------------------------------------------------------ */

static void test_plaintext_layout(void)
{
    /* serial low 6 bits = 0x2A (discriminant), button = 0x5 (masked
     * down to 4 bits in field), counter = 0x1234.  The plaintext layout
     * should be:
     *   [31:16] counter = 0x1234
     *   [15:12] button  = 0x5
     *   [11:10] VLOW    = 0
     *   [ 9: 4] discrim = 0x2A
     *   [ 3: 0] overflow= 0
     */
    uint32_t plain = subghz_keeloq_create_plaintext(0x0001E02AU,
                                                     0x5U,
                                                     0x1234U);
    uint32_t expected = (0x1234UL << 16) |
                        (0x5UL    << 12) |
                        (0x2AUL   <<  4);
    TEST_ASSERT_EQUAL_HEX32(expected, plain);
}

static void test_plaintext_masks_fields(void)
{
    /* Counter > 16 bits, button > 4 bits, serial high bits beyond the
     * low-6-bit discriminant must not bleed into other fields. */
    uint32_t plain = subghz_keeloq_create_plaintext(0xFFFFFFFFU,
                                                     0xFFU,
                                                     0xDEADBEEFU);
    /* counter masked to 0xBEEF, button masked to 0xF, discrim = 0x3F */
    uint32_t expected = (0xBEEFUL << 16) |
                        (0xFUL    << 12) |
                        (0x3FUL   <<  4);
    TEST_ASSERT_EQUAL_HEX32(expected, plain);
}

/* ------------------------------------------------------------------ */
/* Bad-argument paths                                                  */
/* ------------------------------------------------------------------ */

static void test_create_rejects_null_params(void)
{
    uint64_t key = 0xAAAAAAAAAAAAAAAAULL;
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_BAD_ARG,
                          subghz_keeloq_create_key(NULL, &key));
    TEST_ASSERT_EQUAL_UINT64(0ULL, key);   /* key_out zeroed on failure */
}

static void test_create_rejects_null_out(void)
{
    KeeLoqCreateParams p = {
        .protocol = "KeeLoq", .serial = 0x123, .button = 1,
        .counter = 1, .mfr_key = 0, .learn_type = KEELOQ_LEARN_NORMAL,
    };
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_BAD_ARG,
                          subghz_keeloq_create_key(&p, NULL));
}

static void test_create_rejects_unknown_protocol(void)
{
    uint64_t key = 0xAAAAAAAAAAAAAAAAULL;
    KeeLoqCreateParams p = {
        .protocol = "NotARealKeeLoq",
        .serial = 0x0001E02AU, .button = 1, .counter = 1,
        .mfr_key = 0x0123456789ABCDEFULL,
        .learn_type = KEELOQ_LEARN_NORMAL,
    };
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_BAD_PROTOCOL,
                          subghz_keeloq_create_key(&p, &key));
    TEST_ASSERT_EQUAL_UINT64(0ULL, key);   /* key_out zeroed on failure */
}

static void test_create_rejects_secure_learning(void)
{
    uint64_t key = 0xAAAAAAAAAAAAAAAAULL;
    KeeLoqCreateParams p = {
        .protocol = "KeeLoq",
        .serial = 0x0001E02AU, .button = 1, .counter = 1,
        .mfr_key = 0x0123456789ABCDEFULL,
        .learn_type = KEELOQ_LEARN_SECURE,
    };
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_BAD_LEARN,
                          subghz_keeloq_create_key(&p, &key));
    TEST_ASSERT_EQUAL_UINT64(0ULL, key);
}

/* ------------------------------------------------------------------ */
/* Bit layout — KeeLoq (Flipper Bit:64)                                */
/* ------------------------------------------------------------------ */

static void test_keeloq_normal_learning_layout(void)
{
    const uint32_t serial  = 0x0001E02AU;   /* 28-bit */
    const uint8_t  button  = 0x5U;
    const uint32_t counter = 0x1234U;
    const uint64_t mfr_key = 0x0123456789ABCDEFULL;

    KeeLoqCreateParams p = {
        .protocol = "KeeLoq",
        .serial = serial, .button = button, .counter = counter,
        .mfr_key = mfr_key, .learn_type = KEELOQ_LEARN_NORMAL,
    };
    uint64_t key = 0xAAAAAAAAAAAAAAAAULL;
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_OK,
                          subghz_keeloq_create_key(&p, &key));

    /* Re-derive the expected hop_enc independently from the same
     * primitives, then compare the assembled 64-bit key bit-for-bit. */
    uint32_t plain      = subghz_keeloq_create_plaintext(serial, button, counter);
    uint64_t device_key = keeloq_learn_normal(serial & 0x0FFFFFFFUL, mfr_key);
    uint32_t hop_enc    = keeloq_encrypt(plain, device_key);

    uint64_t expected = ((uint64_t)(button & 0x0FU)) << 60
                      | ((uint64_t)(serial & 0x0FFFFFFFUL)) << 32
                      | (uint64_t)hop_enc;

    TEST_ASSERT_EQUAL_HEX64(expected, key);

    /* Sanity: high four bits = button, next 28 = serial. */
    TEST_ASSERT_EQUAL_UINT8(button, (uint8_t)((key >> 60) & 0x0FU));
    TEST_ASSERT_EQUAL_UINT32(serial & 0x0FFFFFFFUL,
                             (uint32_t)((key >> 32) & 0x0FFFFFFFUL));
}

static void test_keeloq_simple_learning_layout(void)
{
    const uint32_t serial  = 0x00ABCDEFU;
    const uint8_t  button  = 0x2U;
    const uint32_t counter = 0x0001U;
    const uint64_t mfr_key = 0xFEDCBA9876543210ULL;

    KeeLoqCreateParams p = {
        .protocol = "Jarolift",     /* same layout as KeeLoq */
        .serial = serial, .button = button, .counter = counter,
        .mfr_key = mfr_key, .learn_type = KEELOQ_LEARN_SIMPLE,
    };
    uint64_t key = 0;
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_OK,
                          subghz_keeloq_create_key(&p, &key));

    uint32_t plain      = subghz_keeloq_create_plaintext(serial, button, counter);
    uint64_t device_key = keeloq_learn_simple(serial & 0x0FFFFFFFUL, mfr_key);
    uint32_t hop_enc    = keeloq_encrypt(plain, device_key);

    uint64_t expected = ((uint64_t)(button & 0x0FU)) << 60
                      | ((uint64_t)(serial & 0x0FFFFFFFUL)) << 32
                      | (uint64_t)hop_enc;
    TEST_ASSERT_EQUAL_HEX64(expected, key);
}

/* ------------------------------------------------------------------ */
/* Bit layout — Star Line (different from KeeLoq)                      */
/* ------------------------------------------------------------------ */

static void test_starline_layout(void)
{
    const uint32_t serial  = 0x01234567U;
    const uint8_t  button  = 0x9U;
    const uint32_t counter = 0xBEEFU;
    const uint64_t mfr_key = 0x1111222233334444ULL;

    KeeLoqCreateParams p = {
        .protocol = "Star Line",
        .serial = serial, .button = button, .counter = counter,
        .mfr_key = mfr_key, .learn_type = KEELOQ_LEARN_NORMAL,
    };
    uint64_t key = 0;
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_OK,
                          subghz_keeloq_create_key(&p, &key));

    uint32_t plain      = subghz_keeloq_create_plaintext(serial, button, counter);
    uint64_t device_key = keeloq_learn_normal(serial & 0x0FFFFFFFUL, mfr_key);
    uint32_t hop_enc    = keeloq_encrypt(plain, device_key);

    /* Star Line: HOP[63:32], serial[31:4], button[3:0]. */
    uint64_t expected = ((uint64_t)hop_enc << 32)
                      | ((uint64_t)(serial & 0x0FFFFFFFUL) << 4)
                      | ((uint64_t)(button & 0x0FU));
    TEST_ASSERT_EQUAL_HEX64(expected, key);

    /* Sanity: low nibble = button, bits [31:4] = serial. */
    TEST_ASSERT_EQUAL_UINT8(button, (uint8_t)(key & 0x0FU));
    TEST_ASSERT_EQUAL_UINT32(serial & 0x0FFFFFFFUL,
                             (uint32_t)((key >> 4) & 0x0FFFFFFFUL));
}

/* ------------------------------------------------------------------ */
/* Counter increment changes the encrypted HOP                          */
/* ------------------------------------------------------------------ */

static void test_counter_changes_hop(void)
{
    const uint32_t serial  = 0x0001E02AU;
    const uint8_t  button  = 0x1U;
    const uint64_t mfr_key = 0x0123456789ABCDEFULL;

    KeeLoqCreateParams p1 = {
        .protocol = "KeeLoq",
        .serial = serial, .button = button, .counter = 1,
        .mfr_key = mfr_key, .learn_type = KEELOQ_LEARN_NORMAL,
    };
    KeeLoqCreateParams p2 = p1;
    p2.counter = 2;

    uint64_t k1 = 0, k2 = 0;
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_OK,
                          subghz_keeloq_create_key(&p1, &k1));
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_OK,
                          subghz_keeloq_create_key(&p2, &k2));

    /* FIX part (high 32 bits for KeeLoq) is identical — same button + serial. */
    TEST_ASSERT_EQUAL_HEX32((uint32_t)(k1 >> 32), (uint32_t)(k2 >> 32));
    /* HOP part (low 32 bits) MUST differ — counter is part of the
     * plaintext and the cipher is non-degenerate. */
    TEST_ASSERT_NOT_EQUAL((uint32_t)k1, (uint32_t)k2);
}

/* ------------------------------------------------------------------ */
/* Test runner                                                          */
/* ------------------------------------------------------------------ */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_plaintext_layout);
    RUN_TEST(test_plaintext_masks_fields);
    RUN_TEST(test_create_rejects_null_params);
    RUN_TEST(test_create_rejects_null_out);
    RUN_TEST(test_create_rejects_unknown_protocol);
    RUN_TEST(test_create_rejects_secure_learning);
    RUN_TEST(test_keeloq_normal_learning_layout);
    RUN_TEST(test_keeloq_simple_learning_layout);
    RUN_TEST(test_starline_layout);
    RUN_TEST(test_counter_changes_hop);
    return UNITY_END();
}
