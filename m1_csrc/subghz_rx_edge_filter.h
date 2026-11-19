/* See COPYING.txt for license details. */

#ifndef SUBGHZ_RX_EDGE_FILTER_H
#define SUBGHZ_RX_EDGE_FILTER_H

#include <stdbool.h>
#include <stdint.h>
#include "m1_compile_cfg.h"
#include "subghz_modem_profile.h"

#define SUBGHZ_FSK_NOISE_PULSE_WIDTH  20U

static inline bool subghz_rx_edge_accept(uint32_t duration_us,
                                         bool raw_recording,
                                         bool fsk_modulation)
{
#ifdef M1_APP_SUB_GHZ_RAW_DATA_RX_NOISE_FILTER_ENABLE
    if (fsk_modulation && duration_us < SUBGHZ_FSK_NOISE_PULSE_WIDTH)
        return false;
    if (raw_recording &&
        duration_us < M1_APP_SUB_GHZ_RAW_DATA_NOISE_PULSE_WIDTH)
        return false;
#else
    (void)duration_us;
    (void)raw_recording;
    (void)fsk_modulation;
#endif
    return true;
}

static inline uint32_t subghz_rx_edge_half_symbol_us(const SubGhzModemProfile *profile)
{
    if (profile == 0 || profile->data_rate_sps == 0U)
        return 1U;
    uint32_t half_symbol = 500000UL / profile->data_rate_sps;
    return half_symbol > 0U ? half_symbol : 1U;
}

static inline bool subghz_rx_edge_accept_profile(uint32_t duration_us,
                                                 bool raw_recording,
                                                 const SubGhzModemProfile *profile)
{
#ifdef M1_APP_SUB_GHZ_RAW_DATA_RX_NOISE_FILTER_ENABLE
    if (profile != 0 && profile->mod >= 2U)
    {
        uint32_t half_symbol = subghz_rx_edge_half_symbol_us(profile);
        uint32_t fsk_limit = half_symbol < SUBGHZ_FSK_NOISE_PULSE_WIDTH ?
                             half_symbol : SUBGHZ_FSK_NOISE_PULSE_WIDTH;
        if (duration_us < fsk_limit)
            return false;
        uint32_t raw_limit = half_symbol < M1_APP_SUB_GHZ_RAW_DATA_NOISE_PULSE_WIDTH ?
                             half_symbol : M1_APP_SUB_GHZ_RAW_DATA_NOISE_PULSE_WIDTH;
        if (raw_recording && duration_us < raw_limit)
            return false;
    }
    else if (raw_recording &&
             duration_us < M1_APP_SUB_GHZ_RAW_DATA_NOISE_PULSE_WIDTH)
    {
        return false;
    }
#else
    (void)duration_us;
    (void)raw_recording;
    (void)profile;
#endif
    return true;
}

#endif /* SUBGHZ_RX_EDGE_FILTER_H */
