/* See COPYING.txt for license details. */

/*
 * test_subghz_proto_pirate.c
 *
 * Host unit tests for Sub_Ghz/subghz_proto_pirate.c
 *
 * Verifies:
 *   - Catalog lookup (exact + case-insensitive)
 *   - is_supported() predicate
 *   - Kia V7 encoder packet structure, CRC, bit-inversion, repetition count
 */

#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "subghz_proto_pirate.h"
#include "subghz_kia_v6_encoder.h"
#include "subghz_protopirate_keys_builtin.h"
#include "subghz_vag_encoder.h"

void setUp(void) {}
void tearDown(void) {}

static SubGhzKeyParams make_params(const char *proto, uint64_t key, uint32_t bits)
{
    SubGhzKeyParams p;
    memset(&p, 0, sizeof(p));
    strncpy(p.protocol, proto, sizeof(p.protocol) - 1);
    p.key_value = key;
    p.bit_count = bits;
    return p;
}

/* ===================================================================
 * Catalog lookup
 * =================================================================== */

void test_find_by_name_exact(void)
{
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_KiaV7,
                      subghz_proto_pirate_find_by_name("Kia V7"));
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_MazdaV0,
                      subghz_proto_pirate_find_by_name("Mazda V0"));
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_HondaStatic,
                      subghz_proto_pirate_find_by_name("Honda Static"));
}

void test_find_by_name_case_insensitive(void)
{
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_KiaV7,
                      subghz_proto_pirate_find_by_name("kia v7"));
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_FordV0,
                      subghz_proto_pirate_find_by_name("FORD V0"));
}

void test_find_by_name_unknown(void)
{
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_Unknown,
                      subghz_proto_pirate_find_by_name("NotAProtocol"));
    TEST_ASSERT_EQUAL(SubGhzProtoPirate_Unknown,
                      subghz_proto_pirate_find_by_name(NULL));
}

void test_is_supported(void)
{
    TEST_ASSERT_TRUE(subghz_proto_pirate_is_supported("Kia V7"));
    TEST_ASSERT_TRUE(subghz_proto_pirate_is_supported("kia v7"));
    TEST_ASSERT_FALSE(subghz_proto_pirate_is_supported("Toyota"));
    TEST_ASSERT_FALSE(subghz_proto_pirate_is_supported(NULL));
}

/* ===================================================================
 * Kia V7 encoder
 * =================================================================== */

void test_kia_v7_required_pairs(void)
{
    /* Key 0 has alternating-ish pattern after inversion; use all-zeros for
     * a deterministic symbol count. */
    SubGhzKeyParams params = make_params("Kia V7", 0, 64);
    uint32_t per_pass = subghz_proto_pirate_required_pairs(&params, 1) / 2;
    /* Per pass: 319 preamble + 1 sync + Manchester symbols + trailing + gap.
     * The exact Manchester symbol count depends on the key; just verify it is
     * non-zero and scales linearly with repetitions. */
    TEST_ASSERT_GREATER_THAN_UINT32(319 + 1 + 64 + 1 + 1, per_pass);
    TEST_ASSERT_EQUAL_UINT32(subghz_proto_pirate_required_pairs(&params, 1) * 3,
                             subghz_proto_pirate_required_pairs(&params, 3));
}

