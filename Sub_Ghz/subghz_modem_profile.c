/* See COPYING.txt for license details. */

#include "subghz_modem_profile.h"
#include "subghz_modem_profile_data.h"
#include <string.h>

#define SCAN_MOD_OOK   0U
#define SCAN_MOD_FSK   2U
#define SCAN_MOD_GFSK  3U
#define SCAN_MOD_MSK   4U

#define SI446X_MOD_OOK   1U
#define SI446X_MOD_2FSK  2U
#define SI446X_MOD_2GFSK 3U
#define SI446X_MOD_MSK   6U

static const uint8_t profile_am270_commands[] = { SUBGHZ_PROFILE_AM270_COMMANDS };
static const uint8_t profile_am650_commands[] = { SUBGHZ_PROFILE_AM650_COMMANDS };
static const uint8_t profile_fm238_commands[] = { SUBGHZ_PROFILE_FM238_COMMANDS };
static const uint8_t profile_fm476_commands[] = { SUBGHZ_PROFILE_FM476_COMMANDS };
static const uint8_t profile_fm12k_commands[] = { SUBGHZ_PROFILE_FM12K_COMMANDS };
static const uint8_t profile_msk100k_commands[] = { SUBGHZ_PROFILE_MSK100K_COMMANDS };
static const uint8_t profile_gfsk10k_commands[] = { SUBGHZ_PROFILE_GFSK10K_COMMANDS };

const SubGhzModemProfile subghz_modem_profiles[SUBGHZ_MOD_PRESET_COUNT] = {
    { "AM270",  "FuriHalSubGhzPresetOok270Async",       SCAN_MOD_OOK,  SI446X_MOD_OOK,   0,     3794,  270833,
      profile_am270_commands, sizeof(profile_am270_commands) },
    { "AM650",  "FuriHalSubGhzPresetOok650Async",       SCAN_MOD_OOK,  SI446X_MOD_OOK,   0,     3794,  650000,
      profile_am650_commands, sizeof(profile_am650_commands) },
    { "FM238",  "FuriHalSubGhzPreset2FSKDev238Async",   SCAN_MOD_FSK,  SI446X_MOD_2FSK,  2380,  4798,  270833,
      profile_fm238_commands, sizeof(profile_fm238_commands) },
    { "FM476",  "FuriHalSubGhzPreset2FSKDev476Async",   SCAN_MOD_FSK,  SI446X_MOD_2FSK,  47607, 4798,  270833,
      profile_fm476_commands, sizeof(profile_fm476_commands) },
    { "FM12K",  "FuriHalSubGhzPreset2FSKDev12KAsync",   SCAN_MOD_FSK,  SI446X_MOD_2FSK,  12695, 4798,  270833,
      profile_fm12k_commands, sizeof(profile_fm12k_commands) },
    { "MSK100K", "FuriHalSubGhzPresetMSK99_97KbAsync",  SCAN_MOD_MSK,  SI446X_MOD_MSK,   47607, 99970, 14509,
      profile_msk100k_commands, sizeof(profile_msk100k_commands) },
    { "GFSK10K", "FuriHalSubGhzPresetGFSK9_99KbAsync",  SCAN_MOD_GFSK, SI446X_MOD_2GFSK, 19043, 9996,  101563,
      profile_gfsk10k_commands, sizeof(profile_gfsk10k_commands) },
};

const SubGhzModemProfile *subghz_modem_profile_get(uint8_t index)
{
    return index < SUBGHZ_MOD_PRESET_COUNT ? &subghz_modem_profiles[index] : 0;
}

int16_t subghz_modem_profile_find_flipper(const char *flipper_name)
{
    if (flipper_name == 0)
        return -1;
    for (uint8_t i = 0; i < SUBGHZ_MOD_PRESET_COUNT; i++)
    {
        if (strcmp(flipper_name, subghz_modem_profiles[i].flipper_name) == 0)
            return (int16_t)i;
    }
    return -1;
}
