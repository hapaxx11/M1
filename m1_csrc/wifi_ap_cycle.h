/* See COPYING.txt for license details. */

/**
 * @file   wifi_ap_cycle.h
 * @brief  Pure-logic AP-cycling helpers — no HAL, RTOS, or display deps.
 *
 * Extracted for the WiFi cleanup Phase 3 selected-network Target context
 * (documentation/wifi_cleanup_plan.md §3.2).  When one SSID is served by
 * several BSSIDs, the Target actions iterate the known AP records for that
 * SSID ("Cycle APs").  Both functions are pure: they operate only on a
 * caller-supplied wifi_ap_t[] array, making them trivially host-testable.
 *
 * M1 Project
 */

#ifndef WIFI_AP_CYCLE_H_
#define WIFI_AP_CYCLE_H_

#include <stdint.h>
#include "wifi_ap_record.h"

/**
 * Count how many entries in @p list share the SSID of the entry at @p cur.
 *
 * The entry at @p cur is included in the count, so a valid index always
 * yields at least 1.  Comparison is an exact, case-sensitive match on the
 * NUL-terminated ssid field (hidden APs with an empty SSID match each other).
 *
 * @param list   Pointer to the AP array (may be NULL → returns 0).
 * @param count  Number of entries in @p list.
 * @param cur    Index of the reference entry (must be < count).
 * @return Number of entries whose ssid equals list[cur].ssid, or 0 if
 *         @p list is NULL or @p cur is out of range.
 */
uint16_t wifi_ap_ssid_count(const wifi_ap_t *list, uint16_t count, uint16_t cur);

/**
 * Return the index of the next AP that shares the SSID of list[cur].
 *
 * The search proceeds forward from @p cur (wrapping around the end of the
 * array) and returns the first other entry whose ssid matches list[cur].ssid.
 * If no other entry shares the SSID, @p cur is returned unchanged.  This lets
 * a caller cycle through the known BSSIDs of a single network.
 *
 * @param list   Pointer to the AP array (may be NULL → returns @p cur).
 * @param count  Number of entries in @p list.
 * @param cur    Index of the current entry (must be < count).
 * @return Index of the next matching entry, or @p cur when there is no other
 *         AP for the SSID or the inputs are invalid.
 */
uint16_t wifi_ap_cycle_next(const wifi_ap_t *list, uint16_t count, uint16_t cur);

#endif /* WIFI_AP_CYCLE_H_ */
