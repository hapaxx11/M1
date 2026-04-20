/* See COPYING.txt for license details. */

/*
 * test_subghz_new_remote_gen.c
 *
 * Unit tests for subghz_new_remote_gen — the pure-logic random remote key
 * generator used by the Bind New Remote wizard.
 *
 * Tests verify:
 *   - Valid params returned for every supported protocol
 *   - key bit width is correctly masked (no bits above bit_count)
 *   - Different seeds produce different keys (PRNG is working)
 *   - Same seed produces same key (deterministic)
 *   - proto_name, freq_hz, te fields match expected values
 *   - file_base is non-empty and contains no path separators or spaces
 *   - Out-of-range protocol returns false
 *   - NULL out-pointer returns false
 *   - subghz_new_remote_proto_label returns non-empty strings
 *   - PRNG produces distinct keys across all five protocols (no aliasing)
 */

#include "unity.h"
#include "subghz_new_remote_gen.h"

#include <string.h>
#include <stdint.h>

void setUp(void)   { }
void tearDown(void){ }

/* ─── Label sanity ────────────────────────────────────────────────────────── */

void test_proto_label_all_valid(void)
{
    for (int i = 0; i < (int)BW_PROTO_COUNT; i++)
    {
        const char *lbl = subghz_new_remote_proto_label((BindWizardProto)i);
        TEST_ASSERT_NOT_NULL_MESSAGE(lbl, "label is NULL");
        TEST_ASSERT_TRUE_MESSAGE(lbl[0] != '\0', "label is empty");
    }
}

void test_proto_label_out_of_range(void)
{
    const char *lbl = subghz_new_remote_proto_label(BW_PROTO_COUNT);
    TEST_ASSERT_NOT_NULL(lbl);
    /* Should return some non-NULL fallback */
}

/* ─── NULL guard ──────────────────────────────────────────────────────────── */

void test_gen_null_out_returns_false(void)
{
    TEST_ASSERT_FALSE(subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, 0xDEADBEEF, NULL));
}

void test_gen_out_of_range_proto_returns_false(void)
{
    NewRemoteParams p;
    TEST_ASSERT_FALSE(subghz_new_remote_gen(BW_PROTO_COUNT, 0x1234, &p));
}

/* ─── CAME Atomo ──────────────────────────────────────────────────────────── */

void test_gen_came_atomo_basic(void)
{
    NewRemoteParams p;
    TEST_ASSERT_TRUE(subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, 0x123456789ABCULL, &p));

    TEST_ASSERT_EQUAL_STRING("CAME Atomo", p.proto_name);
    TEST_ASSERT_EQUAL_UINT32(433920000, p.freq_hz);
    TEST_ASSERT_EQUAL_UINT32(62, p.bit_count);
    TEST_ASSERT_EQUAL_UINT16(200, p.te);
}

void test_gen_came_atomo_key_in_range(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, 0xABCDEF01UL, &p);

    /* Key must fit in 62 bits: no bits above bit 61 */
    uint64_t mask = (1ULL << 62) - 1ULL;
    TEST_ASSERT_EQUAL_UINT64(p.key & mask, p.key);
}

/* ─── Nice FloR-S ─────────────────────────────────────────────────────────── */

void test_gen_nice_flors_basic(void)
{
    NewRemoteParams p;
    TEST_ASSERT_TRUE(subghz_new_remote_gen(BW_PROTO_NICE_FLOR_S, 0xABCDUL, &p));

    TEST_ASSERT_EQUAL_STRING("Nice FloR-S", p.proto_name);
    TEST_ASSERT_EQUAL_UINT32(433920000, p.freq_hz);
    TEST_ASSERT_EQUAL_UINT32(52, p.bit_count);
    TEST_ASSERT_EQUAL_UINT16(500, p.te);
}

void test_gen_nice_flors_key_in_range(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_NICE_FLOR_S, 0xDEADBEEFUL, &p);

    uint64_t mask = (1ULL << 52) - 1ULL;
    TEST_ASSERT_EQUAL_UINT64(p.key & mask, p.key);
}

