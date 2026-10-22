/* See COPYING.txt for license details. */

/*
 * test_models_db.c
 *
 * Host tests for the ProtoPirate car-models database (v2) parser in
 * Sub_Ghz/subghz_models_db.c.  All fixtures are embedded strings.
 */

#include "unity.h"
#include "subghz_models_db.h"

#include <string.h>

/* The complete upstream ProtoPirate models.txt (build/upstream/ProtoPirate/
 * assets/models_db/models.txt), copied verbatim. */
static const char upstream_db[] =
    "Filetype: ProtoPirate Car Models Database\n"
    "Version: 2\n"
    "\n"
    "ModelCount: 11\n"
    "\n"
    "ModelName1: 433.92 Mhz AM650\n"
    "Frequency1: 433920000\n"
    "PresetName1: FuriHalSubGhzPresetOok650Async\n"
    "\n"
    "ModelName2: 433.92 Mhz FM476\n"
    "Frequency2: 433920000\n"
    "PresetName2: FuriHalSubGhzPreset2FSKDev476Async\n"
    "\n"
    "ModelName3: 315 Mhz AM650\n"
    "Frequency3: 315000000\n"
    "PresetName3: FuriHalSubGhzPresetOok650Async\n"
    "\n"
    "ModelName4: 315 Mhz FM476\n"
    "Frequency4: 315000000\n"
    "PresetName4: FuriHalSubGhzPreset2FSKDev476Async\n"
    "\n"
    "ModelName5: Ford 433.92 Mhz AM\n"
    "Frequency5: 433920000\n"
    "PresetName5: FuriHalSubGhzPresetOok650Async\n"
    "\n"
    "ModelName6: Ford 433.92 Mhz FM\n"
    "Frequency6: 433920000\n"
    "PresetName6: FuriHalSubGhzPreset2FSKDev476Async\n"
    "\n"
    "ModelName7: Honda 433.92 Mhz FM\n"
    "Frequency7: 433920000\n"
    "PresetName7: FuriHalSubGhzPresetCustom\n"
    "CustomPresetData7: 02 0D 0B 06 08 32 07 04 14 00 13 02 12 07 11 36 10 E9 15 32 18 18 19 16 1D 92 1C 40 1B 03 20 FB 22 10 21 56 00 00 C0 00 00 00 00 00 00 00\n"
    "\n"
    "ModelName8: Honda 315 Mhz FM\n"
    "Frequency8: 315000000\n"
    "PresetName8: FuriHalSubGhzPresetCustom\n"
    "CustomPresetData8: 02 0D 0B 06 08 32 07 04 14 00 13 02 12 07 11 36 10 E9 15 32 18 18 19 16 1D 92 1C 40 1B 03 20 FB 22 10 21 56 00 00 C0 00 00 00 00 00 00 00\n"
    "\n"
    "ModelName9: KIA 433.92 Mhz AM\n"
    "Frequency9: 433920000\n"
    "PresetName9: FuriHalSubGhzPresetOok650Async\n"
    "\n"
    "ModelName10: KIA 433.92 Mhz FM\n"
    "Frequency10: 433920000\n"
    "PresetName10: FuriHalSubGhzPreset2FSKDev476Async\n"
    "\n"
    "ModelName11: VAG 434.42 Mhz AM\n"
    "Frequency11: 434420000\n"
    "PresetName11: FuriHalSubGhzPresetOok650Async\n";

static subghz_models_db_t db;

void setUp(void)
{
    memset(&db, 0xAA, sizeof(db));
}

void tearDown(void)
{
}

static bool parse(const char *text)
{
    return subghz_models_db_parse(text, strlen(text), &db);
}

/* ------------------------------------------------------------------ */

