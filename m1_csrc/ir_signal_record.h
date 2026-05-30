/* See COPYING.txt for license details. */

/**
 * @file   ir_signal_record.h
 * @brief  Pure-logic IR signal record helpers — no HAL, RTOS, FatFS, or display deps.
 *
 * Extracted from m1_ir_universal.c following the Preferred Modularization Pattern.
 * All functions are testable on the host via the stub-based extraction pattern.
 *
 * Covers:
 *   - ir_map_flipper_protocol()   — Flipper protocol name → IRMP protocol ID
 *   - ir_is_ir_file()             — check if a filename has the ".ir" extension
 *   - ir_path_append()            — append a path component to a base path
 *   - ir_path_go_up()             — navigate one level up in a path string
 *   - ir_str_contains_icase()     — case-insensitive substring search
 *
 * M1 Project
 */

#ifndef IR_SIGNAL_RECORD_H_
#define IR_SIGNAL_RECORD_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "irmp.h"

/* =========================================================================
 * Protocol mapping
 * =========================================================================*/

/**
 * Map a Flipper Zero protocol name string to the corresponding IRMP protocol ID.
 *
 * Covers all protocol names used in Flipper IRDB .ir files.
 * Unknown names return IRMP_UNKNOWN_PROTOCOL (0).
 *
 * @param name  Protocol name string (e.g. "NEC", "RC5", "Samsung48").
 * @return IRMP_*_PROTOCOL constant, or 0 for unknown/NULL input.
 */
uint8_t ir_map_flipper_protocol(const char *name);

/* =========================================================================
 * File-name helpers
 * =========================================================================*/

/**
 * Return true if fname ends with the ".ir" extension.
 *
 * @param fname  File name string (may be NULL — returns false).
 */
bool ir_is_ir_file(const char *fname);

/* =========================================================================
 * Path manipulation
 * =========================================================================*/

/**
 * Append a path component to a base path, inserting "/" if needed.
 *
 * Always null-terminates base within base_size bytes and never writes
 * beyond base_size.
 *
 * @param base       Null-terminated base path buffer (modified in place).
 * @param item       Component to append.
 * @param base_size  Total size of the base buffer in bytes.
 */
void ir_path_append(char *base, const char *item, size_t base_size);

/**
 * Navigate one level up in a path by truncating at the last '/'.
 * A trailing slash is stripped before the search so that
 * "0:/IR/Sony/" becomes "0:/IR".
 * If no slash is found, the path is cleared to an empty string.
 *
 * @param path  Null-terminated path buffer (modified in place).
 */
void ir_path_go_up(char *path);

/* =========================================================================
 * String helpers
 * =========================================================================*/

/**
 * Return true if needle is a case-insensitive substring of haystack.
 *
 * - NULL or empty needle always returns true.
 * - NULL haystack with non-empty needle returns false.
 * - Only ASCII letters are case-folded; all other bytes compare literally.
 *
 * @param haystack  String to search within (may be NULL).
 * @param needle    Substring to search for (may be NULL or empty).
 */
bool ir_str_contains_icase(const char *haystack, const char *needle);

#endif /* IR_SIGNAL_RECORD_H_ */
