/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_signal_fields.c
 * @brief  Unity tests for subghz_signal_fields — Phase 9a-1.
 *
 * Covers:
 *   - KeeLoq family detection (positive + negative cases)
 *   - KeeLoq / Jarolift layout: extract + assemble + round-trip
 *   - Star Line layout: extract + assemble + round-trip
 *   - Cross-protocol consistency with subghz_keeloq_create_key()
 *   - Bad-argument paths (NULL protocol / fields / out, unknown protocol)
 *   - Field masking (over-range serial / button)
 */

#include "unity.h"
#include "subghz_signal_fields.h"
#include "subghz_keeloq_create.h"
#include "subghz_keeloq.h"

#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/*============================================================================*/
/* Protocol detection                                                          */
/*============================================================================*/

static void test_is_keeloq_family_recognises_supported(void)
{
    TEST_ASSERT_TRUE(subghz_signal_fields_is_keeloq_family("KeeLoq"));
    TEST_ASSERT_TRUE(subghz_signal_fields_is_keeloq_family("Star Line"));
    TEST_ASSERT_TRUE(subghz_signal_fields_is_keeloq_family("Jarolift"));
}

static void test_is_keeloq_family_case_insensitive(void)
{
    TEST_ASSERT_TRUE(subghz_signal_fields_is_keeloq_family("keeloq"));
    TEST_ASSERT_TRUE(subghz_signal_fields_is_keeloq_family("KEELOQ"));
    TEST_ASSERT_TRUE(subghz_signal_fields_is_keeloq_family("star line"));
}

static void test_is_keeloq_family_accepts_trailing_space(void)
{
    /* Some protocol fields carry trailing variant tokens — accept the
     * prefix match terminated by space, matching the encoder's contract. */
    TEST_ASSERT_TRUE(subghz_signal_fields_is_keeloq_family("KeeLoq HCS101"));
    TEST_ASSERT_TRUE(subghz_signal_fields_is_keeloq_family("Star Line Twage"));
}

static void test_is_keeloq_family_rejects_others(void)
{
    TEST_ASSERT_FALSE(subghz_signal_fields_is_keeloq_family(NULL));
    TEST_ASSERT_FALSE(subghz_signal_fields_is_keeloq_family(""));
    TEST_ASSERT_FALSE(subghz_signal_fields_is_keeloq_family("Nice FloR-S"));
    TEST_ASSERT_FALSE(subghz_signal_fields_is_keeloq_family("CAME Atomo"));
    TEST_ASSERT_FALSE(subghz_signal_fields_is_keeloq_family("Princeton"));
    TEST_ASSERT_FALSE(subghz_signal_fields_is_keeloq_family("Keelo"));   /* partial — name_matches needs full prefix */
}

/*============================================================================*/
/* KeeLoq / Jarolift layout                                                    */
/*============================================================================*/

static void test_keeloq_extract_known_layout(void)
{
    /* Hand-built key: button = 0xA, serial = 0x0123456 (28 bits set across
     * the range), enc_hop = 0xDEADBEEF.
     *
     *   [63:60] = 0xA
     *   [59:32] = 0x0123456
     *   [31: 0] = 0xDEADBEEF
     */
    const uint64_t key = ((uint64_t)0xAULL << 60) |
                         ((uint64_t)0x0123456ULL << 32) |
                         (uint64_t)0xDEADBEEFULL;

    subghz_keeloq_fields_t f;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_extract("KeeLoq", key, &f));
    TEST_ASSERT_EQUAL_HEX32(0x0123456U, f.serial);
    TEST_ASSERT_EQUAL_HEX8(0xAU, f.button);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFU, f.enc_hop);
}

