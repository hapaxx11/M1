/* See COPYING.txt for license details. */

/*
 * test_rf_protocol_db.c
 *
 * Unit tests for rf_protocol_db — validate the signature table's integrity
 * (well-formed entries, sane ranges, terminated strings) and the label
 * accessors.  Pure data; no hardware.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure -R rf_protocol_db
 */

#include <string.h>
#include "unity.h"
#include "rf_protocol_db.h"
#include "subghz_protocol_registry.h"
#include "m1_sub_ghz_decenc.h"   /* protocol-index enum (FORD_V0, ...) */

void setUp(void) {}
void tearDown(void) {}

void test_db_not_empty(void)
{
    TEST_ASSERT_TRUE(rf_protocol_db_count() > 0);
}

void test_out_of_range_is_null(void)
{
    TEST_ASSERT_NULL(rf_protocol_db_get(rf_protocol_db_count()));
    TEST_ASSERT_NULL(rf_protocol_db_get(0xFFFFu));
}

void test_all_entries_well_formed(void)
{
    uint16_t n = rf_protocol_db_count();
    for (uint16_t i = 0; i < n; i++)
    {
        const rf_protocol_sig_t *s = rf_protocol_db_get(i);
        TEST_ASSERT_NOT_NULL(s);
        /* Identity + notes must be present and non-empty. */
        TEST_ASSERT_NOT_NULL(s->name);
        TEST_ASSERT_TRUE(strlen(s->name) > 0);
        TEST_ASSERT_NOT_NULL(s->device_note);
        TEST_ASSERT_NOT_NULL(s->security_note);
        /* At least one band must be set. */
        TEST_ASSERT_TRUE(s->bands != 0);
        /* Category must be in range. */
        TEST_ASSERT_TRUE(s->category < RF_CAT_COUNT);
        /* If both te bounds are set, min <= max. */
        if (s->te_min_us != 0 && s->te_max_us != 0)
            TEST_ASSERT_TRUE(s->te_min_us <= s->te_max_us);
        /* If both bit bounds are set, min <= max. */
        if (s->bits_min != 0 && s->bits_max != 0)
            TEST_ASSERT_TRUE(s->bits_min <= s->bits_max);
    }
}

void test_category_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("Automotive", rf_category_str(RF_CAT_AUTOMOTIVE));
    TEST_ASSERT_EQUAL_STRING("Home",       rf_category_str(RF_CAT_HOME));
    TEST_ASSERT_EQUAL_STRING("Security",   rf_category_str(RF_CAT_SECURITY));
    TEST_ASSERT_EQUAL_STRING("Weather",    rf_category_str(RF_CAT_WEATHER));
    TEST_ASSERT_EQUAL_STRING("IoT",        rf_category_str(RF_CAT_IOT));
    TEST_ASSERT_EQUAL_STRING("Unknown",    rf_category_str(RF_CAT_UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("Unknown",    rf_category_str((rf_category_t)999));
}

void test_security_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("Fixed",     rf_security_str(RF_SEC_FIXED));
    TEST_ASSERT_EQUAL_STRING("Rolling",   rf_security_str(RF_SEC_ROLLING));
    TEST_ASSERT_EQUAL_STRING("Encrypted", rf_security_str(RF_SEC_ENCRYPTED));
    TEST_ASSERT_EQUAL_STRING("Unknown",   rf_security_str(RF_SEC_UNKNOWN));
}

void test_contains_expected_families(void)
{
    /* Spot-check that key families from the plan are present. */
    bool has_tpms = false, has_ble = false, has_zwave = false;
    uint16_t n = rf_protocol_db_count();
    for (uint16_t i = 0; i < n; i++)
    {
        const rf_protocol_sig_t *s = rf_protocol_db_get(i);
        if (strstr(s->name, "TPMS"))    has_tpms  = true;
        if (strstr(s->name, "BLE"))     has_ble   = true;
        if (strstr(s->name, "Z-Wave"))  has_zwave = true;
    }
    TEST_ASSERT_TRUE(has_tpms);
    TEST_ASSERT_TRUE(has_ble);
    TEST_ASSERT_TRUE(has_zwave);
}

/*============================================================================*/
/* Automotive signature rows linked to the decoder registry                    */
/*============================================================================*/

/* Every registry-listed ProtoPirate automotive protocol and the DB row name
 * expected to cover it.  Kia V4 shares the "Kia V3/V4" row (linked to
 * KIA_V3), so its expected name is the shared row's. */
typedef struct {
    int16_t     registry_idx;   /* enum value from m1_sub_ghz_decenc.h */
    const char *row_name;       /* DB row name that must cover it */
} auto_expect_t;

