/* See COPYING.txt for license details. */

/**
 * @file   wifi_file_utils.h
 * @brief  Pure-logic WiFi file type validators — no HAL, RTOS, or FatFS deps.
 *
 * Extracted from m1_wifi.c following the Preferred Modularization Pattern.
 * All functions operate on filename strings and are testable on the host.
 *
 * Covers:
 *   - wifi_ascii_lower()       — single-char tolower (no locale)
 *   - wifi_ext_is_ap_cache()   — .tsv/.txt extension check
 *   - wifi_ext_is_html()       — .html/.htm extension check
 *   - wifi_ext_is_ssid_list()  — .txt/.lst extension check (beacon SSID lists)
 *
 * M1 Project
 */

#ifndef WIFI_FILE_UTILS_H_
#define WIFI_FILE_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * Character helper
 * =========================================================================*/

/**
 * Convert an ASCII character to lower-case (no locale dependency).
 *
 * @param c  Input character.
 * @return   Lower-case equivalent if c is 'A'–'Z', otherwise c unchanged.
 */
uint8_t wifi_ascii_lower(uint8_t c);

/* =========================================================================
 * Extension checkers
 * =========================================================================*/

/**
 * Return true if the filename has a .tsv or .txt extension (case-insensitive).
 *
 * @param name  Filename string (may be NULL — returns false).
 */
bool wifi_ext_is_ap_cache(const char *name);

/**
 * Return true if the filename has a .html or .htm extension (case-insensitive).
 *
 * @param name  Filename string (may be NULL — returns false).
 */
bool wifi_ext_is_html(const char *name);

/**
 * Return true if the filename has a .txt or .lst extension (case-insensitive).
 *
 * @param name  Filename string (may be NULL — returns false).
 */
bool wifi_ext_is_ssid_list(const char *name);

#endif /* WIFI_FILE_UTILS_H_ */