static void test_parse_upstream_full_db(void)
{
    TEST_ASSERT_TRUE(parse(upstream_db));
    TEST_ASSERT_EQUAL_UINT8(11, db.count);

    static const struct {
        const char *name;
        uint32_t freq;
        uint8_t mod;
        bool custom;
    } expected[11] = {
        {"433.92 Mhz AM650", 433920000, SUBGHZ_MODEL_MOD_AM650, false},
        {"433.92 Mhz FM476", 433920000, SUBGHZ_MODEL_MOD_FM476, false},
        {"315 Mhz AM650", 315000000, SUBGHZ_MODEL_MOD_AM650, false},
        {"315 Mhz FM476", 315000000, SUBGHZ_MODEL_MOD_FM476, false},
        {"Ford 433.92 Mhz AM", 433920000, SUBGHZ_MODEL_MOD_AM650, false},
        {"Ford 433.92 Mhz FM", 433920000, SUBGHZ_MODEL_MOD_FM476, false},
        {"Honda 433.92 Mhz FM", 433920000, SUBGHZ_MODEL_MOD_FM476, true},
        {"Honda 315 Mhz FM", 315000000, SUBGHZ_MODEL_MOD_FM476, true},
        {"KIA 433.92 Mhz AM", 433920000, SUBGHZ_MODEL_MOD_AM650, false},
        {"KIA 433.92 Mhz FM", 433920000, SUBGHZ_MODEL_MOD_FM476, false},
        {"VAG 434.42 Mhz AM", 434420000, SUBGHZ_MODEL_MOD_AM650, false},
    };

    for(uint8_t i = 0; i < 11; i++) {
        TEST_ASSERT_EQUAL_STRING(expected[i].name, db.entries[i].name);
        TEST_ASSERT_EQUAL_UINT32(expected[i].freq, db.entries[i].frequency_hz);
        TEST_ASSERT_EQUAL_UINT8(expected[i].mod, db.entries[i].mod);
        TEST_ASSERT_EQUAL(expected[i].custom, db.entries[i].custom_preset);
    }
}

static void test_parse_crlf_identical(void)
{
    /* Convert the upstream text to CRLF and expect identical results. */
    static char crlf[sizeof(upstream_db) * 2];
    size_t w = 0;
    for(size_t r = 0; upstream_db[r] != '\0'; r++) {
        if(upstream_db[r] == '\n') {
            crlf[w++] = '\r';
        }
        crlf[w++] = upstream_db[r];
    }
    crlf[w] = '\0';

    TEST_ASSERT_TRUE(subghz_models_db_parse(crlf, w, &db));
    TEST_ASSERT_EQUAL_UINT8(11, db.count);
    TEST_ASSERT_EQUAL_STRING("433.92 Mhz AM650", db.entries[0].name);
    TEST_ASSERT_EQUAL_STRING("VAG 434.42 Mhz AM", db.entries[10].name);
    TEST_ASSERT_EQUAL_UINT32(434420000, db.entries[10].frequency_hz);
    TEST_ASSERT_TRUE(db.entries[6].custom_preset);
}

static void test_parse_leading_blank_lines_and_trailing_ws(void)
{
    const char *text =
        "\n"
        "\r\n"
        "Filetype: ProtoPirate Car Models Database   \n"
        "Version: 2\t\n"
        "\n"
        "ModelName1: Test Model \n"
        "Frequency1: 433920000\t\n"
        "PresetName1: FuriHalSubGhzPresetOok270Async  \n";

    TEST_ASSERT_TRUE(parse(text));
    TEST_ASSERT_EQUAL_UINT8(1, db.count);
    TEST_ASSERT_EQUAL_STRING("Test Model", db.entries[0].name);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_MODEL_MOD_AM270, db.entries[0].mod);
}

static void test_parse_missing_filetype_returns_false(void)
{
    const char *text =
        "Version: 2\n"
        "ModelName1: A\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n";

    TEST_ASSERT_FALSE(parse(text));
}

static void test_parse_bad_filetype_returns_false(void)
{
    const char *text =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 2\n"
        "ModelName1: A\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n";

    TEST_ASSERT_FALSE(parse(text));
}

static void test_parse_wrong_version_returns_false(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "Version: 1\n"
        "ModelName1: A\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n";

    TEST_ASSERT_FALSE(parse(text));
}

static void test_parse_missing_version_returns_false(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "ModelName1: A\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n";

    TEST_ASSERT_FALSE(parse(text));
}

static void test_parse_unknown_preset_skipped(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "Version: 2\n"
        "\n"
        "ModelName1: Good\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n"
        "\n"
        "ModelName2: Future preset\n"
        "Frequency2: 433920000\n"
        "PresetName2: FuriHalSubGhzPreset2FSKDev12KAsync\n"
        "\n"
        "ModelName3: Also good\n"
        "Frequency3: 315000000\n"
        "PresetName3: FuriHalSubGhzPreset2FSKDev238Async\n";

    TEST_ASSERT_TRUE(parse(text));
    TEST_ASSERT_EQUAL_UINT8(2, db.count);
    TEST_ASSERT_EQUAL_STRING("Good", db.entries[0].name);
    TEST_ASSERT_EQUAL_STRING("Also good", db.entries[1].name);
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_MODEL_MOD_FM238, db.entries[1].mod);
}