static void test_jarolift_uses_keeloq_layout(void)
{
    const uint64_t key = ((uint64_t)0x5ULL << 60) |
                         ((uint64_t)0x0FEDCBAULL << 32) |
                         (uint64_t)0x12345678ULL;

    subghz_keeloq_fields_t f;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_extract("Jarolift", key, &f));
    TEST_ASSERT_EQUAL_HEX32(0x0FEDCBAU, f.serial);
    TEST_ASSERT_EQUAL_HEX8(0x5U, f.button);
    TEST_ASSERT_EQUAL_HEX32(0x12345678U, f.enc_hop);
}

static void test_keeloq_assemble_known_layout(void)
{
    const subghz_keeloq_fields_t f = {
        .serial  = 0x0ABCDEFU,
        .button  = 0x3U,
        .enc_hop = 0xCAFEBABEU,
    };
    uint64_t key = 0xFFFFFFFFFFFFFFFFULL;     /* sentinel non-zero */
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_assemble("KeeLoq", &f, &key));

    const uint64_t expected = ((uint64_t)0x3ULL << 60) |
                              ((uint64_t)0x0ABCDEFULL << 32) |
                              (uint64_t)0xCAFEBABEULL;
    TEST_ASSERT_EQUAL_HEX64(expected, key);
}

/*============================================================================*/
/* Star Line layout                                                            */
/*============================================================================*/

static void test_star_line_extract_known_layout(void)
{
    /*   [63:32] = enc_hop
     *   [31: 4] = serial[27:0]
     *   [ 3: 0] = button[3:0]
     */
    const uint64_t key = ((uint64_t)0x11223344ULL << 32) |
                         ((uint64_t)0x0ABCDEFULL << 4)   |
                         (uint64_t)0x7ULL;

    subghz_keeloq_fields_t f;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_extract("Star Line", key, &f));
    TEST_ASSERT_EQUAL_HEX32(0x11223344U, f.enc_hop);
    TEST_ASSERT_EQUAL_HEX32(0x0ABCDEFU, f.serial);
    TEST_ASSERT_EQUAL_HEX8(0x7U, f.button);
}

static void test_star_line_assemble_known_layout(void)
{
    const subghz_keeloq_fields_t f = {
        .serial  = 0x0AAAAAAU,
        .button  = 0xFU,
        .enc_hop = 0xBADC0FFEU,
    };
    uint64_t key = 0xFFFFFFFFFFFFFFFFULL;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_assemble("Star Line", &f, &key));

    const uint64_t expected = ((uint64_t)0xBADC0FFEULL << 32) |
                              ((uint64_t)0x0AAAAAAULL << 4)   |
                              (uint64_t)0xFULL;
    TEST_ASSERT_EQUAL_HEX64(expected, key);
}

/*============================================================================*/
/* Round-trip invariants                                                       */
/*============================================================================*/

static void check_round_trip(const char *proto, const subghz_keeloq_fields_t *in)
{
    uint64_t key = 0ULL;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_assemble(proto, in, &key));

    subghz_keeloq_fields_t out;
    memset(&out, 0xCC, sizeof(out));
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_extract(proto, key, &out));

    TEST_ASSERT_EQUAL_HEX32(in->serial & 0x0FFFFFFFU, out.serial);
    TEST_ASSERT_EQUAL_HEX8 (in->button & 0x0FU,       out.button);
    TEST_ASSERT_EQUAL_HEX32(in->enc_hop,              out.enc_hop);
}

static void test_round_trip_keeloq_varied(void)
{
    const subghz_keeloq_fields_t cases[] = {
        { .serial = 0x0000000U, .button = 0x0, .enc_hop = 0x00000000U },
        { .serial = 0x0FFFFFFFU, .button = 0xF, .enc_hop = 0xFFFFFFFFU },
        { .serial = 0x0123456U, .button = 0x5, .enc_hop = 0xDEADBEEFU },
        { .serial = 0x0ABCDEFU, .button = 0xA, .enc_hop = 0xCAFEBABEU },
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i)
    {
        check_round_trip("KeeLoq",   &cases[i]);
        check_round_trip("Jarolift", &cases[i]);
        check_round_trip("Star Line",&cases[i]);
    }
}

