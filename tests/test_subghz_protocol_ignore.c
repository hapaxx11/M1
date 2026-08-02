/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See COPYING.txt for license details. */

/*
 * test_subghz_protocol_ignore.c
 *
 * Unit tests for the Sub-GHz protocol ignore filter module
 * (Sub_Ghz/subghz_protocol_ignore.c) — the Momentum-style CATEGORY GROUP
 * model (Vehicles / Gates / Sensors / Pagers / Weather / TPMS).
 *
 * Covers:
 *   - Default state (nothing ignored) and reset.
 *   - Group membership resolution from the registry:
 *       * Weather / TPMS derived from the protocol `type`.
 *       * Vehicles / Gates / Sensors / Pagers from the name-keyed table.
 *   - group_set / group_get / group_toggle and is_ignored() gating.
 *   - Uncategorised protocols are never ignored (Momentum parity).
 *   - Per-group protocol counts and ignored-group counts.
 *   - Hex serialize/deserialize round-trip of the ignored-group bitmask.
 *   - Name-based query via the registry.
 *
 * The module reads the global protocol registry directly, so this test
 * provides a small stand-in registry with recognisable protocol names/types.
 *
 * Build:
 *   cmake -B tests/build-tests -S tests && cmake --build tests/build-tests
 *   ctest --test-dir tests/build-tests -R subghz_protocol_ignore --output-on-failure
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity.h"
#include "subghz_protocol_ignore.h"
#include "subghz_protocol_registry.h"

/*============================================================================*/
/* Stand-in registry — names/types chosen to exercise every group.            */
/*============================================================================*/

enum {
    P_PRINCETON = 0,   /* Gates    (name table)   */
    P_STARLINE,        /* Vehicles (name table)   */
    P_OREGON,          /* Weather  (type-derived) */
    P_SCHRADER,        /* TPMS     (type-derived) */
    P_KEELOQ,          /* (uncategorised)         */
    P_POCSAG,          /* Pagers   (name table)   */
    P_MAGELLAN,        /* Sensors  (name table)   */
    P_COUNT
};

const SubGhzProtocolDef subghz_protocol_registry[] = {
    [P_PRINCETON] = { .name = "Princeton",     .type = SubGhzProtocolTypeStatic  },
    [P_STARLINE]  = { .name = "Star Line",     .type = SubGhzProtocolTypeDynamic },
    [P_OREGON]    = { .name = "Oregon v2",     .type = SubGhzProtocolTypeWeather },
    [P_SCHRADER]  = { .name = "Schrader TPMS", .type = SubGhzProtocolTypeTPMS    },
    [P_KEELOQ]    = { .name = "KeeLoq",        .type = SubGhzProtocolTypeDynamic },
    [P_POCSAG]    = { .name = "POCSAG",        .type = SubGhzProtocolTypeStatic  },
    [P_MAGELLAN]  = { .name = "Magellan",      .type = SubGhzProtocolTypeStatic  },
};
const uint16_t subghz_protocol_registry_count = P_COUNT;

int16_t subghz_protocol_find_by_name(const char *name)
{
    if (!name) return -1;
    for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
        if (subghz_protocol_registry[i].name &&
            strcmp(subghz_protocol_registry[i].name, name) == 0)
            return (int16_t)i;
    return -1;
}

const char *subghz_protocol_get_name(uint16_t i)
{
    return (i < subghz_protocol_registry_count) ? subghz_protocol_registry[i].name : 0;
}

void setUp(void)   { subghz_ignore_reset(); }
void tearDown(void) {}

/*============================================================================*/
/* Default / reset                                                            */
/*============================================================================*/

void test_default_nothing_ignored(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_group_ignored_count());
    for (uint16_t i = 0; i < subghz_protocol_registry_count; i++)
        TEST_ASSERT_FALSE(subghz_ignore_is_ignored(i));
    for (int g = 0; g < SubGhzIgnoreGroupCount; g++)
        TEST_ASSERT_FALSE(subghz_ignore_group_get((SubGhzIgnoreGroup)g));
}

void test_reset_clears_all(void)
{
    subghz_ignore_group_set(SubGhzIgnoreGroupGates, true);
    subghz_ignore_group_set(SubGhzIgnoreGroupVehicles, true);
    TEST_ASSERT_EQUAL_UINT16(2, subghz_ignore_group_ignored_count());
    subghz_ignore_reset();
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_group_ignored_count());
}

/*============================================================================*/
/* Group membership resolution                                                */
/*============================================================================*/

void test_group_mask_name_table(void)
{
    TEST_ASSERT_EQUAL_UINT32(SUBGHZ_IGNORE_GROUP_BIT(SubGhzIgnoreGroupGates),
                             subghz_ignore_group_mask_of(P_PRINCETON));
    TEST_ASSERT_EQUAL_UINT32(SUBGHZ_IGNORE_GROUP_BIT(SubGhzIgnoreGroupVehicles),
                             subghz_ignore_group_mask_of(P_STARLINE));
    TEST_ASSERT_EQUAL_UINT32(SUBGHZ_IGNORE_GROUP_BIT(SubGhzIgnoreGroupPagers),
                             subghz_ignore_group_mask_of(P_POCSAG));
    TEST_ASSERT_EQUAL_UINT32(SUBGHZ_IGNORE_GROUP_BIT(SubGhzIgnoreGroupSensors),
                             subghz_ignore_group_mask_of(P_MAGELLAN));
}