static void test_parse_out_of_range_frequency_skipped(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "Version: 2\n"
        "\n"
        "ModelName1: Too low\n"
        "Frequency1: 100000000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n"
        "\n"
        "ModelName2: Too high\n"
        "Frequency2: 1000000000\n"
        "PresetName2: FuriHalSubGhzPresetOok650Async\n"
        "\n"
        "ModelName3: Lower bound\n"
        "Frequency3: 300000000\n"
        "PresetName3: FuriHalSubGhzPresetOok650Async\n"
        "\n"
        "ModelName4: Upper bound\n"
        "Frequency4: 928000000\n"
        "PresetName4: FuriHalSubGhzPresetOok650Async\n";

    TEST_ASSERT_TRUE(parse(text));
    TEST_ASSERT_EQUAL_UINT8(2, db.count);
    TEST_ASSERT_EQUAL_STRING("Lower bound", db.entries[0].name);
    TEST_ASSERT_EQUAL_STRING("Upper bound", db.entries[1].name);
}

static void test_parse_all_entries_bad_returns_false(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "Version: 2\n"
        "\n"
        "ModelName1: Bad freq\n"
        "Frequency1: 100000000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n";

    TEST_ASSERT_FALSE(parse(text));
}

static void test_parse_long_name_truncated_safely(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "Version: 2\n"
        "\n"
        "ModelName1: 0123456789012345678901234567890123456789ABCDEFGHIJ\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n";

    TEST_ASSERT_TRUE(parse(text));
    TEST_ASSERT_EQUAL_UINT8(1, db.count);
    TEST_ASSERT_EQUAL_UINT32(SUBGHZ_MODELS_DB_NAME_MAX - 1,
                             strlen(db.entries[0].name));
    TEST_ASSERT_EQUAL_CHAR('\0', db.entries[0].name[SUBGHZ_MODELS_DB_NAME_MAX - 1]);
    TEST_ASSERT_EQUAL_STRING("012345678901234567890123456789012345678",
                             db.entries[0].name);
}

static void test_parse_no_model_count_still_parses(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "Version: 2\n"
        "\n"
        "ModelName1: One\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n"
        "\n"
        "ModelName2: Two\n"
        "Frequency2: 315000000\n"
        "PresetName2: FuriHalSubGhzPreset2FSKDev476Async\n";

    TEST_ASSERT_TRUE(parse(text));
    TEST_ASSERT_EQUAL_UINT8(2, db.count);
}

static void test_explicit_model_selection_disables_previous_hopping(void)
{
    bool hopping = true;
    subghz_models_disable_hopping(&hopping);
    TEST_ASSERT_FALSE(hopping);
}

static void test_parse_model_count_larger_than_entries(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "Version: 2\n"
        "\n"
        "ModelCount: 99\n"
        "\n"
        "ModelName1: Only one\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n";

    TEST_ASSERT_TRUE(parse(text));
    TEST_ASSERT_EQUAL_UINT8(1, db.count);
}

static void test_parse_empty_input_returns_false(void)
{
    TEST_ASSERT_FALSE(subghz_models_db_parse("", 0, &db));
    TEST_ASSERT_EQUAL_UINT8(0, db.count);
}

static void test_parse_null_args_return_false(void)
{
    TEST_ASSERT_FALSE(subghz_models_db_parse(NULL, 0, &db));
    TEST_ASSERT_FALSE(subghz_models_db_parse(upstream_db, sizeof(upstream_db), NULL));
}

static void test_parse_unknown_lines_skipped(void)
{
    const char *text =
        "Filetype: ProtoPirate Car Models Database\n"
        "Version: 2\n"
        "Comment: anything could appear here\n"
        "\n"
        "ModelName1: A\n"
        "Frequency1: 433920000\n"
        "PresetName1: FuriHalSubGhzPresetOok650Async\n"
        "FutureField1: some future value\n";

    TEST_ASSERT_TRUE(parse(text));
    TEST_ASSERT_EQUAL_UINT8(1, db.count);
    TEST_ASSERT_FALSE(db.entries[0].custom_preset);
}

