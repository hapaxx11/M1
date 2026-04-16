/* See COPYING.txt for license details. */

/**
 * @file   m1_system_dashboard_helpers.h
 * @brief  Pure-logic helpers for the system dashboard.
 *
 * Hardware-independent: compiled into both firmware and host-side unit tests.
 */

#ifndef M1_SYSTEM_DASHBOARD_HELPERS_H_
#define M1_SYSTEM_DASHBOARD_HELPERS_H_

#include <stdint.h>
#include <stddef.h>

#include "m1_sdcard.h"

void        dashboard_format_uptime(uint32_t uptime_ms, char *out, size_t out_len);
const char *dashboard_sd_status_text(S_M1_SDCard_Access_Status status);

#endif /* M1_SYSTEM_DASHBOARD_HELPERS_H_ */
