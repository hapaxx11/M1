/* See COPYING.txt for license details. */

/**
 * @file   wifi_selection.c
 * @brief  Parameterized AP/STA selection counters.
 *
 * Pure-logic module extracted from m1_wifi.c.
 * Zero dependencies on HAL, RTOS, or display subsystems.
 *
 * M1 Project
 */

#include "wifi_selection.h"

uint16_t wifi_selected_ap_count(const wifi_ap_t *list, uint16_t count)
{
	uint16_t n = 0;
	if (!list) return 0;
	for (uint16_t i = 0; i < count; i++)
	{
		if (list[i].selected) n++;
	}
	return n;
}

uint16_t wifi_selected_sta_count(const wifi_sta_t *list, uint16_t count)
{
	uint16_t n = 0;
	if (!list) return 0;
	for (uint16_t i = 0; i < count; i++)
	{
		if (list[i].selected) n++;
	}
	return n;
}