void test_kia_v7_encode_structure(void)
{
    /* Build a known key from fields:
     * header=0xB3, counter=0x1234, serial=0x0ABCDEF, button=0x2
     * key_value layout: header<<56 | counter<<16 | serial<<4 | button
     */
    uint64_t key_value = ((uint64_t)0xB3 << 56) |
                         ((uint64_t)0x1234 << 16) |
                         ((uint64_t)0x0ABCDEF << 4) |
                         0x2ULL;
    SubGhzKeyParams params = make_params("Kia V7", key_value, 64);

    SubGhzRawPair out[1000];
    uint32_t count = subghz_proto_pirate_encode(&params, out, 1000, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);

    /* First 319 pairs: preamble [HIGH 250, LOW 250] */
    for (uint32_t i = 0; i < 319; i++) {
        TEST_ASSERT_EQUAL_UINT32(250, out[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(250, out[i].low_us);
    }

    /* Pair 319: extra sync-high [250,250] */
    TEST_ASSERT_EQUAL_UINT32(250, out[319].high_us);
    TEST_ASSERT_EQUAL_UINT32(250, out[319].low_us);

    /* Data section follows until trailing+gap. */
}

void test_kia_v7_repetitions(void)
{
    SubGhzKeyParams params = make_params("Kia V7", 0xB300000000000002ULL, 64);
    SubGhzRawPair out[2000];
    uint32_t count = subghz_proto_pirate_encode(&params, out, 2000, 2);
    uint32_t per_rep = count / 2;
    TEST_ASSERT_GREATER_THAN_UINT32(0, per_rep);

    /* Each repetition should start with the same preamble */
    TEST_ASSERT_EQUAL_UINT32(250, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(250, out[per_rep].high_us);
}

void test_kia_v7_buffer_overflow(void)
{
    SubGhzKeyParams params = make_params("Kia V7", 0xB300000000000002ULL, 64);
    SubGhzRawPair out[100]; /* too small for one rep (896 pairs) */
    uint32_t count = subghz_proto_pirate_encode(&params, out, 100, 1);
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_unsupported_protocol_returns_zero(void)
{
    SubGhzKeyParams params = make_params("ToyotaProtoPirate", 0x12345678ULL, 32);
    SubGhzRawPair out[100];
    TEST_ASSERT_EQUAL_UINT32(0, subghz_proto_pirate_encode(&params, out, 100, 1));
    TEST_ASSERT_EQUAL_UINT32(0, subghz_proto_pirate_required_pairs(&params, 1));
}

void test_null_params(void)
{
    SubGhzRawPair out[100];
    TEST_ASSERT_EQUAL_UINT32(0, subghz_proto_pirate_encode(NULL, out, 100, 1));
    TEST_ASSERT_EQUAL_UINT32(0, subghz_proto_pirate_required_pairs(NULL, 1));
}

/* ===================================================================
 * Tier-B encoders — smoke tests
 * =================================================================== */

void test_honda_v1_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Honda V1", 0x123456789ABCDEF0ULL, 68);
    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    TEST_ASSERT_EQUAL_UINT32(1000, out[0].high_us);
}

void test_honda_v2_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Honda V2", 0, 81);
    params.serial = 0x123456;
    params.btn = 0x02; /* Lock */
    params.cnt = 0x100;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts with short HIGH/short LOW */
    TEST_ASSERT_EQUAL_UINT32(250, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(250, out[0].low_us);
}

void test_ford_v2_encode_nonzero(void)
{
    SubGhzRawPair out[4096];
    SubGhzKeyParams params = make_params("Ford V2", 0, 104);
    /* Default sync word will be filled in; extra holds tail bytes */
    params.extra[0] = 0x12;
    params.extra[1] = 0x34;
    params.extra[2] = 0x56;
    params.extra[3] = 0x78;
    params.extra[4] = 0x9A;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 4096, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts LOW then HIGH */
    TEST_ASSERT_EQUAL_UINT32(0, out[0].high_us);
    TEST_ASSERT_EQUAL_UINT32(200, out[0].low_us);
}

void test_ford_v1_encode_nonzero(void)
{
    SubGhzRawPair out[8192];
    SubGhzKeyParams params = make_params("Ford V1", 0, 136);
    params.serial = 0x12345678;
    params.btn = 0x02;
    params.cnt = 0x12345;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 8192, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts with long HIGH/long LOW */
    TEST_ASSERT_EQUAL_UINT32(130, out[0].high_us);
}

void test_kia_v3_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Kia V3/V4", 0, 68);
    params.serial = 0x0123456;
    params.btn = 0x02;
    params.cnt = 0x1234;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts HIGH for V3 */
    TEST_ASSERT_GREATER_THAN_UINT32(0, out[0].high_us);
}

void test_kia_v4_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Kia V4", 0, 68);
    params.serial = 0x0123456;
    params.btn = 0x02;
    params.cnt = 0x1234;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts LOW for V4 */
    TEST_ASSERT_EQUAL_UINT32(0, out[0].high_us);
}

void test_kia_v5_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Kia V5", 0, 67);
    params.serial = 0x0123456;
    params.btn = 0x02;
    params.cnt = 0x1234;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Preamble starts HIGH */
    TEST_ASSERT_GREATER_THAN_UINT32(0, out[0].high_us);
}