static const auto_expect_t auto_expected[] = {
    { FORD_V0,      "Ford V0" },
    { MAZDA_V0,     "Mazda V0" },
    { HONDA_STATIC, "Honda Static" },
    { KIA_V0,       "Kia V0" },
    { KIA_V1,       "Kia V1" },
    { KIA_V2,       "Kia V2" },
    { KIA_V7,       "Kia V7" },
    { RENAULT_V0,   "Renault V0" },
    { CHRYSLER_V0,  "Chrysler V0" },
    { FIAT_V0,      "Fiat V0" },
    { SUBARU_PP,    "Subaru" },
    { HONDA_V1,     "Honda V1" },
    { HONDA_V2,     "Honda V2" },
    { FORD_V1,      "Ford V1" },
    { FORD_V2,      "Ford V2" },
    { KIA_V3,       "Kia V3/V4" },
    { KIA_V4,       "Kia V3/V4" },   /* grouped with V3 into one row */
    { KIA_V5,       "Kia V5" },
    { FIAT_V1,      "Fiat V1" },
    { FIAT_V2,      "Fiat V2" },
    { RENAULT_V1,   "Renault V1" },
    { PSA,          "PSA" },
    { FORD_V3,      "Ford V3" },
    { VAG,          "VAG" },
    { KIA_V6,       "Kia V6" },
};
#define AUTO_EXPECTED_COUNT (sizeof(auto_expected) / sizeof(auto_expected[0]))

