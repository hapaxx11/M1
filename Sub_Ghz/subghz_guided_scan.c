/* See COPYING.txt for license details. */

#include "subghz_guided_scan.h"
#include "rf_smart_scan.h"
#include <string.h>

void subghz_guided_plan_reset(SubGhzGuidedPlan *plan)
{
    if (plan != NULL)
        memset(plan, 0, sizeof(*plan));
}

static uint8_t guided_mod_from_hit(const rf_sweep_hit_t *hit)
{
    rf_mod_family_t mod = hit->mod;
    const rf_protocol_sig_t *sig = hit->sig != NULL ? hit->sig : hit->confirmed_sig;

    if (mod == RF_MOD_UNKNOWN && sig != NULL)
        mod = sig->mod;
    return mod == RF_MOD_FSK ? SUBGHZ_GUIDED_MOD_FM476 : SUBGHZ_GUIDED_MOD_AM650;
}

bool subghz_guided_entry_from_hit(SubGhzGuidedEntry *entry,
                                  const rf_sweep_hit_t *hit)
{
    if (entry == NULL || hit == NULL || !rf_smart_scan_freq_valid(hit->freq_hz))
        return false;

    entry->freq_hz = hit->freq_hz;
    entry->mod_idx = guided_mod_from_hit(hit);
    return true;
}

void subghz_guided_plan_from_report(SubGhzGuidedPlan *plan,
                                    const rf_sweep_report_t *report)
{
    if (plan == NULL)
        return;
    subghz_guided_plan_reset(plan);
    if (report == NULL)
        return;

    for (uint8_t i = 0; i < report->count; i++)
    {
        SubGhzGuidedEntry candidate;
        bool duplicate = false;
        if (!subghz_guided_entry_from_hit(&candidate, &report->hits[i]))
            continue;

        for (uint8_t j = 0; j < plan->count; j++)
        {
            if (plan->entries[j].freq_hz == candidate.freq_hz &&
                plan->entries[j].mod_idx == candidate.mod_idx)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;
        if (plan->count >= SUBGHZ_GUIDED_SCAN_MAX_FREQS)
            break;
        plan->entries[plan->count++] = candidate;
    }
}

bool subghz_guided_scan_active(bool enabled, bool hopping,
                               const SubGhzGuidedPlan *plan)
{
    return enabled && hopping && plan != NULL && plan->count > 0U;
}

uint8_t subghz_guided_next_index(uint8_t current, uint8_t count)
{
    if (count == 0U)
        return 0U;
    return (uint8_t)((current + 1U) % count);
}