/*============================================================================*/
/* Cross-check against subghz_keeloq_create                                    */
/*============================================================================*/

/* The Phase 8c-3 create flow builds a 64-bit Flipper key from
 * {serial, button, counter, mfr_key, learn_type}.  Our extractor MUST be
 * able to reverse the {serial, button} half of that and re-assemble it
 * byte-for-byte. */
static void test_cross_check_with_keeloq_create(void)
{
    const KeeLoqCreateParams p = {
        .protocol   = "KeeLoq",
        .serial     = 0x0123456U,
        .button     = 0x4,
        .counter    = 0x0042U,
        .mfr_key    = 0x0123456789ABCDEFULL,
        .learn_type = KEELOQ_LEARN_NORMAL,
    };

    uint64_t created = 0ULL;
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_OK,
                          subghz_keeloq_create_key(&p, &created));

    subghz_keeloq_fields_t f;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_extract("KeeLoq", created, &f));
    TEST_ASSERT_EQUAL_HEX32(p.serial, f.serial);
    TEST_ASSERT_EQUAL_HEX8 (p.button, f.button);
    /* enc_hop is opaque (depends on cipher), but must reassemble to the
     * exact same 64-bit key. */
    uint64_t reassembled = 0ULL;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_assemble("KeeLoq", &f, &reassembled));
    TEST_ASSERT_EQUAL_HEX64(created, reassembled);
}

static void test_cross_check_with_star_line_create(void)
{
    const KeeLoqCreateParams p = {
        .protocol   = "Star Line",
        .serial     = 0x0ABCDEFU,
        .button     = 0x2,
        .counter    = 0x1234U,
        .mfr_key    = 0xFEDCBA9876543210ULL,
        .learn_type = KEELOQ_LEARN_SIMPLE,
    };

    uint64_t created = 0ULL;
    TEST_ASSERT_EQUAL_INT(KEELOQ_CREATE_OK,
                          subghz_keeloq_create_key(&p, &created));

    subghz_keeloq_fields_t f;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_extract("Star Line", created, &f));
    TEST_ASSERT_EQUAL_HEX32(p.serial, f.serial);
    TEST_ASSERT_EQUAL_HEX8 (p.button, f.button);

    uint64_t reassembled = 0ULL;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_assemble("Star Line", &f, &reassembled));
    TEST_ASSERT_EQUAL_HEX64(created, reassembled);
}

/*============================================================================*/
/* Bad-argument paths                                                          */
/*============================================================================*/

static void test_extract_rejects_null_out(void)
{
    TEST_ASSERT_FALSE(subghz_signal_fields_keeloq_extract("KeeLoq", 0xDEADBEEF, NULL));
}

static void test_extract_rejects_null_protocol(void)
{
    subghz_keeloq_fields_t f;
    memset(&f, 0xCC, sizeof(f));
    TEST_ASSERT_FALSE(subghz_signal_fields_keeloq_extract(NULL, 0xDEADBEEF, &f));
    /* Failed extracts must zero out the result to avoid surfacing garbage. */
    TEST_ASSERT_EQUAL_UINT32(0U, f.serial);
    TEST_ASSERT_EQUAL_UINT8 (0U, f.button);
    TEST_ASSERT_EQUAL_UINT32(0U, f.enc_hop);
}

static void test_extract_rejects_unknown_protocol(void)
{
    subghz_keeloq_fields_t f;
    memset(&f, 0xCC, sizeof(f));
    TEST_ASSERT_FALSE(subghz_signal_fields_keeloq_extract("Nice FloR-S",
                                                           0xDEADBEEF, &f));
    TEST_ASSERT_EQUAL_UINT32(0U, f.serial);
    TEST_ASSERT_EQUAL_UINT8 (0U, f.button);
    TEST_ASSERT_EQUAL_UINT32(0U, f.enc_hop);
}

