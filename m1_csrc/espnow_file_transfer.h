/* See COPYING.txt for license details. */

/**
 * @file   espnow_file_transfer.h
 * @brief  ESP-NOW file transfer protocol — pure logic, streaming-to-SD.
 *
 * Stop-and-wait ARQ with CRC32 integrity verification.
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * Hardware operations are abstracted via espnow_ft_hal_ops_t function
 * pointers — the caller provides thin adapters for ESP-NOW send and
 * FatFS file operations.
 *
 * M1 Project
 */

#ifndef ESPNOW_FILE_TRANSFER_H_
#define ESPNOW_FILE_TRANSFER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Constants
 * =========================================================================*/

/** Maximum filename length in FILE_OFFER. */
#define ESPNOW_FT_FILENAME_MAX   31

/** Maximum data bytes per FILE_DATA chunk (ESP-NOW 250 - framing). */
#define ESPNOW_FT_CHUNK_MAX      200

/** Default ACK timeout in milliseconds. */
#define ESPNOW_FT_ACK_TIMEOUT_MS 500

/** Maximum retries per chunk before abort. */
#define ESPNOW_FT_MAX_RETRIES    3

/** MAC address length. */
#define ESPNOW_FT_MAC_LEN       6

/* =========================================================================
 * Wire protocol message types
 * =========================================================================*/

typedef enum {
    ESPNOW_FT_MSG_OFFER    = 0x10,
    ESPNOW_FT_MSG_ACCEPT   = 0x11,
    ESPNOW_FT_MSG_REJECT   = 0x12,
    ESPNOW_FT_MSG_DATA     = 0x13,
    ESPNOW_FT_MSG_ACK      = 0x14,
    ESPNOW_FT_MSG_COMPLETE = 0x15,
    ESPNOW_FT_MSG_ABORT    = 0x16,
} espnow_ft_msg_type_t;

/* =========================================================================
 * Transfer states
 * =========================================================================*/

typedef enum {
    ESPNOW_FT_STATE_IDLE = 0,
    /* Sender states */
    ESPNOW_FT_STATE_OFFER_SENT,       /**< Waiting for ACCEPT/REJECT */
    ESPNOW_FT_STATE_SENDING,          /**< Streaming chunks */
    ESPNOW_FT_STATE_WAIT_ACK,         /**< Waiting for chunk ACK */
    ESPNOW_FT_STATE_COMPLETE_SENT,    /**< FILE_COMPLETE sent, done */
    /* Receiver states */
    ESPNOW_FT_STATE_OFFER_RECEIVED,   /**< Offer pending user decision */
    ESPNOW_FT_STATE_RECEIVING,        /**< Receiving chunks */
    ESPNOW_FT_STATE_VERIFYING,        /**< All chunks received, verify CRC */
    /* Terminal */
    ESPNOW_FT_STATE_DONE,             /**< Transfer complete (success) */
    ESPNOW_FT_STATE_FAILED,           /**< Transfer failed / aborted */
} espnow_ft_state_t;

/* =========================================================================
 * HAL operations abstraction
 * =========================================================================*/

/** Opaque file handle (maps to FIL* or test mock). */
typedef void *espnow_ft_file_t;

typedef struct {
    /** Send an ESP-NOW frame to a peer MAC. Returns true on success. */
    bool (*send)(const uint8_t mac[ESPNOW_FT_MAC_LEN],
                 const uint8_t *data, size_t len, void *ctx);

    /** Open a file for writing. Returns non-NULL on success. */
    espnow_ft_file_t (*file_open)(const char *path, void *ctx);

    /** Write data to an open file. Returns true on success. */
    bool (*file_write)(espnow_ft_file_t f, const uint8_t *data,
                       size_t len, void *ctx);

    /** Close a file. */
    void (*file_close)(espnow_ft_file_t f, void *ctx);

    /** Get current time in milliseconds (for timeouts). */
    uint32_t (*millis)(void *ctx);

    /** User context pointer passed to all callbacks. */
    void *ctx;
} espnow_ft_hal_ops_t;

/* =========================================================================
 * Transfer context
 * =========================================================================*/

typedef struct {
    espnow_ft_state_t state;

    /** Peer MAC we are transferring with. */
    uint8_t peer_mac[ESPNOW_FT_MAC_LEN];

    /** File metadata. */
    char     filename[ESPNOW_FT_FILENAME_MAX + 1];
    uint32_t file_size;
    uint32_t expected_crc32;
    uint8_t  chunk_size;   /**< Negotiated chunk size (≤ ESPNOW_FT_CHUNK_MAX) */

    /** Transfer progress. */
    uint32_t bytes_transferred;
    uint8_t  current_seq;
    uint8_t  retry_count;

    /** Running CRC32 (accumulated incrementally). */
    uint32_t running_crc32;

    /** Timestamp of last send (for timeout detection). */
    uint32_t last_send_ms;

    /** Offset and length of the last chunk sent (for exact retry rollback). */
    uint32_t last_chunk_offset;
    uint32_t last_chunk_len;

    /** File handle (receiver side). */
    espnow_ft_file_t file_handle;

    /** HAL operations. */
    const espnow_ft_hal_ops_t *hal;
} espnow_ft_ctx_t;

