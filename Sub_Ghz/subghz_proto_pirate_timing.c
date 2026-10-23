/* See COPYING.txt for license details. */

/**
 * @file   subghz_proto_pirate_timing.c
 * @brief  Proto Pirate — pure-logic timing analysis (hardware-independent).
 *
 * Implements pptime_analyze() and pptime_match() for use by the M1
 * Timing Tuner scene.  No hardware registers, RTOS, or display state
 * are touched here — the entire file is host-testable.
 *
 * Protocol timing values are sourced from the M1 protocol registry — the
 * decoder authority — via pptime_proto_ref_get(); nothing is copied by hand.
 */

#include "subghz_proto_pirate_timing.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_protocol_registry.h"
#include <string.h>

/*============================================================================*/
/* Automotive protocol reference table                                        */
/*                                                                            */
/* Display names plus the registry index each row draws its timing from.      */
/* The registry (decoder) values are authoritative; the previous hand-copied  */
/* numbers had drifted from the real decoders (e.g. Subaru 620/1620 vs the    */
/* actual 800/1600).                                                          */
/*============================================================================*/

const pptime_proto_entry_t pptime_proto_table[] = {
    /* name                  registry index */
    { "KeeLoq",              KEELOQ          },
    { "Star Line",           STAR_LINE       },
    { "Princeton",           PRINCETON       },
    { "CAME",                CAME_12BIT      },
    { "Nice FLO",            NICE_FLO        },
    { "Holtek HT12X",        HOLTEK_HT12E    },
    { "GateTX",              GATE_TX         },
    { "SMC5326",             SMC5326         },
    { "Chrysler V0",         CHRYSLER_V0     },
    { "Subaru",              SUBARU_PP       },
    { "Honda V1",            HONDA_V1        },
    { "Kia V1",              KIA_V1          },
    { "PSA AM",              PSA             },
    { "CAME TWEE",           CAME_TWEE       },
    { "CAME Atomo",          CAME_ATOMO      },
    { "Alutech AT-4N",       ALUTECH_AT4N    },
    { "Nice FloR-S",         NICE_FLOR_S     },
    { "Ansonic",             ANSONIC         },
    { "iDo 117",             IDO             },
    { "BETT",                BETT            },
    { "Nero Radio",          NERO_RADIO      },
    { "Clemsa",              CLEMSA          },
    { "FireFly",             FIREFLY         },
    { "Linear",              LINEAR_10BIT    },
};

const uint8_t pptime_proto_table_count =
    (uint8_t)(sizeof(pptime_proto_table) / sizeof(pptime_proto_table[0]));

pptime_proto_ref_t pptime_proto_ref_get(uint8_t index)
{
    pptime_proto_ref_t ref = { NULL, 0, 0, 0 };
    if (index >= pptime_proto_table_count)
        return ref;

    const pptime_proto_entry_t *entry = &pptime_proto_table[index];
    const SubGhzBlockConst *timing =
        &subghz_protocol_registry[entry->registry_idx].timing;

    ref.name     = entry->name;
    ref.te_short = timing->te_short;
    ref.te_long  = timing->te_long;
    ref.te_delta = timing->te_delta
                   ? timing->te_delta
                   : (uint16_t)(((uint32_t)timing->te_short *
                                 timing->te_tolerance_pct) / 100u);
    return ref;
}

/*============================================================================*/
/* pptime_analyze                                                              */
/*============================================================================*/

