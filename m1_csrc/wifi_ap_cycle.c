/* See COPYING.txt for license details. */

/**
 * @file   wifi_ap_cycle.c
 * @brief  Pure-logic AP-cycling helpers (see wifi_ap_cycle.h).
 *
 * M1 Project
 */

#include "wifi_ap_cycle.h"
#include <string.h>

uint16_t wifi_ap_ssid_count(const wifi_ap_t *list, uint16_t count, uint16_t cur)
{
	uint16_t i;
	uint16_t matches = 0;

	if (list == NULL || cur >= count)
		return 0;

	for (i = 0; i < count; i++)
	{
		if (strncmp(list[i].ssid, list[cur].ssid, sizeof(list[cur].ssid)) == 0)
			matches++;
	}

	return matches;
}

uint16_t wifi_ap_cycle_next(const wifi_ap_t *list, uint16_t count, uint16_t cur)
{
	uint16_t step;

	if (list == NULL || cur >= count)
		return cur;

	/* Walk forward from cur+1, wrapping, and return the first other entry
	 * that shares the current SSID.  Fall through to cur if none matches. */
	for (step = 1; step < count; step++)
	{
		uint16_t j = (uint16_t)((cur + step) % count);
		if (strncmp(list[j].ssid, list[cur].ssid, sizeof(list[cur].ssid)) == 0)
			return j;
	}

	return cur;
}
