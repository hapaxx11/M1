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
 *
 * Phase 9c-1 additions:
 *   - KeeLoq 16-bit counter decode / encode round-trip
 *   - Counter substitution preserves the lower 16 plaintext bits
 *   - Cross-check against keeloq_increment_hop() (counter+1 equivalence)
 *   - Distinct counters yield distinct ciphertext
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
/* Counter decode / encode (Phase 9c-1)                                        */
/*============================================================================*/

/* A representative device key — value is arbitrary but fixed so the
 * tests are deterministic.  Mirrors the style of the other KeeLoq
 * cipher test vectors elsewhere in the suite. */
#define TEST_DEVICE_KEY  0xA1B2C3D4E5F60718ULL

/* Build a plaintext hop word from its component fields, then encrypt
 * it to obtain a captured-style encrypted hop. */
static uint32_t make_enc_hop(uint16_t counter,
                             uint8_t  button,
                             uint8_t  vlow,
                             uint8_t  discriminant,
                             uint8_t  overflow,
                             uint64_t device_key)
{
    const uint32_t plain = ((uint32_t)counter       << 16) |
                           ((uint32_t)(button & 0xF) << 12) |
                           ((uint32_t)(vlow   & 0x3) << 10) |
                           ((uint32_t)(discriminant & 0x3F) << 4) |
                           (uint32_t)(overflow & 0xF);
    return keeloq_encrypt(plain, device_key);
}

static void test_counter_decode_recovers_known_counter(void)
{
    const uint16_t counter = 0x1234;
    uint32_t enc = make_enc_hop(counter, 0x5, 0x1, 0x2A, 0x0, TEST_DEVICE_KEY);

    uint16_t decoded =
        subghz_signal_fields_keeloq_counter_decode(enc, TEST_DEVICE_KEY);
    TEST_ASSERT_EQUAL_HEX16(counter, decoded);
}

static void test_counter_encode_round_trip(void)
{
    const uint16_t counters[] = {
        0x0000, 0x0001, 0x00FF, 0x1234, 0x8000, 0xABCD, 0xFFFF
    };
    /* A fixed captured-style encrypted hop word so we exercise the full
     * decrypt-substitute-encrypt path. */
    uint32_t enc = make_enc_hop(0xDEAD, 0x7, 0x0, 0x15, 0x3, TEST_DEVICE_KEY);

    for (size_t i = 0; i < sizeof(counters)/sizeof(counters[0]); ++i)
    {
        uint32_t new_enc =
            subghz_signal_fields_keeloq_counter_encode(enc, counters[i],
                                                       TEST_DEVICE_KEY);
        uint16_t back =
            subghz_signal_fields_keeloq_counter_decode(new_enc,
                                                       TEST_DEVICE_KEY);
        TEST_ASSERT_EQUAL_HEX16(counters[i], back);
    }
}

static void test_counter_encode_preserves_low_16_plain_bits(void)
{
    /* Distinct sentinel for each of the 16 plaintext bits below counter. */
    uint32_t enc = make_enc_hop(0xC0DE, 0xB, 0x2, 0x29, 0x7, TEST_DEVICE_KEY);

    /* Substituting a different counter must not perturb the lower 16
     * plaintext bits (button / VLOW / discriminant / overflow). */
    uint32_t new_enc =
        subghz_signal_fields_keeloq_counter_encode(enc, 0x1111,
                                                   TEST_DEVICE_KEY);

    uint32_t old_plain = keeloq_decrypt(enc,     TEST_DEVICE_KEY);
    uint32_t new_plain = keeloq_decrypt(new_enc, TEST_DEVICE_KEY);

    TEST_ASSERT_EQUAL_HEX32((uint32_t)0x1111U << 16, new_plain & 0xFFFF0000U);
    TEST_ASSERT_EQUAL_HEX32(old_plain & 0x0000FFFFU,
                            new_plain & 0x0000FFFFU);
}

