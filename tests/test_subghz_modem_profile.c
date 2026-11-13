/* See COPYING.txt for license details. */

#include "unity.h"
#include "subghz_modem_profile.h"
#include <stdbool.h>

void setUp(void) { }
void tearDown(void) { }

static uint32_t profile_fnv1a(const SubGhzModemProfile *profile)
{
    uint32_t hash = 0x811C9DC5UL;
    for (uint16_t i = 0; i < profile->commands_size; i++)
        hash = (hash ^ profile->commands[i]) * 0x01000193UL;
    return hash;
}

void test_all_flipper_presets_have_distinct_si4463_profiles(void)
{
    static const char *labels[] = {
        "AM270", "AM650", "FM238", "FM476", "FM12K", "MSK100K", "GFSK10K"
    };
    static const uint32_t deviations[] = { 0, 0, 2380, 47607, 12695, 47607, 19043 };
    static const uint32_t rates[] = { 3794, 3794, 4798, 4798, 4798, 99970, 9996 };
    static const uint32_t bandwidths[] = { 270833, 650000, 270833, 270833, 270833, 14509, 101563 };
    static const uint8_t types[] = { 1, 1, 2, 2, 2, 6, 3 };

    TEST_ASSERT_EQUAL_UINT8(7, SUBGHZ_MOD_PRESET_COUNT);
    for (uint8_t i = 0; i < SUBGHZ_MOD_PRESET_COUNT; i++)
    {
        const SubGhzModemProfile *profile = subghz_modem_profile_get(i);
        TEST_ASSERT_NOT_NULL(profile);
        TEST_ASSERT_EQUAL_STRING(labels[i], profile->label);
        TEST_ASSERT_EQUAL_UINT32(deviations[i], profile->deviation_hz);
        TEST_ASSERT_EQUAL_UINT32(rates[i], profile->data_rate_sps);
        TEST_ASSERT_EQUAL_UINT32(bandwidths[i], profile->rx_bandwidth_hz);
        TEST_ASSERT_EQUAL_UINT8(types[i], profile->si446x_mod_type);
    }
}

void test_complete_wds_profile_streams_are_preserved(void)
{
    static const uint32_t hashes[] = {
        0x7229581BUL, 0x8F2BCBEEUL, 0xBCF99C0CUL, 0x6943DA91UL,
        0x99D313F6UL, 0x9246BC75UL, 0xC428D911UL
    };
    static const uint8_t rate_scales[] = { 10, 10, 10, 10, 10, 20, 40 };

    for (uint8_t i = 0; i < SUBGHZ_MOD_PRESET_COUNT; i++)
    {
        const SubGhzModemProfile *profile = subghz_modem_profile_get(i);
        uint16_t offset = 0;
        uint8_t blocks = 0;
        uint8_t filter_blocks = 0;
        bool found_bcr = false;
        bool found_raw = false;
        bool found_synth = false;
        uint32_t rate = 0;

        TEST_ASSERT_EQUAL_UINT16(180, profile->commands_size);
        TEST_ASSERT_EQUAL_HEX32(hashes[i], profile_fnv1a(profile));
        while (offset < profile->commands_size)
        {
            const uint8_t *command = &profile->commands[offset];
            uint8_t count;
            TEST_ASSERT_TRUE((uint16_t)(profile->commands_size - offset) >= 4U);
            TEST_ASSERT_EQUAL_HEX8(0x11, command[0]);
            count = command[2];
            TEST_ASSERT_TRUE(count > 0U && count <= 12U);
            TEST_ASSERT_TRUE((uint16_t)(profile->commands_size - offset) >= count + 4U);
            TEST_ASSERT_TRUE(command[1] == 0x20 || command[1] == 0x21 || command[1] == 0x23);
            if (command[1] == 0x21)
                filter_blocks++;
            if (command[1] == 0x20 && command[3] == 0x24)
                found_bcr = true;
            if (command[1] == 0x20 && command[3] == 0x45)
            {
                found_raw = true;
                TEST_ASSERT_BITS_HIGH_MESSAGE(0x80, command[4],
                    "every preset must enable the direct raw-data output");
            }
            if (command[1] == 0x23 && command[3] == 0x00)
                found_synth = true;
            offset = (uint16_t)(offset + count + 4U);
            blocks++;
        }
        TEST_ASSERT_EQUAL_UINT16(profile->commands_size, offset);
        TEST_ASSERT_EQUAL_UINT8(16, blocks);
        TEST_ASSERT_EQUAL_UINT8(3, filter_blocks);
        TEST_ASSERT_TRUE(found_bcr);
        TEST_ASSERT_TRUE(found_raw);
        TEST_ASSERT_TRUE(found_synth);
        TEST_ASSERT_EQUAL_UINT8(profile->si446x_mod_type, profile->commands[4] & 0x07U);
        rate = ((uint32_t)profile->commands[7] << 16) |
               ((uint32_t)profile->commands[8] << 8) | profile->commands[9];
        TEST_ASSERT_EQUAL_UINT32(profile->data_rate_sps * rate_scales[i], rate);
    }
}

void test_flipper_lookup_covers_new_and_existing_presets(void)
{
    TEST_ASSERT_EQUAL_INT16(0, subghz_modem_profile_find_flipper("FuriHalSubGhzPresetOok270Async"));
    TEST_ASSERT_EQUAL_INT16(1, subghz_modem_profile_find_flipper("FuriHalSubGhzPresetOok650Async"));
    TEST_ASSERT_EQUAL_INT16(2, subghz_modem_profile_find_flipper("FuriHalSubGhzPreset2FSKDev238Async"));
    TEST_ASSERT_EQUAL_INT16(3, subghz_modem_profile_find_flipper("FuriHalSubGhzPreset2FSKDev476Async"));
    TEST_ASSERT_EQUAL_INT16(4, subghz_modem_profile_find_flipper("FuriHalSubGhzPreset2FSKDev12KAsync"));
    TEST_ASSERT_EQUAL_INT16(5, subghz_modem_profile_find_flipper("FuriHalSubGhzPresetMSK99_97KbAsync"));
    TEST_ASSERT_EQUAL_INT16(6, subghz_modem_profile_find_flipper("FuriHalSubGhzPresetGFSK9_99KbAsync"));
}

void test_invalid_indices_and_unknown_names_are_rejected(void)
{
    TEST_ASSERT_NULL(subghz_modem_profile_get(SUBGHZ_MOD_PRESET_COUNT));
    TEST_ASSERT_NULL(subghz_modem_profile_get(255));
    TEST_ASSERT_EQUAL_INT16(-1, subghz_modem_profile_find_flipper(NULL));
    TEST_ASSERT_EQUAL_INT16(-1, subghz_modem_profile_find_flipper("Unknown"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_flipper_presets_have_distinct_si4463_profiles);
    RUN_TEST(test_complete_wds_profile_streams_are_preserved);
    RUN_TEST(test_flipper_lookup_covers_new_and_existing_presets);
    RUN_TEST(test_invalid_indices_and_unknown_names_are_rejected);
    return UNITY_END();
}
