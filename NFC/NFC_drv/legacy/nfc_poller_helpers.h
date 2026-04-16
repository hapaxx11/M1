/* See COPYING.txt for license details. */

/**
 * @file  nfc_poller_helpers.h
 * @brief Pure-logic helpers extracted from nfc_poller.c.
 *
 * This module contains hardware-independent helper functions that can
 * be compiled and tested on the host without HAL/RTOS/RFAL dependencies.
 */

#ifndef NFC_POLLER_HELPERS_H_
#define NFC_POLLER_HELPERS_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Check whether a SAK byte indicates a MIFARE Classic variant.
 *
 * Covers Classic 1K/4K, Mini, Plus SL2, and Classic EV1.
 *
 * @param  sak  SAK byte from NFC-A anticollision
 * @retval true  SAK matches a known Classic variant
 * @retval false SAK does not match
 */
bool mfc_is_classic_sak(uint8_t sak);

#endif /* NFC_POLLER_HELPERS_H_ */