void pptime_analyze(const uint16_t *durations, size_t count,
                    const pptime_proto_ref_t *ref,
                    pptime_stats_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    if (!durations || count == 0)
        return;

    int32_t te_short, te_long, te_delta;
    if (ref && ref->te_short > 0 && ref->te_long > 0)
    {
        te_short  = (int32_t)ref->te_short;
        te_long   = (int32_t)ref->te_long;
        te_delta  = (int32_t)ref->te_delta;
    }
    else
    {
        /* Reasonable defaults when no reference is supplied */
        te_short  = 400;
        te_long   = 800;
        te_delta  = 150;
    }

    /* Threshold = midpoint between expected short and long */
    int32_t threshold = (te_short + te_long) / 2;

    /* Valid range = ±2×te_delta around the short/long extremes */
    int32_t min_valid = te_short - (te_delta * 2);
    if (min_valid < 50) min_valid = 50;
    int32_t max_valid = te_long + (te_delta * 2);

    int64_t short_sum = 0;
    int64_t long_sum  = 0;

    int32_t min_short = INT32_MAX;
    int32_t max_short = 0;
    int32_t min_long  = INT32_MAX;
    int32_t max_long  = 0;

    size_t n_short = 0;
    size_t n_long  = 0;
    size_t n_total = 0;

    for (size_t i = 0; i < count; i++)
    {
        int32_t dur = (int32_t)durations[i];

        if (dur < min_valid || dur > max_valid)
            continue;

        n_total++;

        if (dur < threshold)
        {
            short_sum += dur;
            n_short++;
            if (dur < min_short) min_short = dur;
            if (dur > max_short) max_short = dur;
        }
        else
        {
            long_sum += dur;
            n_long++;
            if (dur < min_long) min_long = dur;
            if (dur > max_long) max_long = dur;
        }
    }

    out->n_total = n_total;
    out->n_short = n_short;
    out->n_long  = n_long;

    if (n_short > 0)
    {
        out->avg_short = (int32_t)(short_sum / (int64_t)n_short);
        out->min_short = min_short;
        out->max_short = max_short;
    }

    if (n_long > 0)
    {
        out->avg_long = (int32_t)(long_sum / (int64_t)n_long);
        out->min_long = min_long;
        out->max_long = max_long;
    }
}

/*============================================================================*/
/* pptime_match                                                                */
/*============================================================================*/

pptime_match_result_t pptime_match(const pptime_stats_t *stats,
                                   const pptime_proto_ref_t *ref)
{
    if (!stats || !ref || ref->te_short == 0 || ref->te_long == 0)
        return PPTIME_MATCH_NO_DATA;

    if (stats->n_short < 4 || stats->n_long < 4)
        return PPTIME_MATCH_NO_DATA;

    int32_t te_s = (int32_t)ref->te_short;
    int32_t te_l = (int32_t)ref->te_long;
    int32_t tol  = (int32_t)ref->te_delta;

    bool short_ok = (stats->avg_short >= te_s - tol) &&
                    (stats->avg_short <= te_s + tol);
    bool long_ok  = (stats->avg_long  >= te_l - tol) &&
                    (stats->avg_long  <= te_l + tol);

    if (short_ok && long_ok)
        return PPTIME_MATCH_OK;

    if (!short_ok && !long_ok)
        return PPTIME_MATCH_MISMATCH;

    if (!short_ok)
    {
        return (stats->avg_short > te_s) ? PPTIME_MATCH_SHORT_HI
                                         : PPTIME_MATCH_SHORT_LO;
    }

    /* long not OK */
    return (stats->avg_long > te_l) ? PPTIME_MATCH_LONG_HI
                                    : PPTIME_MATCH_LONG_LO;
}

/*============================================================================*/
/* pptime_match_str                                                            */
/*============================================================================*/

const char *pptime_match_str(pptime_match_result_t result)
{
    switch (result)
    {
        case PPTIME_MATCH_OK:        return "MATCH";
        case PPTIME_MATCH_SHORT_HI:  return "Short too long";
        case PPTIME_MATCH_SHORT_LO:  return "Short too short";
        case PPTIME_MATCH_LONG_HI:   return "Long too long";
        case PPTIME_MATCH_LONG_LO:   return "Long too short";
        case PPTIME_MATCH_MISMATCH:  return "Mismatch";
        case PPTIME_MATCH_NO_DATA:
        default:                     return "No data";
    }
}
