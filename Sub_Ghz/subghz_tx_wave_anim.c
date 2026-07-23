/* See COPYING.txt for license details. */

/**
 * @file   subghz_tx_wave_anim.c
 * @brief  Scrolling sine-wave TX animation — pure logic.
 *
 * See subghz_tx_wave_anim.h for the design rationale.  The lookup table and
 * folding trick below are ported from Momentum's
 * `subghz_read_raw_tab_sin()` (a quarter-wave sine table mirrored across all
 * four quadrants via the sign/mirror bits of the index), which is a compact
 * way to get a full-period sine approximation without floating point.
 */

#include "subghz_tx_wave_anim.h"
#include <stddef.h>

/** Quarter-wave sine table, 0..127 (approximates sin(x) * 127 for
 *  x in [0, pi/2), sampled at 64 points). */
static const uint8_t s_quarter_sin[64] = {
    0,   3,   6,   9,   12,  16,  19,  22,  25,  28,  31,
    34,  37,  40,  43,  46,  49,  51,  54,  57,  60,  63,
    65,  68,  71,  73,  76,  78,  81,  83,  85,  88,  90,
    92,  94,  96,  98,  100, 102, 104, 106, 107, 109, 111,
    112, 113, 115, 116, 117, 118, 120, 121, 122, 122, 123,
    124, 125, 125, 126, 126, 126, 127, 127, 127
};

/** Full-period signed sine sample, folding the quarter-wave table across
 *  all four quadrants of a free-running uint8_t index (0..255 maps to one
 *  full 0..2*pi cycle).  Returns a value in [-127, 127]. */
static int8_t full_period_sine(uint8_t x)
{
    int8_t r = (int8_t)s_quarter_sin[((x & 0x40U) ? (~x) : x) & 0x3fU];
    return (x & 0x80U) ? (int8_t)(-r) : r;
}

void subghz_tx_wave_anim_step(uint8_t *phase)
{
    if (phase == NULL) return;

    uint8_t p = (uint8_t)(*phase % SUBGHZ_TX_WAVE_ANIM_PHASE_PERIOD);
    *phase = (uint8_t)((p + 1U) % SUBGHZ_TX_WAVE_ANIM_PHASE_PERIOD);
}

int8_t subghz_tx_wave_anim_sample(uint16_t column, uint8_t phase, uint8_t amplitude)
{
    if (amplitude == 0U) return 0;

    /* Scroll speed: one full sine period (256 index steps) completes every
     * SUBGHZ_TX_WAVE_ANIM_PHASE_PERIOD ticks, i.e. 4 index steps per tick
     * (256 / 64 == 4). Column simply shifts the wave horizontally so
     * adjacent columns trace out the waveform shape. */
    uint8_t idx = (uint8_t)(column + (uint16_t)phase * 4U);
    int8_t  raw = full_period_sine(idx);

    /* Scale from the LUT's +/-127 range down to the caller's amplitude. */
    int32_t scaled = ((int32_t)raw * (int32_t)amplitude) / 127;
    return (int8_t)scaled;
}