static void test_map_preset_known_names(void)
{
    uint8_t mod = 0xFF;
    bool custom = true;

    TEST_ASSERT_TRUE(subghz_models_db_map_preset("FuriHalSubGhzPresetOok270Async",
                                                 false, &mod, &custom));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_MODEL_MOD_AM270, mod);
    TEST_ASSERT_FALSE(custom);

    TEST_ASSERT_TRUE(subghz_models_db_map_preset("FuriHalSubGhzPresetOok650Async",
                                                 false, &mod, &custom));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_MODEL_MOD_AM650, mod);
    TEST_ASSERT_FALSE(custom);

    TEST_ASSERT_TRUE(subghz_models_db_map_preset("FuriHalSubGhzPreset2FSKDev238Async",
                                                 false, &mod, &custom));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_MODEL_MOD_FM238, mod);
    TEST_ASSERT_FALSE(custom);

    TEST_ASSERT_TRUE(subghz_models_db_map_preset("FuriHalSubGhzPreset2FSKDev476Async",
                                                 false, &mod, &custom));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_MODEL_MOD_FM476, mod);
    TEST_ASSERT_FALSE(custom);
}

static void test_map_preset_custom_with_and_without_data(void)
{
    uint8_t mod = 0;
    bool custom = false;

    TEST_ASSERT_TRUE(subghz_models_db_map_preset("FuriHalSubGhzPresetCustom",
                                                 true, &mod, &custom));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_MODEL_MOD_FM476, mod);
    TEST_ASSERT_TRUE(custom);

    custom = false;
    TEST_ASSERT_TRUE(subghz_models_db_map_preset("FuriHalSubGhzPresetCustom",
                                                 false, &mod, &custom));
    TEST_ASSERT_EQUAL_UINT8(SUBGHZ_MODEL_MOD_FM476, mod);
    TEST_ASSERT_TRUE(custom);
}

static void test_map_preset_unknown_returns_false(void)
{
    uint8_t mod = 0xAA;
    bool custom = true;

    TEST_ASSERT_FALSE(subghz_models_db_map_preset("FuriHalSubGhzPreset2FSKDev12KAsync",
                                                  false, &mod, &custom));
    TEST_ASSERT_FALSE(subghz_models_db_map_preset("Custom", false, &mod, &custom));
    TEST_ASSERT_FALSE(subghz_models_db_map_preset("", false, &mod, &custom));
    TEST_ASSERT_EQUAL_UINT8(0xAA, mod);
    TEST_ASSERT_TRUE(custom);
}

static void test_mod_label_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("AM270", subghz_models_db_mod_label(SUBGHZ_MODEL_MOD_AM270));
    TEST_ASSERT_EQUAL_STRING("AM650", subghz_models_db_mod_label(SUBGHZ_MODEL_MOD_AM650));
    TEST_ASSERT_EQUAL_STRING("FM238", subghz_models_db_mod_label(SUBGHZ_MODEL_MOD_FM238));
    TEST_ASSERT_EQUAL_STRING("FM476", subghz_models_db_mod_label(SUBGHZ_MODEL_MOD_FM476));
    TEST_ASSERT_EQUAL_STRING("?", subghz_models_db_mod_label(4));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_upstream_full_db);
    RUN_TEST(test_parse_crlf_identical);
    RUN_TEST(test_parse_leading_blank_lines_and_trailing_ws);
    RUN_TEST(test_parse_missing_filetype_returns_false);
    RUN_TEST(test_parse_bad_filetype_returns_false);
    RUN_TEST(test_parse_wrong_version_returns_false);
    RUN_TEST(test_parse_missing_version_returns_false);
    RUN_TEST(test_parse_unknown_preset_skipped);
    RUN_TEST(test_parse_out_of_range_frequency_skipped);
    RUN_TEST(test_parse_all_entries_bad_returns_false);
    RUN_TEST(test_parse_long_name_truncated_safely);
    RUN_TEST(test_parse_no_model_count_still_parses);
    RUN_TEST(test_explicit_model_selection_disables_previous_hopping);
    RUN_TEST(test_parse_model_count_larger_than_entries);
    RUN_TEST(test_parse_empty_input_returns_false);
    RUN_TEST(test_parse_null_args_return_false);
    RUN_TEST(test_parse_unknown_lines_skipped);
    RUN_TEST(test_map_preset_known_names);
    RUN_TEST(test_map_preset_custom_with_and_without_data);
    RUN_TEST(test_map_preset_unknown_returns_false);
    RUN_TEST(test_mod_label_strings);
    return UNITY_END();
}