static void test_counter_encode_matches_increment_hop(void)
{
    /* keeloq_increment_hop(h, k) must equal encode(h, decode(h, k)+1, k)
     * for any (h, k) — the two operations share a single source of truth. */
    const uint32_t hops[] = {
        0x00000000U, 0xDEADBEEFU, 0x12345678U, 0xFFFFFFFFU, 0xA5A5A5A5U
    };
    for (size_t i = 0; i < sizeof(hops)/sizeof(hops[0]); ++i)
    {
        uint32_t inc_ref = keeloq_increment_hop(hops[i], TEST_DEVICE_KEY);
        uint16_t cur =
            subghz_signal_fields_keeloq_counter_decode(hops[i], TEST_DEVICE_KEY);
        uint32_t inc_via_encode =
            subghz_signal_fields_keeloq_counter_encode(hops[i],
                                                       (uint16_t)(cur + 1U),
                                                       TEST_DEVICE_KEY);
        TEST_ASSERT_EQUAL_HEX32(inc_ref, inc_via_encode);
    }
}

static void test_counter_distinct_inputs_yield_distinct_ciphertext(void)
{
    uint32_t enc = make_enc_hop(0x0000, 0x1, 0x0, 0x00, 0x0, TEST_DEVICE_KEY);
    uint32_t a = subghz_signal_fields_keeloq_counter_encode(enc, 0x0001U,
                                                            TEST_DEVICE_KEY);
    uint32_t b = subghz_signal_fields_keeloq_counter_encode(enc, 0x0002U,
                                                            TEST_DEVICE_KEY);
    TEST_ASSERT_NOT_EQUAL(a, b);
}

static void test_counter_encode_masks_to_16_bits(void)
{
    /* The function takes a uint16_t, so any wider value the caller
     * passes is already masked by the C type system.  This test
     * documents that boundary explicitly: passing (uint16_t)0xFFFF
     * is accepted and round-trips correctly. */
    uint32_t enc = make_enc_hop(0x4242, 0xC, 0x1, 0x0A, 0x5, TEST_DEVICE_KEY);
    uint32_t new_enc =
        subghz_signal_fields_keeloq_counter_encode(enc, (uint16_t)0xFFFFU,
                                                   TEST_DEVICE_KEY);
    TEST_ASSERT_EQUAL_HEX16((uint16_t)0xFFFFU,
        subghz_signal_fields_keeloq_counter_decode(new_enc, TEST_DEVICE_KEY));
}

/*============================================================================*/
/* Phase 9e-1 — counter-edit capability probe                                  */
/*============================================================================*/

static void test_counter_edit_status_keeloq_family_supported(void)
{
    const char *reason = (const char *)0xDEADBEEFULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_SUPPORTED,
        subghz_signal_fields_counter_edit_status("KeeLoq", &reason));
    TEST_ASSERT_NOT_NULL(reason);
    TEST_ASSERT_EQUAL_STRING("", reason);

    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_SUPPORTED,
        subghz_signal_fields_counter_edit_status("Star Line", &reason));
    TEST_ASSERT_EQUAL_STRING("", reason);

    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_SUPPORTED,
        subghz_signal_fields_counter_edit_status("Jarolift", &reason));
    TEST_ASSERT_EQUAL_STRING("", reason);
}

static void test_counter_edit_status_deferred_protocols(void)
{
    const char *reason = NULL;

    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_DEFERRED,
        subghz_signal_fields_counter_edit_status("Nice FloR-S", &reason));
    TEST_ASSERT_NOT_NULL(reason);
    TEST_ASSERT_TRUE(strstr(reason, "Nice FloR-S") != NULL);

    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_DEFERRED,
        subghz_signal_fields_counter_edit_status("CAME Atomo", &reason));
    TEST_ASSERT_NOT_NULL(reason);
    TEST_ASSERT_TRUE(strstr(reason, "CAME Atomo") != NULL);

    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_DEFERRED,
        subghz_signal_fields_counter_edit_status("Alutech AT-4N", &reason));
    TEST_ASSERT_NOT_NULL(reason);
    TEST_ASSERT_TRUE(strstr(reason, "Alutech") != NULL);

    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_DEFERRED,
        subghz_signal_fields_counter_edit_status("Phoenix_V2", &reason));
    TEST_ASSERT_NOT_NULL(reason);
    TEST_ASSERT_TRUE(strstr(reason, "Phoenix V2") != NULL);
}