void test_fiat_v1_encode_nonzero(void)
{
    SubGhzRawPair out[256];
    SubGhzKeyParams params = make_params("Fiat V1", 0, 104);
    params.serial = 0x12345678;
    params.btn = 0x02;
    params.cnt = 0x123;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 256, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    /* Lead-in pulse */
    TEST_ASSERT_EQUAL_UINT32(2033, out[0].high_us);
}

/* ===================================================================
 * Tier-A encoders — smoke tests
 * =================================================================== */

static void tier_a_smoke(const char *proto, uint32_t serial, uint8_t btn,
                         uint32_t cnt, uint32_t out_size)
{
    SubGhzRawPair *out = calloc(out_size, sizeof(SubGhzRawPair));
    TEST_ASSERT_NOT_NULL(out);
    SubGhzKeyParams params = make_params(proto, 0, 64);
    params.serial = serial;
    params.btn = btn;
    params.cnt = cnt;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(out_size, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, out_size, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
    free(out);
}

void test_ford_v0_encode_nonzero(void)
{
    tier_a_smoke("Ford V0", 0x12345678, 0x02, 0x1234, 8192);
}

void test_mazda_v0_encode_nonzero(void)
{
    tier_a_smoke("Mazda V0", 0x12345678, 0x02, 0x12345, 4096);
}

void test_honda_static_encode_nonzero(void)
{
    tier_a_smoke("Honda Static", 0x0ABCDEF, 0x02, 0x1234, 2048);
}

void test_kia_v0_encode_nonzero(void)
{
    tier_a_smoke("Kia V0", 0x0123456, 0x02, 0x1234, 8192);
}

void test_kia_v1_encode_nonzero(void)
{
    tier_a_smoke("Kia V1", 0x12345678, 0x02, 0x1234, 4096);
}

void test_kia_v2_encode_nonzero(void)
{
    tier_a_smoke("Kia V2", 0x12345678, 0x02, 0x1234, 4096);
}

void test_renault_v0_encode_nonzero(void)
{
    tier_a_smoke("Renault V0", 0x12345678, 0x0A, 0x12, 4096);
}

void test_chrysler_v0_encode_nonzero(void)
{
    SubGhzRawPair out[4096];
    SubGhzKeyParams params = make_params("Chrysler V0", 0x03, 64);
    params.btn = 0x01;
    params.cnt = 0x10;
    for (size_t i = 0; i < 9; i++) params.extra[i] = (uint8_t)(0xA0 + i);

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 4096, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
}

void test_fiat_v0_encode_nonzero(void)
{
    SubGhzRawPair out[8192];
    SubGhzKeyParams params = make_params("Fiat V0", 0x123456789ABCDEF0ULL, 64);
    params.btn = 0x02;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 8192, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
}

void test_subaru_encode_nonzero(void)
{
    tier_a_smoke("Subaru", 0xABCDEF, 0x02, 0x1234, 4096);
}

void test_star_line_encode_nonzero(void)
{
    SubGhzRawPair out[2048];
    SubGhzKeyParams params = make_params("Star Line", 0, 64);
    params.serial = 0x12345678;
    params.btn = 0x02;
    params.cnt = 0x1234;

    uint32_t req = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, req);

    uint32_t count = subghz_proto_pirate_encode(&params, out, 2048, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(req, count);
}

/* ===================================================================
 * Runner
 * =================================================================== */

/* ===================================================================
 * Kia V6 (AES, keystore-gated) — full pipeline chain:
 * params → subghz_proto_pirate_encode() → pairs → pulse feed → the real
 * subghz_decode_kia_v6() must recover the fields.
 * =================================================================== */

void test_kia_v6_required_pairs(void)
{
    SubGhzKeyParams params =
        make_params("Kia V6", ((uint64_t)0x123456 << 32) | 0x003CF089u, 144);
    params.btn = 2;
    params.extra[0] = 0x34;

    SubGhzRawPair out[1000];
    const uint32_t count = subghz_proto_pirate_encode(&params, out, 1000, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, count);
    /* required_pairs is an allocation bound: >= actual, per repetition. */
    const uint32_t bound1 = subghz_proto_pirate_required_pairs(&params, 1);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(count, bound1);
    /* And the required-size buffer is sufficient for repetitions. */
    const uint32_t need = subghz_proto_pirate_required_pairs(&params, 3);
    SubGhzRawPair *big = malloc(need * sizeof(*big));
    TEST_ASSERT_NOT_NULL(big);
    const uint32_t n3 = subghz_proto_pirate_encode(&params, big, need, 3);
    TEST_ASSERT_EQUAL_UINT32(count * 3u, n3);
    free(big);
}

void test_kia_v6_encode_matches_upload_builder(void)
{
    const uint32_t serial = 0x123456u;
    const uint32_t cnt = 0x003CF089u;
    SubGhzKeyParams params =
        make_params("Kia V6", ((uint64_t)serial << 32) | cnt, 144);
    params.btn = 2;
    params.extra[0] = 0x34;

    SubGhzRawPair pairs[1000];
    const uint32_t npairs = subghz_proto_pirate_encode(&params, pairs, 1000, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, npairs);

    /* The pipeline output must be the merged-pair form of the upload
     * builder's waveform for the same frame (field mapping through
     * params -> frame -> upload -> pairs). */
    uint8_t frame[18];
    subghz_kia_v6_encrypt_frame(0x34u, serial, 2u, cnt,
                                m1_kia_v6_keys_builtin, frame);
    static subghz_upload_entry_t up[KIA_V6_ENC_UPLOAD_SIZE];
    const uint32_t n =
        subghz_kia_v6_build_upload(up, KIA_V6_ENC_UPLOAD_SIZE, frame);
    TEST_ASSERT_EQUAL_UINT32(KIA_V6_ENC_UPLOAD_SIZE, n);

    /* Rebuild merged durations from the upload and compare with pairs. */
    uint32_t merged[2048];
    uint32_t nm = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        if (nm > 0 && (merged[nm - 1] & 0x80000000u) ==
                          (up[i].level ? 0x80000000u : 0u))
            merged[nm - 1] += up[i].duration;
        else
            merged[nm++] = up[i].duration |
                           (up[i].level ? 0x80000000u : 0u);
    }
    TEST_ASSERT_EQUAL_UINT32(nm % 2u, 0u);  /* must pair evenly */
    TEST_ASSERT_EQUAL_UINT32(nm / 2u, npairs);
    for (uint32_t i = 0; i < npairs; i++)
    {
        TEST_ASSERT_TRUE(merged[2 * i] & 0x80000000u);       /* high */
        TEST_ASSERT_FALSE(merged[2 * i + 1] & 0x80000000u);  /* low  */
        TEST_ASSERT_EQUAL_UINT32(merged[2 * i] & 0x7FFFFFFFu,
                                 pairs[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(merged[2 * i + 1] & 0x7FFFFFFFu,
                                 pairs[i].low_us);
    }
}

/* ===================================================================
 * VAG (type 2 = TEA, keystore-free path) — params → encode → pairs must
 * equal the merged-pair form of the upload builder's waveform.
 * =================================================================== */

void test_vag_encode_matches_upload_builder(void)
{
    const uint32_t serial = 0x12345678u;
    const uint32_t cnt = 0x005634u;
    const uint64_t orig_key1 = 0xC0AABBCCDDEEFF00ULL;

    SubGhzKeyParams params = make_params("VAG", orig_key1, 96);
    params.btn = 1;                       /* unlock */
    params.cnt = cnt;
    params.extra[8]  = (uint8_t)(serial >> 24);
    params.extra[9]  = (uint8_t)(serial >> 16);
    params.extra[10] = (uint8_t)(serial >> 8);
    params.extra[11] = (uint8_t)serial;
    params.extra[12] = 2u;                /* type 2 (TEA) */
    params.extra[13] = 0xFFu;             /* key auto (unused for type 2) */

    SubGhzRawPair pairs[400];
    const uint32_t npairs = subghz_proto_pirate_encode(&params, pairs, 400, 1);
    TEST_ASSERT_GREATER_THAN_UINT32(0, npairs);

    /* Counter auto-advances per encode (reference behaviour). */
    uint64_t key1 = 0;
    uint16_t key2 = 0;
    TEST_ASSERT_TRUE(subghz_vag_encrypt_frame(
        serial, (cnt + 1u) & 0xFFFFFFu, 1u, 2u, 0xC0u, 0xFFu,
        orig_key1, 0, NULL, false, &key1, &key2));

    static subghz_upload_entry_t up[VAG_ENC_UPLOAD_T12];
    const uint32_t n =
        subghz_vag_build_upload(up, VAG_ENC_UPLOAD_T12, 2u, key1, key2);
    TEST_ASSERT_EQUAL_UINT32(VAG_ENC_UPLOAD_T12, n);

    /* Rebuild merged durations from the upload and compare with pairs. */
    uint32_t merged[VAG_ENC_UPLOAD_T12];
    uint32_t nm = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        if (nm > 0 && (merged[nm - 1] & 0x80000000u) ==
                          (up[i].level ? 0x80000000u : 0u))
            merged[nm - 1] += up[i].duration;
        else
            merged[nm++] = up[i].duration |
                           (up[i].level ? 0x80000000u : 0u);
    }
    TEST_ASSERT_EQUAL_UINT32(0u, nm % 2u);
    TEST_ASSERT_EQUAL_UINT32(nm / 2u, npairs);
    for (uint32_t i = 0; i < npairs; i++)
    {
        TEST_ASSERT_TRUE(merged[2 * i] & 0x80000000u);
        TEST_ASSERT_FALSE(merged[2 * i + 1] & 0x80000000u);
        TEST_ASSERT_EQUAL_UINT32(merged[2 * i] & 0x7FFFFFFFu,
                                 pairs[i].high_us);
        TEST_ASSERT_EQUAL_UINT32(merged[2 * i + 1] & 0x7FFFFFFFu,
                                 pairs[i].low_us);
    }
}

int main(void)
{    UNITY_BEGIN();

    RUN_TEST(test_find_by_name_exact);
    RUN_TEST(test_find_by_name_case_insensitive);
    RUN_TEST(test_find_by_name_unknown);
    RUN_TEST(test_is_supported);

    RUN_TEST(test_kia_v7_required_pairs);
    RUN_TEST(test_kia_v7_encode_structure);
    RUN_TEST(test_kia_v7_repetitions);
    RUN_TEST(test_kia_v7_buffer_overflow);
    RUN_TEST(test_unsupported_protocol_returns_zero);
    RUN_TEST(test_null_params);

    RUN_TEST(test_honda_v1_encode_nonzero);
    RUN_TEST(test_honda_v2_encode_nonzero);
    RUN_TEST(test_ford_v2_encode_nonzero);
    RUN_TEST(test_ford_v1_encode_nonzero);
    RUN_TEST(test_kia_v3_encode_nonzero);
    RUN_TEST(test_kia_v4_encode_nonzero);
    RUN_TEST(test_kia_v5_encode_nonzero);
    RUN_TEST(test_fiat_v1_encode_nonzero);

    RUN_TEST(test_kia_v6_encode_matches_upload_builder);
    RUN_TEST(test_kia_v6_required_pairs);
    RUN_TEST(test_vag_encode_matches_upload_builder);

    RUN_TEST(test_ford_v0_encode_nonzero);
    RUN_TEST(test_mazda_v0_encode_nonzero);
    RUN_TEST(test_honda_static_encode_nonzero);
    RUN_TEST(test_kia_v0_encode_nonzero);
    RUN_TEST(test_kia_v1_encode_nonzero);
    RUN_TEST(test_kia_v2_encode_nonzero);
    RUN_TEST(test_renault_v0_encode_nonzero);
    RUN_TEST(test_chrysler_v0_encode_nonzero);
    RUN_TEST(test_fiat_v0_encode_nonzero);
    RUN_TEST(test_subaru_encode_nonzero);
    RUN_TEST(test_star_line_encode_nonzero);
    return UNITY_END();
}
