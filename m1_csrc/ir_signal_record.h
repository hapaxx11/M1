/* See COPYING.txt for license details. */

/**
 * @file   ir_signal_record.h
 * @brief  Pure-logic IR signal record helpers — no HAL, RTOS, FatFS, or display deps.
 *
 * Extracted from m1_ir_universal.c following the Preferred Modularization Pattern.
 * All functions are testable on the host via the stub-based extraction pattern.
 *
 * Covers:
 *   - ir_universal_cmd_t struct   — parsed IR command record (moved from m1_ir_universal.h)
 *   - ir_map_flipper_protocol()   — Flipper protocol name → IRMP protocol ID
 *   - ir_is_ir_file()             — check if a filename has the ".ir" extension
 *   - ir_path_append()            — append a path component to a base path
 *   - ir_path_go_up()             — navigate one level up in a path string
 *   - ir_str_contains_icase()     — case-insensitive substring search
 *   - ir_parse_hex_bytes()        — parse "AA BB CC" hex string → byte array
 *   - ir_block_reader_t           — vtable for line-by-line KV readers (FatFS or string)
 *   - ir_cmd_parse()              — parse one IR signal block via any ir_block_reader_t
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
 * IR command record (moved from m1_ir_universal.h in Phase G)
 * =========================================================================*/

/** Maximum length of an IR signal name, including the NUL terminator. */
#define IR_CMD_NAME_MAX_LEN  32

/**
 * @brief  Parsed IR signal command record.
 *
 * Populated by ir_cmd_parse().  For parsed signals (is_raw == false):
 *   protocol, address, command, flags are valid.
 * For raw signals (is_raw == true):
 *   raw_freq and raw_count are valid.
 */
typedef struct {
	char     name[IR_CMD_NAME_MAX_LEN];
	uint8_t  protocol;   /**< IRMP_*_PROTOCOL ID */
	uint16_t address;
	uint16_t command;
	uint8_t  flags;
	bool     is_raw;
	uint32_t raw_freq;   /**< Raw signal carrier frequency in Hz */
	uint16_t raw_count;  /**< Number of raw timing samples */
	bool     valid;
} ir_universal_cmd_t;

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

/* =========================================================================
 * Hex byte parser
 * =========================================================================*/

/**
 * Parse a space-separated hexadecimal byte string into a byte array.
 *
 * Input format: "07 00 00 00" or "AB CD EF" (pairs of hex digits, space-separated).
 * Parsing stops at the first malformed byte pair or when out is full.
 *
 * @param str      Input hex string (may be NULL — returns 0).
 * @param out      Output byte buffer.
 * @param max_len  Maximum number of bytes to write into out.
 * @return Number of bytes written.
 */
uint8_t ir_parse_hex_bytes(const char *str, uint8_t *out, uint8_t max_len);

/* =========================================================================
 * KV-reader vtable + block parser
 * =========================================================================*/

/**
 * @brief  Minimal key-value reader vtable for ir_cmd_parse().
 *
 * Abstracts the line-by-line reader so ir_cmd_parse() can operate on any
 * source (FatFS file, in-memory string, UART stream, …) without a direct
 * dependency on flipper_file_t or the FatFS library.
 *
 * Contract:
 *   1. Call next() to advance.  Returns false at EOF — stop iterating.
 *   2. After a successful next(), call is_sep() to detect a block separator.
 *      If true, the current signal block has ended; do not call parse_kv().
 *   3. If not a separator, call parse_kv().  Returns true if the line is a
 *      well-formed "key: value" pair.
 *   4. After a successful parse_kv(), get_key() / get_value() return the
 *      parsed strings.  These pointers are valid until the next next() call.
 *
 * All function pointers must be non-NULL when passed to ir_cmd_parse().
 */
typedef struct {
	bool        (*next)(void *ctx);      /**< Advance; false = EOF */
	bool        (*is_sep)(void *ctx);    /**< Current line is a separator */
	bool        (*parse_kv)(void *ctx);  /**< Parse current line as "key: value" */
	const char *(*get_key)(void *ctx);   /**< Key after successful parse_kv() */
	const char *(*get_value)(void *ctx); /**< Value after successful parse_kv() */
} ir_block_reader_t;

/**
 * @brief  Parse one IR signal block using an ir_block_reader_t.
 *
 * Reads key-value pairs from ops until a separator or EOF is encountered.
 * Populates *cmd and returns true on success (valid parsed or raw signal).
 *
 * This is the FatFS-free extraction of the formerly-static
 * parse_ir_signal_block() from m1_ir_universal.c (Phase G).
 *
 * @param ops  Non-NULL reader vtable.
 * @param ctx  Opaque context pointer forwarded to every vtable call.
 * @param cmd  Output command record; zeroed by this function on entry.
 * @return true if a valid signal was parsed, false otherwise.
 */
bool ir_cmd_parse(const ir_block_reader_t *ops, void *ctx, ir_universal_cmd_t *cmd);

#endif /* IR_SIGNAL_RECORD_H_ */