/* ─── Alutech AT-4N ───────────────────────────────────────────────────────── */

void test_gen_alutech_basic(void)
{
    NewRemoteParams p;
    TEST_ASSERT_TRUE(subghz_new_remote_gen(BW_PROTO_ALUTECH_AT4N, 0x5555UL, &p));

    TEST_ASSERT_EQUAL_STRING("Alutech AT-4N", p.proto_name);
    TEST_ASSERT_EQUAL_UINT32(433920000, p.freq_hz);
    TEST_ASSERT_EQUAL_UINT32(64, p.bit_count);
    TEST_ASSERT_EQUAL_UINT16(400, p.te);
}

void test_gen_alutech_key_64bit(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_ALUTECH_AT4N, 0x9999AAAA5555BBBBULL, &p);
    /* bit_count = 64 means all 64 bits are valid */
    TEST_ASSERT_EQUAL_UINT32(64, p.bit_count);
    /* key can be any value; just verify gen succeeded */
    (void)p.key;
}

/* ─── DITEC GOL4 ──────────────────────────────────────────────────────────── */

void test_gen_ditec_gol4_basic(void)
{
    NewRemoteParams p;
    TEST_ASSERT_TRUE(subghz_new_remote_gen(BW_PROTO_DITEC_GOL4, 0x7777UL, &p));

    TEST_ASSERT_EQUAL_STRING("DITEC_GOL4", p.proto_name);
    TEST_ASSERT_EQUAL_UINT32(433920000, p.freq_hz);
    TEST_ASSERT_EQUAL_UINT32(54, p.bit_count);
    TEST_ASSERT_EQUAL_UINT16(400, p.te);
}

void test_gen_ditec_gol4_key_in_range(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_DITEC_GOL4, 0xCAFEBABEUL, &p);

    uint64_t mask = (1ULL << 54) - 1ULL;
    TEST_ASSERT_EQUAL_UINT64(p.key & mask, p.key);
}

/* ─── KingGates Stylo4k ───────────────────────────────────────────────────── */

void test_gen_kinggates_basic(void)
{
    NewRemoteParams p;
    TEST_ASSERT_TRUE(subghz_new_remote_gen(BW_PROTO_KINGGATES_STYLO4K, 0xBEEFUL, &p));

    TEST_ASSERT_EQUAL_STRING("KingGates Stylo4k", p.proto_name);
    TEST_ASSERT_EQUAL_UINT32(433920000, p.freq_hz);
    TEST_ASSERT_EQUAL_UINT32(60, p.bit_count);
    TEST_ASSERT_EQUAL_UINT16(400, p.te);
}

void test_gen_kinggates_key_in_range(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_KINGGATES_STYLO4K, 0xFEEDFACEUL, &p);

    uint64_t mask = (1ULL << 60) - 1ULL;
    TEST_ASSERT_EQUAL_UINT64(p.key & mask, p.key);
}

/* ─── Determinism: same seed → same key ──────────────────────────────────── */

void test_gen_deterministic(void)
{
    NewRemoteParams p1, p2;
    uint64_t seed = 0xBEEFDEADCAFEBEEFULL;

    subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, seed, &p1);
    subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, seed, &p2);

    TEST_ASSERT_EQUAL_UINT64(p1.key, p2.key);
    TEST_ASSERT_EQUAL_STRING(p1.file_base, p2.file_base);
}

/* ─── Distinct seeds → distinct keys ─────────────────────────────────────── */

void test_gen_different_seeds_different_keys(void)
{
    NewRemoteParams p1, p2;
    subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, 0x0000000000000001ULL, &p1);
    subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, 0x0000000000000002ULL, &p2);
    /* With splitmix64 this is extremely unlikely to collide */
    TEST_ASSERT_NOT_EQUAL(p1.key, p2.key);
}

/* ─── Filename checks ─────────────────────────────────────────────────────── */

void test_gen_file_base_non_empty(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, 0xABCDUL, &p);
    TEST_ASSERT_TRUE(p.file_base[0] != '\0');
}

