/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_hal.h
 * @brief  ESP-NOW HAL glue — transport dispatch for CD3 RPC / AT variants.
 *
 * Provides the thin adapter between the pure-logic ESP-NOW protocol modules
 * (espnow_peer_session, espnow_file_transfer, espnow_tictactoe) and the
 * actual ESP32 coprocessor communication.
 *
 * Transport dispatch:
 *   - CD3 (binary M1_RPC): M1_RPC_NOW_* messages (0x0600..0x0605)
 *   - CD3-AT: AT+M1ESPNOW=... commands (future)
 *   - SiN360: Not supported (M1_ESP32_CAP_ESPNOW never set)
 *
 * M1 Project
 */

#ifndef M1_ESPNOW_HAL_H_
#define M1_ESPNOW_HAL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESPNOW_MAC_LEN  6

/*==========================================================================*/
/* Transport layer API                                                      */
/*==========================================================================*/

/**
 * @brief  Start ESP-NOW on the specified WiFi channel.
 *
 * Sends M1_RPC_NOW_START to the ESP32.  Returns the local MAC
 * address from the response.
 *
 * @param  channel  WiFi channel (1-14).
 * @return true on success.
 */
bool m1_espnow_start(uint8_t channel);

/**
 * @brief  Stop ESP-NOW and release radio resources.
 * @return true on success.
 */
bool m1_espnow_stop(void);

/**
 * @brief  Send a broadcast ANNOUNCE message (for peer discovery).
 * @return true on success.
 */
bool m1_espnow_announce(void);

/**
 * @brief  Poll the ESP32 for discovered peers.
 *
 * @param  peers     Output array to fill.
 * @param  max_peers Maximum entries in the output array.
 * @return Number of peers returned.
 */
uint8_t m1_espnow_poll_peers(void *peers, uint8_t max_peers);

/**
 * @brief  Send data to a specific peer MAC.
 *
 * @param  mac   Destination 6-byte MAC address.
 * @param  data  Payload bytes.
 * @param  len   Payload length.  Capped to 42 bytes per call by the 64-byte
 *               SPI transaction limit (SPI_BUF_SIZE(64) - RPC_HDR(16) - MAC(6)).
 *               ENL_MSG_MAX=240 is the ESP-NOW protocol limit but cannot be
 *               reached in a single SPI call without RPC multi-transaction
 *               chunking (not yet implemented).
 * @return true on success.
 */
bool m1_espnow_send(const uint8_t mac[6], const uint8_t *data, size_t len);

/**
 * @brief  Poll for a received message.
 *
 * @param  from_mac  Output: sender's MAC (6 bytes).
 * @param  buf       Output buffer for message payload.
 * @param  buf_size  Size of output buffer.
 * @param  out_len   Output: actual message length.
 * @return true if a message was available.
 */
bool m1_espnow_recv_msg(uint8_t from_mac[6], uint8_t *buf,
                         size_t buf_size, uint8_t *out_len);

/**
 * @brief  Get our ESP32's MAC address (cached from NOW_START response).
 * @param  mac  Output: 6-byte MAC.
 */
void m1_espnow_get_mac(uint8_t mac[6]);

/**
 * @brief  Get the current WiFi channel (default: 1).
 * @return Channel number (1-14).
 */
uint8_t m1_espnow_get_channel(void);

/*==========================================================================*/
/* File operations (FatFS adapter for file transfer)                         */
/*==========================================================================*/

/**
 * @brief  Open a file for writing on the SD card.
 * @param  path  File path (e.g. "/ESPNOW/file.sub").
 * @return Opaque file handle (NULL on failure).
 */
void *m1_espnow_file_open(const char *path);

/**
 * @brief  Write data to an open file.
 * @return true on success.
 */
bool m1_espnow_file_write(void *handle, const uint8_t *data, size_t len);

/**
 * @brief  Close a file handle.
 */
void m1_espnow_file_close(void *handle);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESPNOW_HAL_H_ */
