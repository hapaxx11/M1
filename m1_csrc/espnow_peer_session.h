/* See COPYING.txt for license details. */

/**
 * @file   espnow_peer_session.h
 * @brief  ESP-NOW peer discovery & pairing state machine — pure logic.
 *
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * The state machine models the STM32-side peer link lifecycle:
 *   IDLE → SCANNING → PEER_FOUND → PAIR_SENT → PAIRED → IDLE
 *                                             → PAIR_REJECTED → SCANNING
 *
 * Transitions are driven by explicit event calls; the caller (scene code)
 * is responsible for polling/timing and invoking the appropriate event.
 *
 * M1 Project
 */

#ifndef ESPNOW_PEER_SESSION_H_
#define ESPNOW_PEER_SESSION_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Constants
 * =========================================================================*/

/** Maximum device name length (matches CD3 ENL_NAME_MAX). */
#define ESPNOW_NAME_MAX         23

/** Maximum tracked peers (matches CD3 ENL_MAX_PEERS). */
#define ESPNOW_MAX_PEERS        16

/** MAC address length. */
#define ESPNOW_MAC_LEN          6

/** ESP-NOW application-layer message types (over CD3 DATA frames). */
typedef enum {
    ESPNOW_MSG_PAIR_REQUEST  = 0x01,
    ESPNOW_MSG_PAIR_ACCEPT   = 0x02,
    ESPNOW_MSG_PAIR_REJECT   = 0x03,
    ESPNOW_MSG_DISCONNECT    = 0x04,
} espnow_msg_type_t;

/* =========================================================================
 * State machine states
 * =========================================================================*/

typedef enum {
    ESPNOW_STATE_IDLE = 0,      /**< Not active */
    ESPNOW_STATE_SCANNING,      /**< Broadcasting ANNOUNCE, polling peers */
    ESPNOW_STATE_PEER_FOUND,    /**< User selected a peer from scan list */
    ESPNOW_STATE_PAIR_SENT,     /**< PAIR_REQUEST sent, awaiting response */
    ESPNOW_STATE_PAIRED,        /**< Mutually paired — ready for app layer */
    ESPNOW_STATE_PAIR_REJECTED, /**< Peer rejected pairing */
} espnow_session_state_t;

/* =========================================================================
 * Peer descriptor
 * =========================================================================*/

typedef struct {
    uint8_t  mac[ESPNOW_MAC_LEN];
    int8_t   rssi;
    uint8_t  channel;
    char     name[ESPNOW_NAME_MAX + 1];  /**< Null-terminated */
} espnow_peer_info_t;

/* =========================================================================
 * Session context
 * =========================================================================*/

typedef struct {
    espnow_session_state_t state;

    /** Our device name (null-terminated). */
    char our_name[ESPNOW_NAME_MAX + 1];

    /** Our MAC address. */
    uint8_t our_mac[ESPNOW_MAC_LEN];

    /** Channel we are operating on. */
    uint8_t our_channel;

    /** Discovered peers table. */
    espnow_peer_info_t peers[ESPNOW_MAX_PEERS];
    uint8_t peer_count;

    /** Index into peers[] of the selected/paired peer. */
    uint8_t selected_peer_idx;

    /** 4-digit visual confirmation code (valid in PAIRED state). */
    uint16_t confirm_code;
} espnow_session_t;

/* =========================================================================
 * API
 * =========================================================================*/

/**
 * @brief  Initialise a session context to IDLE state.
 * @param  s         Session context to initialise.
 * @param  name      Our device name (truncated to ESPNOW_NAME_MAX).
 * @param  mac       Our 6-byte MAC address.
 * @param  channel   WiFi channel to operate on (1-14).
 */
void espnow_session_init(espnow_session_t *s, const char *name,
                          const uint8_t mac[ESPNOW_MAC_LEN], uint8_t channel);

/**
 * @brief  Transition from IDLE → SCANNING.
 * @return true on success, false if not in IDLE state.
 */
bool espnow_session_start_scan(espnow_session_t *s);

/**
 * @brief  Update the peer table from a poll result.
 *
 * Merges new peers and updates RSSI for existing ones (matched by MAC).
 * Peers not present in the new list are retained (no eviction in v1).
 *
 * @param  s         Session context.
 * @param  peers     Array of discovered peers.
 * @param  count     Number of peers in the array (clamped to ESPNOW_MAX_PEERS).
 */
void espnow_session_update_peers(espnow_session_t *s,
                                  const espnow_peer_info_t *peers,
                                  uint8_t count);

/**
 * @brief  User selects a peer from the scan list.
 *         Transitions SCANNING → PEER_FOUND.
 * @param  s     Session context.
 * @param  idx   Index into the peers[] table.
 * @return true on success.
 */
bool espnow_session_select_peer(espnow_session_t *s, uint8_t idx);

/**
 * @brief  Record that a PAIR_REQUEST was sent to the selected peer.
 *         Transitions PEER_FOUND → PAIR_SENT.
 * @return true on success.
 */
bool espnow_session_pair_request_sent(espnow_session_t *s);

/**
 * @brief  Process a received PAIR_ACCEPT from the selected peer.
 *         Transitions PAIR_SENT → PAIRED and computes confirm_code.
 * @return true on success.
 */
bool espnow_session_pair_accepted(espnow_session_t *s);

/**
 * @brief  Process a received PAIR_REJECT from the selected peer.
 *         Transitions PAIR_SENT → PAIR_REJECTED.
 * @return true on success.
 */
bool espnow_session_pair_rejected(espnow_session_t *s);

/**
 * @brief  Acknowledge rejection and return to scanning.
 *         Transitions PAIR_REJECTED → SCANNING.
 * @return true on success.
 */
bool espnow_session_ack_rejection(espnow_session_t *s);

/**
 * @brief  Disconnect / stop the session → IDLE.
 *         Valid from any state.
 */
void espnow_session_stop(espnow_session_t *s);

/**
 * @brief  Compute the 4-digit visual confirmation code from two MACs.
 *
 * Code = CRC32(mac_a || mac_b) % 10000, formatted as 0000-9999.
 * Pure function — used internally and exposed for testing.
 */
uint16_t espnow_compute_confirm_code(const uint8_t mac_a[ESPNOW_MAC_LEN],
                                      const uint8_t mac_b[ESPNOW_MAC_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_PEER_SESSION_H_ */