static void test_assemble_rejects_null_key_out(void)
{
    const subghz_keeloq_fields_t f = { .serial = 1U, .button = 1U, .enc_hop = 1U };
    TEST_ASSERT_FALSE(subghz_signal_fields_keeloq_assemble("KeeLoq", &f, NULL));
}

static void test_assemble_rejects_null_fields(void)
{
    uint64_t key = 0xDEADBEEFULL;
    TEST_ASSERT_FALSE(subghz_signal_fields_keeloq_assemble("KeeLoq", NULL, &key));
    TEST_ASSERT_EQUAL_HEX64(0ULL, key);     /* zeroed on failure */
}

static void test_assemble_rejects_unknown_protocol(void)
{
    const subghz_keeloq_fields_t f = { .serial = 1U, .button = 1U, .enc_hop = 1U };
    uint64_t key = 0xDEADBEEFULL;
    TEST_ASSERT_FALSE(subghz_signal_fields_keeloq_assemble("Princeton", &f, &key));
    TEST_ASSERT_EQUAL_HEX64(0ULL, key);
}

/*============================================================================*/
/* Field masking — over-range inputs must not bleed into adjacent fields       */
/*============================================================================*/

static void test_assemble_masks_overrange_serial(void)
{
    /* serial = 0xFFFFFFFF (32 bits set) — only the low 28 bits should
     * survive into the assembled key. */
    const subghz_keeloq_fields_t f = {
        .serial  = 0xFFFFFFFFU,
        .button  = 0x0,
        .enc_hop = 0x00000000U,
    };
    uint64_t key = 0ULL;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_assemble("KeeLoq", &f, &key));

    /* [63:60] button = 0, [59:32] serial = 0xFFFFFFF, [31:0] enc_hop = 0 */
    const uint64_t expected = ((uint64_t)0x0FFFFFFFULL << 32);
    TEST_ASSERT_EQUAL_HEX64(expected, key);
}

static void test_assemble_masks_overrange_button(void)
{
    /* button = 0xFF (8 bits set) — only the low 4 bits should survive. */
    const subghz_keeloq_fields_t f = {
        .serial  = 0x00000000U,
        .button  = 0xFFU,
        .enc_hop = 0x00000000U,
    };
    uint64_t key = 0ULL;
    TEST_ASSERT_TRUE(subghz_signal_fields_keeloq_assemble("KeeLoq", &f, &key));

    /* button bits land at [63:60] only. */
    const uint64_t expected = ((uint64_t)0xFULL << 60);
    TEST_ASSERT_EQUAL_HEX64(expected, key);
}

/*============================================================================*/
/* Test runner                                                                 */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_is_keeloq_family_recognises_supported);
    RUN_TEST(test_is_keeloq_family_case_insensitive);
    RUN_TEST(test_is_keeloq_family_accepts_trailing_space);
    RUN_TEST(test_is_keeloq_family_rejects_others);

    RUN_TEST(test_keeloq_extract_known_layout);
    RUN_TEST(test_jarolift_uses_keeloq_layout);
    RUN_TEST(test_keeloq_assemble_known_layout);

    RUN_TEST(test_star_line_extract_known_layout);
    RUN_TEST(test_star_line_assemble_known_layout);

    RUN_TEST(test_round_trip_keeloq_varied);

    RUN_TEST(test_cross_check_with_keeloq_create);
    RUN_TEST(test_cross_check_with_star_line_create);

    RUN_TEST(test_extract_rejects_null_out);
    RUN_TEST(test_extract_rejects_null_protocol);
    RUN_TEST(test_extract_rejects_unknown_protocol);
    RUN_TEST(test_assemble_rejects_null_key_out);
    RUN_TEST(test_assemble_rejects_null_fields);
    RUN_TEST(test_assemble_rejects_unknown_protocol);

    RUN_TEST(test_assemble_masks_overrange_serial);
    RUN_TEST(test_assemble_masks_overrange_button);
    return UNITY_END();
}
