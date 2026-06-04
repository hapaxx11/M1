/* See COPYING.txt for license details. */

/**
 * @file   wifi_mac_utils.h
 * @brief  Pure-logic MAC address utilities — no HAL, RTOS, or display deps.
 *
 * Extracted from m1_wifi.c following the Preferred Modularization Pattern.
 * Complements the BSSID helpers in wifi_ap_record.h with general-purpose
 * MAC address operations.
 *
 * Covers:
 *   - wifi_mac_is_zero()   — all-zero MAC check
 *   - wifi_mac_format()    — 6-byte MAC → "XX:XX:XX:XX:XX:XX" string
 *   - wifi_mac_match()     — byte-exact MAC comparison
 *
 * M1 Project
 */

#ifndef WIFI_MAC_UTILS_H_
#define WIFI_MAC_UTILS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * MAC address helpers
 * =========================================================================*/

/**
 * Return true if all 6 bytes of a MAC address are zero.
 *
 * @param mac  6-byte MAC/BSSID array (must not be NULL).
 */
bool wifi_mac_is_zero(const uint8_t mac[6]);

/**
 * Format a 6-byte MAC address as "XX:XX:XX:XX:XX:XX".
 *
 * @param mac      6-byte MAC/BSSID array (must not be NULL).
 * @param out      Output buffer.
 * @param out_len  Size of output buffer in bytes (must be >= 18 for full
 *                 output: 17 chars + NUL).  If smaller, the output is
 *                 safely truncated.
 */
void wifi_mac_format(const uint8_t mac[6], char *out, size_t out_len);

/**
 * Return true if two 6-byte MAC addresses are byte-identical.
 *
 * @param a  First MAC (must not be NULL).
 * @param b  Second MAC (must not be NULL).
 */
bool wifi_mac_match(const uint8_t *a, const uint8_t *b);

#endif /* WIFI_MAC_UTILS_H_ */