static void test_counter_edit_status_unsupported_protocols(void)
{
    const char *reason = NULL;

    /* Static-OOK families — not in scope for counter editing. */
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_UNSUPPORTED,
        subghz_signal_fields_counter_edit_status("Princeton", &reason));
    TEST_ASSERT_EQUAL_STRING("", reason);

    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_UNSUPPORTED,
        subghz_signal_fields_counter_edit_status("Nice FLO 12b", &reason));
    TEST_ASSERT_EQUAL_STRING("", reason);

    /* Unknown gibberish. */
    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_UNSUPPORTED,
        subghz_signal_fields_counter_edit_status("NotARealProtocol", &reason));
    TEST_ASSERT_EQUAL_STRING("", reason);
}

static void test_counter_edit_status_null_protocol(void)
{
    const char *reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_UNSUPPORTED,
        subghz_signal_fields_counter_edit_status(NULL, &reason));
    TEST_ASSERT_EQUAL_STRING("", reason);
}

static void test_counter_edit_status_null_out_reason_ok(void)
{
    /* The out_reason pointer is optional — passing NULL must not crash
     * and must still return the correct status. */
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_SUPPORTED,
        subghz_signal_fields_counter_edit_status("KeeLoq", NULL));
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_DEFERRED,
        subghz_signal_fields_counter_edit_status("Nice FloR-S", NULL));
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_UNSUPPORTED,
        subghz_signal_fields_counter_edit_status("Princeton", NULL));
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_UNSUPPORTED,
        subghz_signal_fields_counter_edit_status(NULL, NULL));
}

static void test_counter_edit_status_case_insensitive(void)
{
    const char *reason = NULL;

    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_SUPPORTED,
        subghz_signal_fields_counter_edit_status("keeloq", &reason));

    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_DEFERRED,
        subghz_signal_fields_counter_edit_status("nice flor-s", &reason));
    TEST_ASSERT_TRUE(strstr(reason, "Nice FloR-S") != NULL);

    reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_DEFERRED,
        subghz_signal_fields_counter_edit_status("CAME ATOMO", &reason));
}

static void test_counter_edit_status_trailing_space_match(void)
{
    /* The name-matching convention is NUL-or-space terminated, mirroring
     * is_keeloq_family().  A registry name like "Nice FloR-S " (trailing
     * space) must still classify correctly. */
    const char *reason = NULL;
    TEST_ASSERT_EQUAL(SUBGHZ_COUNTER_EDIT_DEFERRED,
        subghz_signal_fields_counter_edit_status("Nice FloR-S 433", &reason));
    TEST_ASSERT_TRUE(strstr(reason, "Nice FloR-S") != NULL);
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

    /* Phase 9c-1 — counter decode / encode pure logic */
    RUN_TEST(test_counter_decode_recovers_known_counter);
    RUN_TEST(test_counter_encode_round_trip);
    RUN_TEST(test_counter_encode_preserves_low_16_plain_bits);
    RUN_TEST(test_counter_encode_matches_increment_hop);
    RUN_TEST(test_counter_distinct_inputs_yield_distinct_ciphertext);
    RUN_TEST(test_counter_encode_masks_to_16_bits);

    /* Phase 9e-1 — counter-edit capability probe */
    RUN_TEST(test_counter_edit_status_keeloq_family_supported);
    RUN_TEST(test_counter_edit_status_deferred_protocols);
    RUN_TEST(test_counter_edit_status_unsupported_protocols);
    RUN_TEST(test_counter_edit_status_null_protocol);
    RUN_TEST(test_counter_edit_status_null_out_reason_ok);
    RUN_TEST(test_counter_edit_status_case_insensitive);
    RUN_TEST(test_counter_edit_status_trailing_space_match);
    return UNITY_END();
}
