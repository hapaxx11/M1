/* See COPYING.txt for license details. */

/*
 * test_subghz_create_proto.c
 *
 * Host-side unit tests for Sub_Ghz/subghz_create_proto.c — Phase 8a of
 * the Momentum parity migration.  Verifies the protocol catalog metadata,
 * the editable-field bitmask query, and the key-range masking helper.
 *
 * M1 Project — Hapax fork
 */

#include "unity.h"

#include <string.h>

#include "subghz_create_proto.h"

void setUp(void) { }
void tearDown(void) { }

/*============================================================================*/
/* Catalog shape                                                              */
/*============================================================================*/

static void test_count_matches_enum(void)
{
    TEST_ASSERT_EQUAL_UINT32((uint32_t)SUBGHZ_CREATE_PROTO_COUNT,
                             subghz_create_proto_count());
    /* Phase 8a: 5 rolling-code remotes + Phase 8b-1: 12 static-OOK families. */
    TEST_ASSERT_EQUAL_UINT32(17U, subghz_create_proto_count());
}

static void test_spec_non_null_for_every_valid_id(void)
{
    for (uint32_t i = 0; i < subghz_create_proto_count(); i++) {
        const SubGhzCreateProtoSpec *s =
            subghz_create_proto_spec((SubGhzCreateProtoId)i);
        TEST_ASSERT_NOT_NULL(s);
        TEST_ASSERT_NOT_NULL(s->label);
        TEST_ASSERT_NOT_NULL(s->proto_name);
        TEST_ASSERT_NOT_NULL(s->file_prefix);
        TEST_ASSERT_TRUE(s->label[0] != '\0');
        TEST_ASSERT_TRUE(s->proto_name[0] != '\0');
        TEST_ASSERT_TRUE(s->file_prefix[0] != '\0');
    }
}

static void test_spec_null_for_out_of_range_id(void)
{
    TEST_ASSERT_NULL(subghz_create_proto_spec(
        (SubGhzCreateProtoId)SUBGHZ_CREATE_PROTO_COUNT));
    TEST_ASSERT_NULL(subghz_create_proto_spec((SubGhzCreateProtoId)0xFFFFu));
}

static void test_bit_count_in_valid_range(void)
{
    for (uint32_t i = 0; i < subghz_create_proto_count(); i++) {
        const SubGhzCreateProtoSpec *s =
            subghz_create_proto_spec((SubGhzCreateProtoId)i);
        TEST_ASSERT_TRUE(s->bit_count > 0U);
        TEST_ASSERT_TRUE(s->bit_count <= 64U);
    }
}

static void test_433_band_for_rolling_code_protocols(void)
{
    /* The five Phase 8a rolling-code entries are all 433-band remotes. */
    static const SubGhzCreateProtoId rolling[] = {
        SUBGHZ_CREATE_PROTO_CAME_ATOMO,
        SUBGHZ_CREATE_PROTO_NICE_FLOR_S,
        SUBGHZ_CREATE_PROTO_ALUTECH_AT4N,
        SUBGHZ_CREATE_PROTO_DITEC_GOL4,
        SUBGHZ_CREATE_PROTO_KINGGATES_STYLO4K,
    };
    for (size_t i = 0; i < sizeof(rolling) / sizeof(rolling[0]); i++) {
        const SubGhzCreateProtoSpec *s = subghz_create_proto_spec(rolling[i]);
        TEST_ASSERT_NOT_NULL(s);
        TEST_ASSERT_TRUE(s->freq_hz >= 430000000U);
        TEST_ASSERT_TRUE(s->freq_hz <= 440000000U);
    }
}

static void test_all_freqs_in_supported_bands(void)
{
    /* Every entry must sit in one of the four ISM bands the SI4463 covers:
     * 300, 315, 433, or 868 MHz (±5 MHz). */
    for (uint32_t i = 0; i < subghz_create_proto_count(); i++) {
        const SubGhzCreateProtoSpec *s =
            subghz_create_proto_spec((SubGhzCreateProtoId)i);
        const uint32_t f = s->freq_hz;
        const bool ok = (f >= 295000000U && f <= 305000000U) ||
                        (f >= 310000000U && f <= 320000000U) ||
                        (f >= 430000000U && f <= 440000000U) ||
                        (f >= 860000000U && f <= 870000000U);
        TEST_ASSERT_TRUE_MESSAGE(ok, "freq outside supported ISM bands");
    }
}

