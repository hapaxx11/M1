/* See COPYING.txt for license details. */

/**
 * @file   wifi_deauth_cmd.c
 * @brief  Pure-logic deauth multi-target command builder.
 *
 * Pure-logic module extracted from m1_wifi.c.
 * Zero dependencies on HAL, RTOS, or display subsystems.
 *
 * M1 Project
 */

#include <string.h>
#include "wifi_deauth_cmd.h"
#include "wifi_ap_record.h"   /* wifi_mac_is_nonzero() */
#include "wifi_selection.h"   /* wifi_selected_ap_count(), wifi_selected_sta_count() */

uint8_t wifi_deauth_add_target(m1_cmd_t *cmd, uint8_t count, uint8_t mode,
	uint8_t channel, const uint8_t bssid[6], const uint8_t sta[6])
{
	uint8_t off;

	if (count >= DEAUTH_MULTI_MAX_TARGETS || !bssid || !wifi_mac_is_nonzero(bssid) || channel == 0)
	{
		return count;
	}

	off = 1 + count * DEAUTH_MULTI_TARGET_BYTES;
	cmd->payload[off] = mode;
	cmd->payload[off + 1] = channel;
	memcpy(&cmd->payload[off + 2], bssid, 6);
	if (sta)
	{
		memcpy(&cmd->payload[off + 8], sta, 6);
	}
	else
	{
		memset(&cmd->payload[off + 8], 0, 6);
	}

	count++;
	cmd->payload[0] = count;
	cmd->payload_len = 1 + count * DEAUTH_MULTI_TARGET_BYTES;
	return count;
}

uint8_t wifi_build_selected_deauth_cmd(m1_cmd_t *cmd,
	const wifi_ap_t *ap_list, uint16_t ap_count,
	const wifi_sta_t *sta_list, uint16_t sta_count,
	uint8_t *ap_targets, uint8_t *sta_targets, uint16_t *selected_total)
{
	uint8_t count = 0;
	uint8_t ap_count_used = 0;
	uint8_t sta_count_used = 0;

	memset(cmd, 0, sizeof(*cmd));
	cmd->magic = M1_CMD_MAGIC;
	cmd->cmd_id = CMD_DEAUTH_MULTI;
	cmd->payload_len = 1;

	if (ap_list)
	{
		for (uint16_t i = 0; i < ap_count && count < DEAUTH_MULTI_MAX_TARGETS; i++)
		{
			if (!ap_list[i].selected) continue;
			uint8_t before = count;
			count = wifi_deauth_add_target(cmd, count, 0, ap_list[i].channel,
				ap_list[i].bssid, NULL);
			if (count != before) ap_count_used++;
		}
	}

	if (sta_list)
	{
		for (uint16_t i = 0; i < sta_count && count < DEAUTH_MULTI_MAX_TARGETS; i++)
		{
			if (!sta_list[i].selected) continue;
			uint8_t before = count;
			count = wifi_deauth_add_target(cmd, count, 1, sta_list[i].channel,
				sta_list[i].bssid, sta_list[i].mac);
			if (count != before) sta_count_used++;
		}
	}

	if (ap_targets) *ap_targets = ap_count_used;
	if (sta_targets) *sta_targets = sta_count_used;
	if (selected_total) *selected_total = wifi_selected_ap_count(ap_list, ap_count)
	                                    + wifi_selected_sta_count(sta_list, sta_count);
	return count;
}
