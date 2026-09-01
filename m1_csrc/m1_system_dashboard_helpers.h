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

/**
 * Split a formatted RPC call diagnostic line (see
 * m1_esp32_rpc_call_diag_format()) into a base part and an optional
 * trailing wall-clock "tNs" suffix (issue #719 Phase 6).
 *
 * At the dashboard's small font the full line (e.g.
 * "op0103 no-reply st253 r0 p0 t10s") runs past the right edge of the
 * 128px display, making the "tNs" detail unreadable. This splits the
 * suffix off so the caller can draw it on its own line instead.
 *
 * @param line       Full diagnostic line to split.
 * @param base       Destination for everything before the suffix (or the
 *                   whole line, if no "tNs" suffix is present).
 * @param base_len   Capacity of @p base in bytes.
 * @param suffix     Destination for the "tNs" suffix, or an empty string
 *                   if @p line has none. May be NULL if not needed.
 * @param suffix_len Capacity of @p suffix in bytes (ignored if @p suffix
 *                   is NULL).
 */
void        dashboard_split_rpc_wallclock_suffix(const char *line,
                                                 char *base, size_t base_len,
                                                 char *suffix, size_t suffix_len);

#endif /* M1_SYSTEM_DASHBOARD_HELPERS_H_ */
