/* See COPYING.txt for license details. */

/**
 * @file   wifi_selection.h
 * @brief  Parameterized AP/STA selection counters — no HAL, RTOS, or display deps.
 *
 * Extracted from m1_wifi.c following the Preferred Modularization Pattern.
 * Both functions are pure: they operate only on caller-supplied arrays and
 * contain no global-state references, making them trivially host-testable.
 *
 * M1 Project
 */

#ifndef WIFI_SELECTION_H_
#define WIFI_SELECTION_H_

#include <stdint.h>
#include "wifi_ap_record.h"
#include "wifi_sta_record.h"

/**
 * Count the number of selected entries in an AP list.
 *
 * @param list   Pointer to the AP array (may be NULL → returns 0).
 * @param count  Number of entries in @p list.
 * @return Number of entries with selected == true.
 */
uint16_t wifi_selected_ap_count(const wifi_ap_t *list, uint16_t count);

/**
 * Count the number of selected entries in a STA list.
 *
 * @param list   Pointer to the STA array (may be NULL → returns 0).
 * @param count  Number of entries in @p list.
 * @return Number of entries with selected == true.
 */
uint16_t wifi_selected_sta_count(const wifi_sta_t *list, uint16_t count);

#endif /* WIFI_SELECTION_H_ */
