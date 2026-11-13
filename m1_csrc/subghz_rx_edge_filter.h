/* See COPYING.txt for license details. */

#ifndef SUBGHZ_RX_EDGE_FILTER_H
#define SUBGHZ_RX_EDGE_FILTER_H

#include <stdbool.h>
#include <stdint.h>
#include "m1_compile_cfg.h"

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

#endif /* SUBGHZ_RX_EDGE_FILTER_H */
