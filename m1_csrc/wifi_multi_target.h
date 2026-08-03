/* See COPYING.txt for license details. */

/**
 * @file   wifi_multi_target.h
 * @brief  Pure-logic multi-target AP list builder — no HAL, RTOS, or display deps.
 *
 * Extracted following the Preferred Modularization Pattern.
 * wifi_multi_target_build() is a pure function: it operates only on
 * caller-supplied arrays and contains no global-state references, making it
 * trivially host-testable.
 *
 * Usage pattern (within m1_wifi.c):
 *   wifi_ap_t targets[WIFI_AP_MAX];
 *   uint16_t n = wifi_multi_target_build(ap_list, ap_count,
 *                                         targets, WIFI_AP_MAX);
 *   // operate on targets[0..n-1]
 *
 * M1 Project
 */

#ifndef WIFI_MULTI_TARGET_H_
#define WIFI_MULTI_TARGET_H_

#include <stdint.h>
#include "wifi_ap_record.h"

/**
 * Build a flat target list from the selected entries of an AP list.
 *
 * Copies every entry where selected == true from @p list[] into @p out[],
 * up to @p out_max entries.  Entries beyond @p out_max are silently dropped.
 *
 * The function is pure: it reads @p list and writes @p out; it touches no
 * global state and has no side effects.
 *
 * @param list     Source AP array (may be NULL → returns 0).
 * @param count    Number of entries in @p list.
 * @param out      Destination array for selected AP records.
 * @param out_max  Capacity of @p out (maximum entries to copy).
 * @return         Number of entries written to @p out.
 */
uint16_t wifi_multi_target_build(const wifi_ap_t *list, uint16_t count,
                                  wifi_ap_t       *out,  uint16_t out_max);

#endif /* WIFI_MULTI_TARGET_H_ */