static void test_labels_are_unique(void)
{
    /* No two catalog entries may have identical display labels — the
     * SetType picker relies on labels as visible identifiers. */
    for (uint32_t i = 0; i < subghz_create_proto_count(); i++) {
        for (uint32_t j = i + 1; j < subghz_create_proto_count(); j++) {
            const SubGhzCreateProtoSpec *a =
                subghz_create_proto_spec((SubGhzCreateProtoId)i);
            const SubGhzCreateProtoSpec *b =
                subghz_create_proto_spec((SubGhzCreateProtoId)j);
            TEST_ASSERT_FALSE_MESSAGE(strcmp(a->label, b->label) == 0,
                                      "duplicate label in catalog");
        }
    }
}

/*============================================================================*/
/* Per-protocol metadata regression                                           */
/*============================================================================*/

static void check_proto(SubGhzCreateProtoId id, const char *proto_name,
                       uint32_t freq_hz, uint32_t bits, uint16_t te,
                       const char *prefix)
{
    const SubGhzCreateProtoSpec *s = subghz_create_proto_spec(id);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING(proto_name, s->proto_name);
    TEST_ASSERT_EQUAL_UINT32(freq_hz, s->freq_hz);
    TEST_ASSERT_EQUAL_UINT32(bits, s->bit_count);
    TEST_ASSERT_EQUAL_UINT16(te, s->te);
    TEST_ASSERT_EQUAL_STRING(prefix, s->file_prefix);
    /* Every Phase 8a protocol exposes the opaque-key field. */
    TEST_ASSERT_TRUE(subghz_create_proto_has_field(id, SUBGHZ_CREATE_FIELD_KEY));
}

static void test_came_atomo_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_CAME_ATOMO,
                "CAME Atomo", 433920000U, 62U, 200U, "CameAtomo");
}

static void test_nice_flors_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_NICE_FLOR_S,
                "Nice FloR-S", 433920000U, 52U, 500U, "NiceFlors");
}

static void test_alutech_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_ALUTECH_AT4N,
                "Alutech AT-4N", 433920000U, 64U, 400U, "Alutech");
}

static void test_ditec_gol4_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_DITEC_GOL4,
                "DITEC_GOL4", 433920000U, 54U, 400U, "DitecGol4");
}

static void test_kinggates_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_KINGGATES_STYLO4K,
                "KingGates Stylo4k", 433920000U, 60U, 400U, "KingGates");
}

/*----------------------------------------------------------------------------*
 * Phase 8b-1 — Static-OOK families                                           *
 *----------------------------------------------------------------------------*/

static void test_princeton_433_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_PRINCETON_433,
                "Princeton", 433920000U, 24U, 350U, "Princeton");
}

static void test_princeton_315_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_PRINCETON_315,
                "Princeton", 315000000U, 24U, 350U, "Princeton");
}

static void test_nice_flo_12_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_NICE_FLO_12_433,
                "Nice FLO", 433920000U, 12U, 700U, "NiceFLO");
}

static void test_nice_flo_24_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_NICE_FLO_24_433,
                "Nice FLO", 433920000U, 24U, 700U, "NiceFLO");
}

static void test_came_12_433_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_CAME_12_433,
                "CAME", 433920000U, 12U, 320U, "CAME");
}

static void test_came_24_433_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_CAME_24_433,
                "CAME", 433920000U, 24U, 320U, "CAME");
}

static void test_came_12_868_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_CAME_12_868,
                "CAME", 868350000U, 12U, 320U, "CAME");
}

static void test_linear_300_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_LINEAR_300,
                "Linear", 300000000U, 10U, 500U, "Linear");
}

static void test_gatetx_433_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_GATETX_433,
                "GateTX", 433920000U, 24U, 350U, "GateTX");
}

static void test_doorhan_315_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_DOORHAN_315,
                "DoorHan", 315000000U, 24U, 350U, "DoorHan");
}

static void test_doorhan_433_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_DOORHAN_433,
                "DoorHan", 433920000U, 24U, 350U, "DoorHan");
}

static void test_holtek_ht12x_433_metadata(void)
{
    check_proto(SUBGHZ_CREATE_PROTO_HOLTEK_HT12X_433,
                "Holtek_HT12X", 433920000U, 12U, 340U, "Holtek");
}

