/* See COPYING.txt for license details. */

/*
 * m1_json_mini.h
 *
 * Minimal JSON parser for GitHub API responses.
 * Not a full JSON parser — only handles the subset needed to extract
 * release metadata and asset download URLs from GitHub REST API.
 *
 * M1 Project
 */

#ifndef M1_JSON_MINI_H_
#define M1_JSON_MINI_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Find a JSON string value by key in a JSON object string.
 * Searches for "key":"value" and copies value into out_buf.
 * Returns true if found, false otherwise.
 * Handles escaped quotes within values.
 */
bool json_get_string(const char *json, const char *key, char *out_buf, uint16_t buf_size);

/*
 * Find a JSON integer value by key in a JSON object string.
 * Searches for "key":123 and writes integer to *out_val.
 * Returns true if found, false otherwise.
 */
bool json_get_int(const char *json, const char *key, int32_t *out_val);

/*
 * Find a JSON boolean value by key.
 * Searches for "key":true or "key":false.
 * Returns true if found, false otherwise.
 */
bool json_get_bool(const char *json, const char *key, bool *out_val);

/*
 * Advance past the current JSON array element.
 * Given a pointer inside a JSON array (after '[' or after ','),
 * skips the current element (object, string, number, etc.)
 * and returns a pointer to the next element or NULL if end of array.
 */
const char *json_array_next(const char *pos);

/*
 * Find the start of a JSON array value for the given key.
 * Searches for "key":[ and returns pointer to first element after '['.
 * Returns NULL if not found.
 */
const char *json_find_array(const char *json, const char *key);

/*
 * Find the start of a JSON object value for the given key.
 * Searches for "key":{ and returns pointer to the '{'.
 * Returns NULL if not found.
 */
const char *json_find_object(const char *json, const char *key);

/*
 * Get the extent of the current JSON object starting at pos.
 * pos must point to '{'. Returns pointer past the closing '}'.
 * Returns NULL on error (unbalanced braces).
 */
const char *json_object_end(const char *pos);

#endif /* M1_JSON_MINI_H_ */
