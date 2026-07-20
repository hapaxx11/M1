/* See COPYING.txt for license details. */

/*
 * rf_scan_plan.c
 *
 * Pure-logic probe plan + cursor for the Signal Identifier sweep.  See
 * rf_scan_plan.h.  No hardware dependencies.
 *
 * The plan is a curated set of the most common Sub-GHz ISM frequencies used
 * by remotes, sensors and IoT devices.  It is deliberately compact so a full
 * pass completes quickly enough to feel "live" on the device.
 *
 * M1 Project — Hapax fork
 */

#include "rf_scan_plan.h"

#include <stddef.h>   /* NULL */

/* use_915 is precomputed as (freq >= RF_SCAN_915_BOUNDARY_HZ) and verified by
 * a unit test so the table and the boundary rule can never silently diverge. */
static const rf_scan_point_t k_plan[] = {
    { 300000000UL, false, "300.00" },
    { 303875000UL, false, "303.87" },
    { 310000000UL, false, "310.00" },
    { 315000000UL, false, "315.00" },
    { 318000000UL, false, "318.00" },
    { 330000000UL, false, "330.00" },
    { 345000000UL, false, "345.00" },
    { 390000000UL, false, "390.00" },
    { 418000000UL, false, "418.00" },
    { 433420000UL, false, "433.42" },
    { 433920000UL, false, "433.92" },
    { 434420000UL, false, "434.42" },
    { 868350000UL, true,  "868.35" },
    { 915000000UL, true,  "915.00" },
};

#define RF_SCAN_PLAN_COUNT  (sizeof(k_plan) / sizeof(k_plan[0]))

uint16_t rf_scan_plan_count(void)
{
    return (uint16_t)RF_SCAN_PLAN_COUNT;
}

const rf_scan_point_t *rf_scan_plan_point(uint16_t idx)
{
    if (idx >= RF_SCAN_PLAN_COUNT)
        return NULL;
    return &k_plan[idx];
}

void rf_scan_cursor_reset(rf_scan_cursor_t *cur)
{
    if (cur != NULL) {
        cur->idx  = 0;
        cur->pass = 0;
    }
}

const rf_scan_point_t *rf_scan_cursor_point(const rf_scan_cursor_t *cur)
{
    if (cur == NULL || RF_SCAN_PLAN_COUNT == 0)
        return NULL;
    uint16_t idx = cur->idx;
    if (idx >= RF_SCAN_PLAN_COUNT)
        idx = 0;
    return &k_plan[idx];
}

bool rf_scan_cursor_advance(rf_scan_cursor_t *cur)
{
    if (cur == NULL || RF_SCAN_PLAN_COUNT == 0)
        return false;

    cur->idx++;
    if (cur->idx >= RF_SCAN_PLAN_COUNT) {
        cur->idx = 0;
        cur->pass++;
        return true;
    }
    return false;
}