static const rf_protocol_sig_t *find_row_by_name(const char *name)
{
    uint16_t n = rf_protocol_db_count();
    for (uint16_t i = 0; i < n; i++)
    {
        const rf_protocol_sig_t *s = rf_protocol_db_get(i);
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

/* Every registry-listed automotive protocol has a covering DB row, and the
 * row links back into the registry. */
void test_automotive_coverage_and_linkage(void)
{
    for (size_t k = 0; k < AUTO_EXPECTED_COUNT; k++)
    {
        const rf_protocol_sig_t *s = find_row_by_name(auto_expected[k].row_name);
        TEST_ASSERT_NOT_NULL_MESSAGE(s, auto_expected[k].row_name);
        TEST_ASSERT_EQUAL(RF_CAT_AUTOMOTIVE, s->category);
        TEST_ASSERT_EQUAL(RF_SEC_ROLLING, s->security);
        TEST_ASSERT_TRUE(s->registry_index >= 0);
        TEST_ASSERT_TRUE(s->registry_index < (int16_t)subghz_protocol_registry_count);
        /* The row's name must equal the name of the registry entry it links
         * to (for the grouped Kia V3/V4 row that is the KIA_V3 entry). */
        const SubGhzProtocolDef *def =
            subghz_protocol_get((uint16_t)s->registry_index);
        TEST_ASSERT_NOT_NULL(def);
        TEST_ASSERT_EQUAL_STRING(s->name, def->name);
        /* And for non-grouped protocols the link is the protocol itself. */
        if (auto_expected[k].registry_idx != KIA_V4)
            TEST_ASSERT_EQUAL_INT(auto_expected[k].registry_idx, s->registry_index);
        else
            TEST_ASSERT_EQUAL_INT(KIA_V3, s->registry_index);
    }
}

/* Rows without a decoder counterpart must carry -1, never the
 * zero-initialised PRINCETON alias. */
void test_unlinked_rows_are_minus_one(void)
{
    const char *unlinked[] = {
        "Car Key Fob (rolling)", "Car Key Fob (fixed)", "TPMS Tyre Sensor",
        "Garage/Gate (fixed)", "Garage/Gate (rolling)", "Weather Station",
        "BLE Advertising", "WiFi (2.4 GHz)", "Zigbee/Thread (802.15.4)",
    };
    for (size_t k = 0; k < sizeof(unlinked) / sizeof(unlinked[0]); k++)
    {
        const rf_protocol_sig_t *s = find_row_by_name(unlinked[k]);
        TEST_ASSERT_NOT_NULL_MESSAGE(s, unlinked[k]);
        TEST_ASSERT_EQUAL_INT(-1, s->registry_index);
    }
}

/* The linked registry entry's timing window and bit floor must fit inside
 * the signature row's window/range. */
void test_automotive_window_consistency(void)
{
    for (size_t k = 0; k < AUTO_EXPECTED_COUNT; k++)
    {
        const rf_protocol_sig_t *s = find_row_by_name(auto_expected[k].row_name);
        TEST_ASSERT_NOT_NULL(s);
        const SubGhzProtocolDef *def =
            subghz_protocol_get((uint16_t)s->registry_index);
        TEST_ASSERT_NOT_NULL(def);
        TEST_ASSERT_TRUE_MESSAGE(def->timing.te_short >= s->te_min_us, s->name);
        TEST_ASSERT_TRUE_MESSAGE(def->timing.te_short <= s->te_max_us, s->name);
        TEST_ASSERT_TRUE_MESSAGE(def->timing.te_long  >= s->te_min_us, s->name);
        TEST_ASSERT_TRUE_MESSAGE(def->timing.te_long  <= s->te_max_us, s->name);
        TEST_ASSERT_TRUE_MESSAGE(
            def->timing.min_count_bit_for_found >= s->bits_min, s->name);
        if (s->bits_max != 0)
            TEST_ASSERT_TRUE_MESSAGE(
                def->timing.min_count_bit_for_found <= s->bits_max, s->name);
    }

    /* Fiat V1's second timing variant (100/200 us) must also fit the row —
     * the registry carries only variant A (250/500). */
    const rf_protocol_sig_t *fiat_v1 = find_row_by_name("Fiat V1");
    TEST_ASSERT_NOT_NULL(fiat_v1);
    TEST_ASSERT_TRUE(100 >= fiat_v1->te_min_us && 100 <= fiat_v1->te_max_us);
    TEST_ASSERT_TRUE(200 >= fiat_v1->te_min_us && 200 <= fiat_v1->te_max_us);
}

/* Registry band flags must be a subset of the row's bands, and the row's
 * modulation family must agree with the registry AM/FM flags. */
void test_automotive_band_mod_consistency(void)
{
    for (size_t k = 0; k < AUTO_EXPECTED_COUNT; k++)
    {
        const rf_protocol_sig_t *s = find_row_by_name(auto_expected[k].row_name);
        TEST_ASSERT_NOT_NULL(s);
        const SubGhzProtocolDef *def =
            subghz_protocol_get((uint16_t)s->registry_index);
        TEST_ASSERT_NOT_NULL(def);

        if (def->flags & SubGhzProtocolFlag_315)
            TEST_ASSERT_TRUE_MESSAGE(s->bands & RF_BAND_315, s->name);
        if (def->flags & SubGhzProtocolFlag_433)
            TEST_ASSERT_TRUE_MESSAGE(s->bands & RF_BAND_433, s->name);
        if (def->flags & SubGhzProtocolFlag_868)
            TEST_ASSERT_TRUE_MESSAGE(s->bands & RF_BAND_868, s->name);

        bool am = (def->flags & SubGhzProtocolFlag_AM) != 0;
        bool fm = (def->flags & SubGhzProtocolFlag_FM) != 0;
        if (am && !fm)
            TEST_ASSERT_EQUAL_MESSAGE(RF_MOD_OOK, s->mod, s->name);
        else if (fm && !am)
            TEST_ASSERT_EQUAL_MESSAGE(RF_MOD_FSK, s->mod, s->name);
        else if (am && fm)
            TEST_ASSERT_EQUAL_MESSAGE(RF_MOD_UNKNOWN, s->mod, s->name);
    }
}

/* The specific automotive rows must precede the generic catch-all rows so
 * rf_match_best()'s first-wins tie-breaking names the specific protocol. */
void test_specific_rows_precede_generic_automotive(void)
{
    uint16_t n = rf_protocol_db_count();
    int first_generic = -1, last_specific = -1;
    for (uint16_t i = 0; i < n; i++)
    {
        const rf_protocol_sig_t *s = rf_protocol_db_get(i);
        if (s->category != RF_CAT_AUTOMOTIVE)
            continue;
        if (s->registry_index >= 0)
            last_specific = (int)i;
        else if (first_generic < 0)
            first_generic = (int)i;
    }
    TEST_ASSERT_TRUE(last_specific >= 0);
    TEST_ASSERT_TRUE(first_generic > last_specific);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_db_not_empty);
    RUN_TEST(test_out_of_range_is_null);
    RUN_TEST(test_all_entries_well_formed);
    RUN_TEST(test_category_strings);
    RUN_TEST(test_security_strings);
    RUN_TEST(test_contains_expected_families);
    RUN_TEST(test_automotive_coverage_and_linkage);
    RUN_TEST(test_unlinked_rows_are_minus_one);
    RUN_TEST(test_automotive_window_consistency);
    RUN_TEST(test_automotive_band_mod_consistency);
    RUN_TEST(test_specific_rows_precede_generic_automotive);
    return UNITY_END();
}