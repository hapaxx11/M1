/* See COPYING.txt for license details. */

/**
 * @file   wifi_status_msg.h
 * @brief  Pure-logic WiFi status message model — no HAL, RTOS, or display deps.
 *
 * Extracted from m1_wifi.c following the Preferred Modularization Pattern.
 * Models the "show message + HAL_Delay(N)" pattern as data that a scene
 * event loop can poll for expiry, enabling async conversion.
 *
 * Covers:
 *   - wifi_status_msg_t      — message struct (title, lines, display duration)
 *   - wifi_status_msg_set()  — populate a message (validates/truncates)
 *   - wifi_status_msg_clear() — reset a message to inactive
 *   - wifi_status_msg_active() — check if a message is currently set
 *   - wifi_status_msg_expired() — pure tick-comparison for expiry
 *
 * M1 Project
 */

#ifndef WIFI_STATUS_MSG_H_
#define WIFI_STATUS_MSG_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * Constants
 * =========================================================================*/

/** Maximum title length including NUL terminator. */
#define WIFI_STATUS_MSG_TITLE_MAX  32

/** Maximum line length including NUL terminator. */
#define WIFI_STATUS_MSG_LINE_MAX   40

/** Default display duration in milliseconds (matches wifi_show_message). */
#define WIFI_STATUS_MSG_DEFAULT_MS 1800

/* =========================================================================
 * Status message struct
 * =========================================================================*/

/**
 * Pure-data model for a timed status message.
 *
 * Replaces the blocking pattern:
 *   wifi_show_message(title, line1, line2);  // draws + HAL_Delay(1800)
 *
 * With the async-friendly pattern:
 *   wifi_status_msg_set(&msg, title, line1, line2, 1800);
 *   msg.start_ms = HAL_GetTick();   // caller sets this from system tick
 *   // scene loop: draw msg fields, check wifi_status_msg_expired()
 */
typedef struct {
    char     title[WIFI_STATUS_MSG_TITLE_MAX];  /**< Message title. */
    char     line1[WIFI_STATUS_MSG_LINE_MAX];   /**< First detail line. */
    char     line2[WIFI_STATUS_MSG_LINE_MAX];   /**< Second detail line. */
    uint32_t display_ms;  /**< How long to show (ms). 0 = inactive. */
    uint32_t start_ms;    /**< System tick when message was shown.
                               Caller sets this from HAL_GetTick(). */
    bool     active;      /**< True if a message is currently being shown. */
} wifi_status_msg_t;

/* =========================================================================
 * API
 * =========================================================================*/

/**
 * Populate a status message.
 *
 * Strings are safely copied and NUL-terminated.  NULL pointers are treated
 * as empty strings.  The 'active' flag is set to true and 'start_ms' is
 * left unchanged (caller must set it from the system tick source).
 *
 * @param msg         Destination message struct.
 * @param title       Title string (may be NULL).
 * @param line1       First detail line (may be NULL).
 * @param line2       Second detail line (may be NULL).
 * @param display_ms  Display duration in ms (0 treated as
 *                    WIFI_STATUS_MSG_DEFAULT_MS).
 */
void wifi_status_msg_set(wifi_status_msg_t *msg,
                         const char *title,
                         const char *line1,
                         const char *line2,
                         uint32_t display_ms);

/**
 * Clear a status message to inactive state.
 *
 * @param msg  Message struct to clear.
 */
void wifi_status_msg_clear(wifi_status_msg_t *msg);

/**
 * Return true if a message is currently active (set and not cleared).
 *
 * @param msg  Message struct to check (may be NULL — returns false).
 */
bool wifi_status_msg_active(const wifi_status_msg_t *msg);

/**
 * Check if a timed status message has expired.
 *
 * Pure tick-comparison helper.  Handles uint32_t wraparound at ~49.7 days.
 *
 * @param msg    Message struct.
 * @param now_ms Current system tick in ms (e.g. from HAL_GetTick()).
 * @return true if the message is active and (now_ms - start_ms) >= display_ms,
 *         or if the message is not active.
 */
bool wifi_status_msg_expired(const wifi_status_msg_t *msg, uint32_t now_ms);

#endif /* WIFI_STATUS_MSG_H_ */