/* =========================================================================
 * API — Sender side
 * =========================================================================*/

/**
 * @brief  Initialise a transfer context for sending.
 * @param  ctx        Transfer context.
 * @param  hal        HAL operations (must outlive the transfer).
 * @param  peer_mac   Destination MAC.
 * @param  filename   File name (truncated to ESPNOW_FT_FILENAME_MAX).
 * @param  file_size  Total file size in bytes.
 * @param  file_crc32 Pre-computed CRC32 of the entire file.
 * @param  chunk_size Data bytes per chunk (clamped to ESPNOW_FT_CHUNK_MAX).
 */
void espnow_ft_send_init(espnow_ft_ctx_t *ctx,
                          const espnow_ft_hal_ops_t *hal,
                          const uint8_t peer_mac[ESPNOW_FT_MAC_LEN],
                          const char *filename,
                          uint32_t file_size,
                          uint32_t file_crc32,
                          uint8_t chunk_size);

/**
 * @brief  Send the FILE_OFFER message. Transitions IDLE → OFFER_SENT.
 * @return true if the offer was sent successfully.
 */
bool espnow_ft_send_offer(espnow_ft_ctx_t *ctx);

/**
 * @brief  Process a received message (sender side).
 *
 * Handles ACCEPT, REJECT, ACK responses.
 *
 * @param  ctx   Transfer context.
 * @param  type  Message type byte.
 * @param  seq   Message sequence byte.
 * @param  data  Message payload (after type+seq header).
 * @param  len   Payload length.
 * @return true if the message was handled.
 */
bool espnow_ft_send_on_recv(espnow_ft_ctx_t *ctx, uint8_t type, uint8_t seq,
                             const uint8_t *data, size_t len);

/**
 * @brief  Send the next data chunk. Call when the sender has data ready.
 *
 * @param  ctx        Transfer context.
 * @param  chunk_data Pointer to chunk payload.
 * @param  chunk_len  Bytes in this chunk (≤ chunk_size).
 * @return true if the chunk was sent.
 */
bool espnow_ft_send_chunk(espnow_ft_ctx_t *ctx, const uint8_t *chunk_data,
                           size_t chunk_len);

/**
 * @brief  Check for ACK timeout. Call periodically.
 * @return true if a timeout occurred and retry/abort was handled.
 */
bool espnow_ft_send_check_timeout(espnow_ft_ctx_t *ctx);

/* =========================================================================
 * API — Receiver side
 * =========================================================================*/

/**
 * @brief  Initialise a transfer context for receiving.
 * @param  ctx  Transfer context.
 * @param  hal  HAL operations.
 */
void espnow_ft_recv_init(espnow_ft_ctx_t *ctx,
                          const espnow_ft_hal_ops_t *hal);

/**
 * @brief  Process a received message (receiver side).
 *
 * Handles OFFER, DATA, COMPLETE messages.
 *
 * @param  ctx       Transfer context.
 * @param  peer_mac  Sender's MAC.
 * @param  type      Message type byte.
 * @param  seq       Sequence number.
 * @param  data      Message payload.
 * @param  len       Payload length.
 * @return true if the message was handled.
 */
bool espnow_ft_recv_on_msg(espnow_ft_ctx_t *ctx,
                            const uint8_t peer_mac[ESPNOW_FT_MAC_LEN],
                            uint8_t type, uint8_t seq,
                            const uint8_t *data, size_t len);

/**
 * @brief  User accepts the file offer. Transitions OFFER_RECEIVED → RECEIVING.
 * @param  ctx       Transfer context.
 * @param  save_path Path on SD card to write the file to.
 * @return true on success (file opened, ACCEPT sent).
 */
bool espnow_ft_recv_accept(espnow_ft_ctx_t *ctx, const char *save_path);

/**
 * @brief  User rejects the file offer. Transitions OFFER_RECEIVED → IDLE.
 * @return true on success (REJECT sent).
 */
bool espnow_ft_recv_reject(espnow_ft_ctx_t *ctx);

/* =========================================================================
 * Utility
 * =========================================================================*/

/**
 * @brief  CRC32 computation (same polynomial as bit_util.c).
 *         Exposed for testing; used internally for running CRC accumulation.
 */
uint32_t espnow_ft_crc32(uint32_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_FILE_TRANSFER_H_ */