void test_group_mask_type_derived(void)
{
    TEST_ASSERT_EQUAL_UINT32(SUBGHZ_IGNORE_GROUP_BIT(SubGhzIgnoreGroupWeather),
                             subghz_ignore_group_mask_of(P_OREGON));
    TEST_ASSERT_EQUAL_UINT32(SUBGHZ_IGNORE_GROUP_BIT(SubGhzIgnoreGroupTPMS),
                             subghz_ignore_group_mask_of(P_SCHRADER));
}

void test_group_mask_uncategorised_and_oor(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, subghz_ignore_group_mask_of(P_KEELOQ));
    TEST_ASSERT_EQUAL_UINT32(0, subghz_ignore_group_mask_of(P_COUNT));       /* OOR */
    TEST_ASSERT_EQUAL_UINT32(0, subghz_ignore_group_mask_of(1000));          /* OOR */
}

/*============================================================================*/
/* Ignore gating                                                              */
/*============================================================================*/

void test_ignore_group_gates(void)
{
    subghz_ignore_group_set(SubGhzIgnoreGroupGates, true);
    TEST_ASSERT_TRUE(subghz_ignore_group_get(SubGhzIgnoreGroupGates));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(P_PRINCETON));   /* Gates */
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(P_STARLINE));   /* Vehicles */
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(P_KEELOQ));     /* none */
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_group_ignored_count());
}

void test_ignore_group_weather_type_derived(void)
{
    subghz_ignore_group_set(SubGhzIgnoreGroupWeather, true);
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(P_OREGON));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(P_SCHRADER));   /* TPMS, not Weather */
}

void test_ignore_group_tpms_type_derived(void)
{
    subghz_ignore_group_set(SubGhzIgnoreGroupTPMS, true);
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(P_SCHRADER));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(P_OREGON));
}

void test_uncategorised_never_ignored(void)
{
    /* Ignore every group; KeeLoq (no group) must still decode. */
    for (int g = 0; g < SubGhzIgnoreGroupCount; g++)
        subghz_ignore_group_set((SubGhzIgnoreGroup)g, true);
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(P_KEELOQ));
    /* But the categorised ones are all ignored. */
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(P_PRINCETON));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(P_STARLINE));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(P_OREGON));
}

void test_group_toggle_and_clear(void)
{
    TEST_ASSERT_FALSE(subghz_ignore_group_get(SubGhzIgnoreGroupSensors));
    subghz_ignore_group_toggle(SubGhzIgnoreGroupSensors);
    TEST_ASSERT_TRUE(subghz_ignore_group_get(SubGhzIgnoreGroupSensors));
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored(P_MAGELLAN));
    subghz_ignore_group_toggle(SubGhzIgnoreGroupSensors);
    TEST_ASSERT_FALSE(subghz_ignore_group_get(SubGhzIgnoreGroupSensors));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored(P_MAGELLAN));

    subghz_ignore_group_set(SubGhzIgnoreGroupSensors, true);
    subghz_ignore_group_set(SubGhzIgnoreGroupSensors, false);
    TEST_ASSERT_FALSE(subghz_ignore_group_get(SubGhzIgnoreGroupSensors));
}

void test_group_out_of_range_is_noop(void)
{
    subghz_ignore_group_set(SubGhzIgnoreGroupCount, true);
    subghz_ignore_group_toggle((SubGhzIgnoreGroup)(SubGhzIgnoreGroupCount + 3));
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_group_ignored_count());
    TEST_ASSERT_FALSE(subghz_ignore_group_get(SubGhzIgnoreGroupCount));
}

/*============================================================================*/
/* Names / counts                                                             */
/*============================================================================*/

void test_group_names(void)
{
    TEST_ASSERT_EQUAL_STRING("Weather",  subghz_ignore_group_name(SubGhzIgnoreGroupWeather));
    TEST_ASSERT_EQUAL_STRING("Vehicles", subghz_ignore_group_name(SubGhzIgnoreGroupVehicles));
    TEST_ASSERT_EQUAL_STRING("Gates",    subghz_ignore_group_name(SubGhzIgnoreGroupGates));
    TEST_ASSERT_EQUAL_STRING("Pagers",   subghz_ignore_group_name(SubGhzIgnoreGroupPagers));
}

void test_group_protocol_counts(void)
{
    /* One protocol per group in the stand-in registry. */
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_group_protocol_count(SubGhzIgnoreGroupGates));
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_group_protocol_count(SubGhzIgnoreGroupVehicles));
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_group_protocol_count(SubGhzIgnoreGroupWeather));
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_group_protocol_count(SubGhzIgnoreGroupTPMS));
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_group_protocol_count(SubGhzIgnoreGroupSensors));
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_group_protocol_count(SubGhzIgnoreGroupPagers));
}

/*============================================================================*/
/* Hex serialization                                                          */
/*============================================================================*/

