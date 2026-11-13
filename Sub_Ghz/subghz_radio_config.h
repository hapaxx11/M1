/* See COPYING.txt for license details. */

#ifndef SUBGHZ_RADIO_CONFIG_H
#define SUBGHZ_RADIO_CONFIG_H

#include <stdbool.h>

/*
 * The stock low-band profiles are OOK-only and the stock 915 profile is FSK.
 * A requested modulation that differs from the profile must use the custom
 * path, which loads the appropriate modem profile and then retunes it.
 */
static inline bool subghz_radio_force_custom_config(bool band_is_915,
                                                    bool modulation_is_fsk)
{
    return band_is_915 != modulation_is_fsk;
}

#endif /* SUBGHZ_RADIO_CONFIG_H */
