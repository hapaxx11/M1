/* See COPYING.txt for license details. */

#ifndef SUBGHZ_GUIDED_SCAN_H
#define SUBGHZ_GUIDED_SCAN_H

#include <stdbool.h>
#include <stdint.h>
#include "rf_sweep.h"

#define SUBGHZ_GUIDED_SCAN_MAX_FREQS  RF_SWEEP_MAX_HITS
#define SUBGHZ_GUIDED_MOD_AM650       1U
#define SUBGHZ_GUIDED_MOD_FM476       3U

typedef struct {
    uint32_t freq_hz;
    uint8_t  mod_idx;
} SubGhzGuidedEntry;

typedef struct {
    SubGhzGuidedEntry entries[SUBGHZ_GUIDED_SCAN_MAX_FREQS];
    uint8_t           count;
} SubGhzGuidedPlan;

void subghz_guided_plan_reset(SubGhzGuidedPlan *plan);
bool subghz_guided_entry_from_hit(SubGhzGuidedEntry *entry,
                                  const rf_sweep_hit_t *hit);
void subghz_guided_plan_from_report(SubGhzGuidedPlan *plan,
                                    const rf_sweep_report_t *report);
bool subghz_guided_scan_active(bool enabled, bool hopping,
                               const SubGhzGuidedPlan *plan);
uint8_t subghz_guided_next_index(uint8_t current, uint8_t count);

#endif /* SUBGHZ_GUIDED_SCAN_H */