/* Regression: Add Manually's strchr-based name stripping silently maps
 * "Nice FLO 12b" → "Nice" and "Gate TX 433" → "Gate".  The catalog must
 * carry the actual registry names so the .sub Protocol: field matches
 * the receiver decoder exactly. */
static void test_proto_names_match_registry(void)
{
    /* "Nice FLO" preserves the space (registry uses "Nice FLO"). */
    TEST_ASSERT_EQUAL_STRING("Nice FLO",
        subghz_create_proto_spec(SUBGHZ_CREATE_PROTO_NICE_FLO_12_433)->proto_name);
    TEST_ASSERT_EQUAL_STRING("Nice FLO",
        subghz_create_proto_spec(SUBGHZ_CREATE_PROTO_NICE_FLO_24_433)->proto_name);
    /* "GateTX" has no space (registry uses "GateTX"). */
    TEST_ASSERT_EQUAL_STRING("GateTX",
        subghz_create_proto_spec(SUBGHZ_CREATE_PROTO_GATETX_433)->proto_name);
    /* Holtek registry name is "Holtek_HT12X". */
    TEST_ASSERT_EQUAL_STRING("Holtek_HT12X",
        subghz_create_proto_spec(SUBGHZ_CREATE_PROTO_HOLTEK_HT12X_433)->proto_name);
}

/*============================================================================*/
/* Field-flag query                                                           */
/*============================================================================*/

static void test_has_field_true_for_advertised_field(void)
{
    /* Sanity: every catalog entry advertises FIELD_KEY. */
    for (uint32_t i = 0; i < subghz_create_proto_count(); i++) {
        TEST_ASSERT_TRUE(subghz_create_proto_has_field(
            (SubGhzCreateProtoId)i, SUBGHZ_CREATE_FIELD_KEY));
    }
}

static void test_has_field_false_for_unsupported_fields(void)
{
    /* None of the Phase 8a entries support the KeeLoq-family fields yet. */
    for (uint32_t i = 0; i < subghz_create_proto_count(); i++) {
        TEST_ASSERT_FALSE(subghz_create_proto_has_field(
            (SubGhzCreateProtoId)i, SUBGHZ_CREATE_FIELD_SERIAL));
        TEST_ASSERT_FALSE(subghz_create_proto_has_field(
            (SubGhzCreateProtoId)i, SUBGHZ_CREATE_FIELD_BUTTON));
        TEST_ASSERT_FALSE(subghz_create_proto_has_field(
            (SubGhzCreateProtoId)i, SUBGHZ_CREATE_FIELD_COUNTER));
        TEST_ASSERT_FALSE(subghz_create_proto_has_field(
            (SubGhzCreateProtoId)i, SUBGHZ_CREATE_FIELD_MFKEY));
    }
}

static void test_has_field_false_for_out_of_range_id(void)
{
    TEST_ASSERT_FALSE(subghz_create_proto_has_field(
        (SubGhzCreateProtoId)SUBGHZ_CREATE_PROTO_COUNT,
        SUBGHZ_CREATE_FIELD_KEY));
}

/*============================================================================*/
/* Key-range masking                                                          */
/*============================================================================*/

static void test_key_in_range_pass_through_when_fits(void)
{
    uint64_t out = 0;
    /* CAME Atomo: 62 bits — value with top 2 bits clear should fit. */
    const uint64_t k = 0x3FFFFFFFFFFFFFFFull;
    TEST_ASSERT_TRUE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_CAME_ATOMO, k, &out));
    TEST_ASSERT_EQUAL_UINT64(k, out);
}

static void test_key_in_range_truncates_high_bits(void)
{
    uint64_t out = 0;
    /* DITEC GOL4: 54 bits — bit 54 set should be truncated, return false. */
    const uint64_t k = (uint64_t)1 << 54;
    TEST_ASSERT_FALSE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_DITEC_GOL4, k, &out));
    TEST_ASSERT_EQUAL_UINT64(0U, out);
}

static void test_key_in_range_alutech_64_bit_full_range_ok(void)
{
    uint64_t out = 0;
    /* Alutech AT-4N: 64 bits — every uint64_t value must fit. */
    const uint64_t k = 0xFFFFFFFFFFFFFFFFull;
    TEST_ASSERT_TRUE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_ALUTECH_AT4N, k, &out));
    TEST_ASSERT_EQUAL_UINT64(k, out);
}

