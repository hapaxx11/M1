/* See COPYING.txt for license details. */

#ifndef SUBGHZ_MODEM_PROFILE_H
#define SUBGHZ_MODEM_PROFILE_H

#include <stdint.h>

#define SUBGHZ_MOD_PRESET_COUNT  7U

typedef struct {
    const char *label;
    const char *flipper_name;
    uint8_t     mod;
    uint8_t     si446x_mod_type;
    uint32_t    deviation_hz;
    uint32_t    data_rate_sps;
    uint32_t    rx_bandwidth_hz;
    const uint8_t *commands;
    uint16_t    commands_size;
} SubGhzModemProfile;

extern const SubGhzModemProfile subghz_modem_profiles[SUBGHZ_MOD_PRESET_COUNT];

const SubGhzModemProfile *subghz_modem_profile_get(uint8_t index);
int16_t subghz_modem_profile_find_flipper(const char *flipper_name);

#endif /* SUBGHZ_MODEM_PROFILE_H */
