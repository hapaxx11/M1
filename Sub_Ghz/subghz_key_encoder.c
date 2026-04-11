/* See COPYING.txt for license details. */

/*
 * subghz_key_encoder.c
 *
 * KEY→RAW OOK PWM encoder — extracted from sub_ghz_replay_flipper_file()
 * in m1_sub_ghz.c (lines 1735-1850).
 *
 * This module is hardware-independent.  It uses only the protocol registry
 * for timing lookup and performs pure arithmetic for signal generation.
 *
 * M1 Project — Hapax fork
 */

#include <string.h>
#include "subghz_key_encoder.h"
#include "subghz_protocol_registry.h"

/*============================================================================*/
/* subghz_key_resolve_timing                                                   */
/*============================================================================*/

uint8_t subghz_key_resolve_timing(const SubGhzKeyParams *params, SubGhzKeyTiming *timing)
{
    if (!params || !timing)
        return SUBGHZ_KEY_ERR_UNSUPPORTED;

    int16_t reg_idx = subghz_protocol_find_by_name(params->protocol);

    /* Rolling-code / dynamic protocols — cannot replay from KEY data.
     * Also reject weather sensors and TPMS. */
    if (reg_idx >= 0)
    {
        const SubGhzProtocolDef *proto = &subghz_protocol_registry[reg_idx];
        if (proto->type == SubGhzProtocolTypeDynamic ||
            proto->type == SubGhzProtocolTypeWeather ||
            proto->type == SubGhzProtocolTypeTPMS)
        {
            return SUBGHZ_KEY_ERR_DYNAMIC;
        }
    }

    /* Registry-based timing: use actual te_short / te_long from protocol */
    if (reg_idx >= 0 &&
        subghz_protocol_registry[reg_idx].type == SubGhzProtocolTypeStatic &&
        subghz_protocol_registry[reg_idx].timing.te_short > 0 &&
        subghz_protocol_registry[reg_idx].timing.te_long > 0)
    {
        const SubGhzBlockConst *t = &subghz_protocol_registry[reg_idx].timing;
        timing->te_short = (params->te > 0) ? params->te : t->te_short;
        timing->te_long  = (uint32_t)timing->te_short * t->te_long / t->te_short;
        timing->gap_low  = timing->te_short * 30;
    }
    /* Legacy strstr() fallback for unknown protocols */
    else if (strstr(params->protocol, "Princeton") || strstr(params->protocol, "Gate") ||
             strstr(params->protocol, "Holtek") || strstr(params->protocol, "Linear") ||
             strstr(params->protocol, "SMC5326") || strstr(params->protocol, "Power") ||
             strstr(params->protocol, "iDo"))
    {
        /* 1:3 ratio protocols */
        timing->te_short = (params->te > 0) ? params->te : 350;
        timing->te_long  = timing->te_short * 3;
        timing->gap_low  = timing->te_short * 30;
    }
    else if (strstr(params->protocol, "CAME") || strstr(params->protocol, "Nice") ||
             strstr(params->protocol, "Ansonic"))
    {
        /* 1:2 ratio protocols */
        timing->te_short = (params->te > 0) ? params->te : 320;
        timing->te_long  = timing->te_short * 2;
        timing->gap_low  = timing->te_short * 36;
    }
    else
    {
        return SUBGHZ_KEY_ERR_UNSUPPORTED;
    }

    return SUBGHZ_KEY_OK;
}

/*============================================================================*/
/* subghz_key_encode                                                           */
/*============================================================================*/

uint32_t subghz_key_encode(const SubGhzKeyParams *params,
                            const SubGhzKeyTiming *timing,
                            SubGhzRawPair *out,
                            uint32_t max_pairs,
                            uint8_t repetitions)
{
    if (!params || !timing || !out || max_pairs == 0 || repetitions == 0)
        return 0;

    /* Clamp bit count to 64 (uint64_t max) */
    uint32_t bit_count = params->bit_count;
    if (bit_count > 64) bit_count = 64;
    if (bit_count == 0) return 0;

    /* Each repetition = bit_count data pairs + 1 sync gap pair */
    uint32_t pairs_per_rep = bit_count + 1;
    uint32_t total_pairs   = (uint32_t)repetitions * pairs_per_rep;

    if (total_pairs > max_pairs)
        return 0;   /* overflow */

    uint32_t idx = 0;

    for (uint8_t rep = 0; rep < repetitions; rep++)
    {
        uint64_t mask = 1ULL << (bit_count - 1);

        for (uint32_t b = 0; b < bit_count; b++)
        {
            if (params->key_value & mask)
            {
                /* Bit 1: long HIGH, short LOW */
                out[idx].high_us = timing->te_long;
                out[idx].low_us  = timing->te_short;
            }
            else
            {
                /* Bit 0: short HIGH, long LOW */
                out[idx].high_us = timing->te_short;
                out[idx].low_us  = timing->te_long;
            }
            mask >>= 1;
            idx++;
        }

        /* Sync gap: short HIGH pulse + long LOW gap */
        out[idx].high_us = timing->te_short;
        out[idx].low_us  = timing->gap_low;
        idx++;
    }

    return idx;
}