static void test_key_in_range_null_out_ptr_is_safe(void)
{
    /* Passing a NULL out pointer must not crash and must still report
     * the in/out-of-range status correctly. */
    TEST_ASSERT_TRUE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_NICE_FLOR_S, 0x12345ULL, (uint64_t *)0));
    TEST_ASSERT_FALSE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_NICE_FLOR_S, ((uint64_t)1) << 52, (uint64_t *)0));
}

static void test_key_in_range_returns_false_for_out_of_range_id(void)
{
    uint64_t out = 0xDEADBEEFull;
    TEST_ASSERT_FALSE(subghz_create_proto_key_in_range(
        (SubGhzCreateProtoId)SUBGHZ_CREATE_PROTO_COUNT, 0, &out));
    /* out is left untouched on invalid id. */
    TEST_ASSERT_EQUAL_UINT64(0xDEADBEEFull, out);
}

static void test_key_in_range_linear_10_bit(void)
{
    /* Linear 300 is the narrowest entry — 10 bits.  Value 0x3FF must fit;
     * 0x400 must truncate to 0. */
    uint64_t out = 0;
    TEST_ASSERT_TRUE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_LINEAR_300, 0x3FFull, &out));
    TEST_ASSERT_EQUAL_UINT64(0x3FFull, out);
    TEST_ASSERT_FALSE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_LINEAR_300, 0x400ull, &out));
    TEST_ASSERT_EQUAL_UINT64(0U, out);
}

static void test_key_in_range_came_12_truncates(void)
{
    /* CAME 12-bit: value 0xFFF fits, 0x1000 truncates to 0. */
    uint64_t out = 0xAAAAull;
    TEST_ASSERT_TRUE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_CAME_12_433, 0xFFFull, &out));
    TEST_ASSERT_EQUAL_UINT64(0xFFFull, out);
    TEST_ASSERT_FALSE(subghz_create_proto_key_in_range(
        SUBGHZ_CREATE_PROTO_CAME_12_433, 0x1000ull, &out));
    TEST_ASSERT_EQUAL_UINT64(0U, out);
}

/*============================================================================*/
/* Test runner                                                                */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* Catalog shape */
    RUN_TEST(test_count_matches_enum);
    RUN_TEST(test_spec_non_null_for_every_valid_id);
    RUN_TEST(test_spec_null_for_out_of_range_id);
    RUN_TEST(test_bit_count_in_valid_range);
    RUN_TEST(test_433_band_for_rolling_code_protocols);
    RUN_TEST(test_all_freqs_in_supported_bands);
    RUN_TEST(test_labels_are_unique);

    /* Per-protocol metadata — rolling-code (Phase 8a) */
    RUN_TEST(test_came_atomo_metadata);
    RUN_TEST(test_nice_flors_metadata);
    RUN_TEST(test_alutech_metadata);
    RUN_TEST(test_ditec_gol4_metadata);
    RUN_TEST(test_kinggates_metadata);

    /* Per-protocol metadata — static-OOK (Phase 8b-1) */
    RUN_TEST(test_princeton_433_metadata);
    RUN_TEST(test_princeton_315_metadata);
    RUN_TEST(test_nice_flo_12_metadata);
    RUN_TEST(test_nice_flo_24_metadata);
    RUN_TEST(test_came_12_433_metadata);
    RUN_TEST(test_came_24_433_metadata);
    RUN_TEST(test_came_12_868_metadata);
    RUN_TEST(test_linear_300_metadata);
    RUN_TEST(test_gatetx_433_metadata);
    RUN_TEST(test_doorhan_315_metadata);
    RUN_TEST(test_doorhan_433_metadata);
    RUN_TEST(test_holtek_ht12x_433_metadata);
    RUN_TEST(test_proto_names_match_registry);

    /* Field-flag query */
    RUN_TEST(test_has_field_true_for_advertised_field);
    RUN_TEST(test_has_field_false_for_unsupported_fields);
    RUN_TEST(test_has_field_false_for_out_of_range_id);

    /* Key-range masking */
    RUN_TEST(test_key_in_range_pass_through_when_fits);
    RUN_TEST(test_key_in_range_truncates_high_bits);
    RUN_TEST(test_key_in_range_alutech_64_bit_full_range_ok);
    RUN_TEST(test_key_in_range_null_out_ptr_is_safe);
    RUN_TEST(test_key_in_range_returns_false_for_out_of_range_id);
    RUN_TEST(test_key_in_range_linear_10_bit);
    RUN_TEST(test_key_in_range_came_12_truncates);

    return UNITY_END();
}