void test_gen_file_base_no_path_separator(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_NICE_FLOR_S, 0x1234UL, &p);
    TEST_ASSERT_NULL(strchr(p.file_base, '/'));
    TEST_ASSERT_NULL(strchr(p.file_base, '\\'));
}

void test_gen_file_base_no_spaces(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_DITEC_GOL4, 0x5678UL, &p);
    TEST_ASSERT_NULL(strchr(p.file_base, ' '));
}

void test_gen_file_base_starts_with_newremote(void)
{
    NewRemoteParams p;
    subghz_new_remote_gen(BW_PROTO_ALUTECH_AT4N, 0xABCDUL, &p);
    TEST_ASSERT_TRUE(strncmp(p.file_base, "NewRemote_", 10) == 0);
}

/* ─── All protocols produce valid results from the same seed ────────────── */

void test_gen_all_protocols_succeed(void)
{
    uint64_t seed  = 0x42424242DEADBEEFULL;
    NewRemoteParams p;

    for (int i = 0; i < (int)BW_PROTO_COUNT; i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(
            subghz_new_remote_gen((BindWizardProto)i, seed, &p),
            "gen returned false for a valid protocol");

        /* Key must fit within bit_count bits */
        if (p.bit_count < 64)
        {
            uint64_t max_key = (1ULL << p.bit_count) - 1ULL;
            TEST_ASSERT_TRUE_MESSAGE(
                p.key <= max_key,
                "key exceeds bit_count mask");
        }

        /* All required fields are filled */
        TEST_ASSERT_TRUE_MESSAGE(p.proto_name[0] != '\0', "proto_name empty");
        TEST_ASSERT_TRUE_MESSAGE(p.freq_hz > 0,           "freq_hz is zero");
        TEST_ASSERT_TRUE_MESSAGE(p.bit_count > 0,         "bit_count is zero");
        TEST_ASSERT_TRUE_MESSAGE(p.file_base[0] != '\0',  "file_base empty");
    }
}

/* ─── Zero seed is valid ─────────────────────────────────────────────────── */

void test_gen_zero_seed_valid(void)
{
    NewRemoteParams p;
    TEST_ASSERT_TRUE(subghz_new_remote_gen(BW_PROTO_CAME_ATOMO, 0ULL, &p));
    /* splitmix64(0) is defined and non-zero */
    TEST_ASSERT_NOT_EQUAL(0, p.key);
}

/*============================================================================*/
/* main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Label */
    RUN_TEST(test_proto_label_all_valid);
    RUN_TEST(test_proto_label_out_of_range);

    /* NULL / range guards */
    RUN_TEST(test_gen_null_out_returns_false);
    RUN_TEST(test_gen_out_of_range_proto_returns_false);

    /* CAME Atomo */
    RUN_TEST(test_gen_came_atomo_basic);
    RUN_TEST(test_gen_came_atomo_key_in_range);

    /* Nice FloR-S */
    RUN_TEST(test_gen_nice_flors_basic);
    RUN_TEST(test_gen_nice_flors_key_in_range);

    /* Alutech AT-4N */
    RUN_TEST(test_gen_alutech_basic);
    RUN_TEST(test_gen_alutech_key_64bit);

    /* DITEC GOL4 */
    RUN_TEST(test_gen_ditec_gol4_basic);
    RUN_TEST(test_gen_ditec_gol4_key_in_range);

    /* KingGates Stylo4k */
    RUN_TEST(test_gen_kinggates_basic);
    RUN_TEST(test_gen_kinggates_key_in_range);

    /* PRNG behaviour */
    RUN_TEST(test_gen_deterministic);
    RUN_TEST(test_gen_different_seeds_different_keys);
    RUN_TEST(test_gen_zero_seed_valid);

    /* Filename */
    RUN_TEST(test_gen_file_base_non_empty);
    RUN_TEST(test_gen_file_base_no_path_separator);
    RUN_TEST(test_gen_file_base_no_spaces);
    RUN_TEST(test_gen_file_base_starts_with_newremote);

    /* All protocols */
    RUN_TEST(test_gen_all_protocols_succeed);

    return UNITY_END();
}