void test_serialize_empty(void)
{
    char buf[SUBGHZ_IGNORE_HEX_BUFSZ];
    size_t n = subghz_ignore_serialize_hex(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(SUBGHZ_IGNORE_HEX_LEN, n);
    for (size_t i = 0; i < n; i++)
        TEST_ASSERT_EQUAL_CHAR('0', buf[i]);
}

void test_serialize_bit(void)
{
    /* Vehicles == bit 2 → 0x4 in the least-significant nibble. */
    subghz_ignore_group_set(SubGhzIgnoreGroupVehicles, true);
    char buf[SUBGHZ_IGNORE_HEX_BUFSZ];
    subghz_ignore_serialize_hex(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('4', buf[SUBGHZ_IGNORE_HEX_LEN - 1]);
    TEST_ASSERT_EQUAL_CHAR('0', buf[0]);
}

void test_serialize_buffer_too_small(void)
{
    char buf[4];
    TEST_ASSERT_EQUAL_size_t(0, subghz_ignore_serialize_hex(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_size_t(0, subghz_ignore_serialize_hex(NULL, 100));
}

void test_serialize_deserialize_roundtrip(void)
{
    subghz_ignore_group_set(SubGhzIgnoreGroupWeather, true);
    subghz_ignore_group_set(SubGhzIgnoreGroupGates, true);
    subghz_ignore_group_set(SubGhzIgnoreGroupPagers, true);

    char buf[SUBGHZ_IGNORE_HEX_BUFSZ];
    subghz_ignore_serialize_hex(buf, sizeof(buf));

    subghz_ignore_reset();
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_group_ignored_count());
    subghz_ignore_deserialize_hex(buf);

    TEST_ASSERT_TRUE(subghz_ignore_group_get(SubGhzIgnoreGroupWeather));
    TEST_ASSERT_TRUE(subghz_ignore_group_get(SubGhzIgnoreGroupGates));
    TEST_ASSERT_TRUE(subghz_ignore_group_get(SubGhzIgnoreGroupPagers));
    TEST_ASSERT_FALSE(subghz_ignore_group_get(SubGhzIgnoreGroupVehicles));
    TEST_ASSERT_EQUAL_UINT16(3, subghz_ignore_group_ignored_count());
}

void test_deserialize_null_and_empty_clear(void)
{
    subghz_ignore_group_set(SubGhzIgnoreGroupGates, true);
    subghz_ignore_deserialize_hex(NULL);
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_group_ignored_count());

    subghz_ignore_group_set(SubGhzIgnoreGroupGates, true);
    subghz_ignore_deserialize_hex("");
    TEST_ASSERT_EQUAL_UINT16(0, subghz_ignore_group_ignored_count());
}

void test_deserialize_masks_invalid_bits(void)
{
    /* High bits beyond the valid group range must be dropped. */
    subghz_ignore_deserialize_hex("FFFFFFFF");
    TEST_ASSERT_EQUAL_UINT16(SubGhzIgnoreGroupCount,
                             subghz_ignore_group_ignored_count());
}

void test_deserialize_leading_whitespace(void)
{
    subghz_ignore_deserialize_hex("   1");   /* bit 0 == Weather */
    TEST_ASSERT_TRUE(subghz_ignore_group_get(SubGhzIgnoreGroupWeather));
    TEST_ASSERT_EQUAL_UINT16(1, subghz_ignore_group_ignored_count());
}

/*============================================================================*/
/* Name-based query                                                           */
/*============================================================================*/

void test_is_ignored_name(void)
{
    subghz_ignore_group_set(SubGhzIgnoreGroupVehicles, true);
    TEST_ASSERT_TRUE(subghz_ignore_is_ignored_name("Star Line"));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored_name("Princeton"));  /* Gates, not set */
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored_name("Nonexistent"));
    TEST_ASSERT_FALSE(subghz_ignore_is_ignored_name(NULL));
}

/*============================================================================*/
/* main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_default_nothing_ignored);
    RUN_TEST(test_reset_clears_all);

    RUN_TEST(test_group_mask_name_table);
    RUN_TEST(test_group_mask_type_derived);
    RUN_TEST(test_group_mask_uncategorised_and_oor);

    RUN_TEST(test_ignore_group_gates);
    RUN_TEST(test_ignore_group_weather_type_derived);
    RUN_TEST(test_ignore_group_tpms_type_derived);
    RUN_TEST(test_uncategorised_never_ignored);
    RUN_TEST(test_group_toggle_and_clear);
    RUN_TEST(test_group_out_of_range_is_noop);

    RUN_TEST(test_group_names);
    RUN_TEST(test_group_protocol_counts);

    RUN_TEST(test_serialize_empty);
    RUN_TEST(test_serialize_bit);
    RUN_TEST(test_serialize_buffer_too_small);
    RUN_TEST(test_serialize_deserialize_roundtrip);
    RUN_TEST(test_deserialize_null_and_empty_clear);
    RUN_TEST(test_deserialize_masks_invalid_bits);
    RUN_TEST(test_deserialize_leading_whitespace);

    RUN_TEST(test_is_ignored_name);

    return UNITY_END();
}
