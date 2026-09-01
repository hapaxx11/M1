/* See COPYING.txt for license details. */

/**
 * @file   espnow_appmsg.h
 * @brief  Unified ESP-NOW application-message type registry.
 *
 * All M1↔M1 peer-link app protocols share a single DATA channel (CD3
 * over-the-air frame type 0x01).  The first payload byte is an application
 * message type.  Historically each sub-protocol picked its own type bytes
 * independently, which risked collisions once multiple protocols run in the
 * same session (pairing 0x01 vs Tic-Tac-Toe 0x01).
 *
 * This header assigns each sub-protocol a non-overlapping 16-value block so a
 * receiver can demultiplex an inbound DATA frame to the correct handler by
 * inspecting byte 0 alone.  New protocols MUST claim a fresh block here rather
 * than reusing an existing range.
 *
 * Pure header — no HAL/RTOS/display dependencies.  Host-testable.
 *
 * M1 Project
 */

#ifndef ESPNOW_APPMSG_H_
#define ESPNOW_APPMSG_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Application-message type blocks (byte 0 of a DATA payload)
 * =========================================================================
 *
 *   0x01..0x0F  Peer session / pairing  (espnow_peer_session.h)
 *   0x10..0x1F  File transfer           (espnow_file_transfer.h)
 *   0x20..0x2F  Short messaging         (espnow_message.h)      [Phase 2]
 *   0x30..0x3F  Remote trigger          (espnow_trigger.h)      [Phase 3]
 *   0x40..0x4F  Games (Tic-Tac-Toe, …)  (espnow_tictactoe.h)
 *   0xE0..0xEF  Encrypted envelope      (espnow_crypto.h)       [Phase 4]
 */

/** Inclusive block bounds for each sub-protocol. */
#define ESPNOW_APP_PAIR_BASE     0x01u
#define ESPNOW_APP_PAIR_LAST     0x0Fu
#define ESPNOW_APP_FT_BASE       0x10u
#define ESPNOW_APP_FT_LAST       0x1Fu
#define ESPNOW_APP_MSG_BASE      0x20u
#define ESPNOW_APP_MSG_LAST      0x2Fu
#define ESPNOW_APP_TRIGGER_BASE  0x30u
#define ESPNOW_APP_TRIGGER_LAST  0x3Fu
#define ESPNOW_APP_GAME_BASE     0x40u
#define ESPNOW_APP_GAME_LAST     0x4Fu
#define ESPNOW_APP_CRYPTO_BASE   0xE0u
#define ESPNOW_APP_CRYPTO_LAST   0xEFu

/** Logical protocol classes, resolved from a DATA payload's byte 0. */
typedef enum {
    ESPNOW_APP_CLASS_UNKNOWN = 0,
    ESPNOW_APP_CLASS_PAIR,
    ESPNOW_APP_CLASS_FILE_TRANSFER,
    ESPNOW_APP_CLASS_MESSAGE,
    ESPNOW_APP_CLASS_TRIGGER,
    ESPNOW_APP_CLASS_GAME,
    ESPNOW_APP_CLASS_CRYPTO,
} espnow_app_class_t;

/**
 * @brief  Classify a DATA payload by its first (type) byte.
 *
 * Pure function — used by the top-level peer-link demultiplexer to route an
 * inbound frame to the owning sub-protocol handler.
 *
 * @param  type_byte  First byte of the DATA payload.
 * @return The owning protocol class, or ESPNOW_APP_CLASS_UNKNOWN.
 */
static inline espnow_app_class_t espnow_app_classify(uint8_t type_byte)
{
    if (type_byte >= ESPNOW_APP_PAIR_BASE && type_byte <= ESPNOW_APP_PAIR_LAST)
        return ESPNOW_APP_CLASS_PAIR;
    if (type_byte >= ESPNOW_APP_FT_BASE && type_byte <= ESPNOW_APP_FT_LAST)
        return ESPNOW_APP_CLASS_FILE_TRANSFER;
    if (type_byte >= ESPNOW_APP_MSG_BASE && type_byte <= ESPNOW_APP_MSG_LAST)
        return ESPNOW_APP_CLASS_MESSAGE;
    if (type_byte >= ESPNOW_APP_TRIGGER_BASE &&
        type_byte <= ESPNOW_APP_TRIGGER_LAST)
        return ESPNOW_APP_CLASS_TRIGGER;
    if (type_byte >= ESPNOW_APP_GAME_BASE && type_byte <= ESPNOW_APP_GAME_LAST)
        return ESPNOW_APP_CLASS_GAME;
    if (type_byte >= ESPNOW_APP_CRYPTO_BASE &&
        type_byte <= ESPNOW_APP_CRYPTO_LAST)
        return ESPNOW_APP_CLASS_CRYPTO;
    return ESPNOW_APP_CLASS_UNKNOWN;
}

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_APPMSG_H_ */
