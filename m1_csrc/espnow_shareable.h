/* See COPYING.txt for license details. */

/**
 * @file   espnow_shareable.h
 * @brief  Capture-sharing helpers for the ESP-NOW sender side — pure logic.
 *
 * Phase 1 of the peer link finishes the *sender* half of the existing file
 * transfer protocol (espnow_file_transfer.c already implements the sender
 * FSM).  The remaining decisions are pure and belong here so they can be
 * host-tested independently of the file-browser scene that will call them:
 *
 *   - which saved-item file types may be shared to a peer,
 *   - extracting the transfer filename (basename) from a full SD path,
 *   - validating an inbound filename before it is written, and
 *   - building the receive save path under /ESPNOW.
 *
 * No HAL, RTOS, FatFS, or display dependencies.  Host-testable.
 *
 * M1 Project
 */

#ifndef ESPNOW_SHAREABLE_H_
#define ESPNOW_SHAREABLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Directory that received captures are streamed into. */
#define ESPNOW_SHARE_RECV_DIR   "/ESPNOW"

/** Shareable saved-item kinds (mirrors the M1 module set). */
typedef enum {
    ESPNOW_SHARE_KIND_UNKNOWN = 0,
    ESPNOW_SHARE_KIND_SUBGHZ,   /**< .sub */
    ESPNOW_SHARE_KIND_NFC,      /**< .nfc */
    ESPNOW_SHARE_KIND_RFID,     /**< .rfid */
    ESPNOW_SHARE_KIND_IR,       /**< .ir  */
} espnow_share_kind_t;

/**
 * @brief  Classify a file path/name by extension into a shareable kind.
 *
 * Case-insensitive on the extension.  Returns ESPNOW_SHARE_KIND_UNKNOWN for
 * any type that must not be shared (no extension, unknown extension).
 *
 * @param  name  File path or bare name.
 * @return The shareable kind, or ESPNOW_SHARE_KIND_UNKNOWN.
 */
espnow_share_kind_t espnow_share_classify(const char *name);

/**
 * @brief  True if @p name is a shareable saved-item type.
 */
bool espnow_share_is_shareable(const char *name);

/**
 * @brief  Return the saved-capture browser root for a shareable kind.
 *
 * The returned strings use the existing storage browser volume prefix.
 * Returns NULL for UNKNOWN.
 */
const char *espnow_share_kind_dir(espnow_share_kind_t kind);

/**
 * @brief  Extract the basename (final path component) from a full path.
 *
 * "/SUBGHZ/x/foo.sub" → "foo.sub".  A path with no separator is copied as-is.
 * The output is always null-terminated (possibly truncated to out_cap-1).
 *
 * @param  path     Source path.
 * @param  out      Destination buffer.
 * @param  out_cap  Capacity of @p out (must be ≥ 1).
 * @return true on success, false on a NULL/zero-capacity argument.
 */
bool espnow_share_basename(const char *path, char *out, size_t out_cap);

/**
 * @brief  Validate a filename received in a FILE_OFFER before it is written.
 *
 * Rejects names that are empty, too long (> max_len), contain a path separator
 * ('/' or '\\'), contain "..", or begin with a dot.  This prevents a malicious
 * peer from escaping the receive directory or writing hidden/parent files.
 *
 * @param  name     Candidate filename (as received).
 * @param  max_len  Maximum permitted length (e.g. ESPNOW_FT_FILENAME_MAX).
 * @return true if the name is safe to use as a leaf filename.
 */
bool espnow_share_name_is_safe(const char *name, size_t max_len);

/**
 * @brief  Build the receive save path "/ESPNOW/<name>" for a validated name.
 *
 * The caller must have already validated @p name with
 * espnow_share_name_is_safe().  Output is null-terminated.
 *
 * @return true on success, false if the result would not fit in @p out_cap or
 *         on a NULL argument.
 */
bool espnow_share_recv_path(const char *name, char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_SHAREABLE_H_ */
