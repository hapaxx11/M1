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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_db_not_empty);
    RUN_TEST(test_out_of_range_is_null);
    RUN_TEST(test_all_entries_well_formed);
    RUN_TEST(test_category_strings);
    RUN_TEST(test_security_strings);
    RUN_TEST(test_contains_expected_families);
    return UNITY_END();
}
