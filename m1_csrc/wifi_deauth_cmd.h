/* See COPYING.txt for license details. */

/**
 * @file   wifi_deauth_cmd.h
 * @brief  Pure-logic deauth multi-target command builder — no HAL, RTOS, or display deps.
 *
 * Extracted from m1_wifi.c following the Preferred Modularization Pattern.
 * Builds a CMD_DEAUTH_MULTI payload from parameterized AP and STA lists.
 * The functions operate only on caller-supplied data and are trivially
 * host-testable.
 *
 * Protocol constants (DEAUTH_MULTI_MAX_TARGETS, DEAUTH_MULTI_TARGET_BYTES)
 * are defined here so callers can size buffers without depending on m1_wifi.c
 * internals.
 *
 * M1 Project
 */

#ifndef WIFI_DEAUTH_CMD_H_
#define WIFI_DEAUTH_CMD_H_

#include <stdint.h>
#include "m1_esp32_cmd.h"
#include "wifi_ap_record.h"
#include "wifi_sta_record.h"

/** Maximum number of targets packed into one CMD_DEAUTH_MULTI payload. */
#define DEAUTH_MULTI_MAX_TARGETS  4

/** Wire bytes consumed per target in the CMD_DEAUTH_MULTI payload. */
#define DEAUTH_MULTI_TARGET_BYTES 14

/**
 * Append one target to a CMD_DEAUTH_MULTI command payload.
 *
 * The payload wire format is:
 *   payload[0]          = target count (updated on each successful append)
 *   payload[1 + n*14]   = mode  (0 = AP-only deauth, 1 = directed STA deauth)
 *   payload[2 + n*14]   = channel
 *   payload[3 + n*14]   = bssid[0..5]
 *   payload[9 + n*14]   = sta[0..5]  (zeros when mode == 0)
 *
 * No-op conditions (target not appended, count returned unchanged):
 *   - count has already reached DEAUTH_MULTI_MAX_TARGETS
 *   - bssid is NULL or all-zero
 *   - channel is 0
 *
 * @param cmd      Command buffer to fill (magic/cmd_id must be pre-set by caller).
 * @param count    Current target count (before this call).
 * @param mode     0 = AP deauth (broadcast), 1 = directed STA deauth.
 * @param channel  IEEE 802.11 channel number.
 * @param bssid    6-byte BSSID of the AP (required).
 * @param sta      6-byte STA MAC (may be NULL for mode 0; zeros written).
 * @return Updated target count (count+1 on success, count on no-op).
 */
uint8_t wifi_deauth_add_target(m1_cmd_t       *cmd,
                                uint8_t         count,
                                uint8_t         mode,
                                uint8_t         channel,
                                const uint8_t   bssid[6],
                                const uint8_t   sta[6]);

/**
 * Build a complete CMD_DEAUTH_MULTI command from selected AP and STA entries.
 *
 * Iterates @p ap_list and @p sta_list, appending every entry with
 * selected == true until DEAUTH_MULTI_MAX_TARGETS is reached.
 * AP entries are appended first (mode 0), then STA entries (mode 1).
 *
 * @param cmd           Output command buffer (zeroed and fully initialised).
 * @param ap_list       AP array (may be NULL → skipped).
 * @param ap_count      Number of entries in @p ap_list.
 * @param sta_list      STA array (may be NULL → skipped).
 * @param sta_count     Number of entries in @p sta_list.
 * @param ap_targets    Out: number of AP entries successfully appended (may be NULL).
 * @param sta_targets   Out: number of STA entries successfully appended (may be NULL).
 * @param selected_total Out: total selected entries across both lists (may be NULL).
 * @return Total target count written into the command (0 if nothing was selected).
 */
uint8_t wifi_build_selected_deauth_cmd(m1_cmd_t         *cmd,
                                        const wifi_ap_t  *ap_list,
                                        uint16_t          ap_count,
                                        const wifi_sta_t *sta_list,
                                        uint16_t          sta_count,
                                        uint8_t          *ap_targets,
                                        uint8_t          *sta_targets,
                                        uint16_t         *selected_total);

#endif /* WIFI_DEAUTH_CMD_H_ */
