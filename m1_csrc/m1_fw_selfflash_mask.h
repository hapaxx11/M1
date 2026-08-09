/* See COPYING.txt for license details. */

/*
 * m1_fw_selfflash_mask.h
 *
 * Pure state-machine tracking whether the ESP32 interrupt lines (DataReady /
 * Handshake / UART RX) should stay masked during an M1 self-flash (own STM32
 * firmware) RPC session.
 *
 * Ported/adapted from bedge117/M1 C3.164 ("fix M1 self-flash data-phase
 * reset"): after ESP (WiFi/BLE/802.15.4) activity the ESP keeps asserting
 * these lines and streaming over UART4; those high-priority ISRs preempt the
 * flash worker and the (lowest-priority) IWDG feeder in every unmasked gap.
 * Unmasking right after the erase — before the first DATA chunk arrives —
 * let a buffered ESP backlog starve the feeder/worker and the IWDG reset the
 * device before a single byte was written (inactive bank erased, 0 bytes,
 * device reports v0.0.0 after the reset). The fix is to keep the lines
 * masked for the WHOLE session (erase through FINISH) and only unmask once,
 * whenever the event that ends the session actually happens (FINISH, an
 * error, or a teardown/abandon).
 *
 * This header is hardware-independent — it only tracks the "is the session
 * currently masked, and should the caller invoke unmask now?" decision. The
 * caller supplies the actual HAL_NVIC_Disable/EnableIRQ() calls.
 *
 * M1 Project
 */

#ifndef M1_FW_SELFFLASH_MASK_H
#define M1_FW_SELFFLASH_MASK_H

#include <stdbool.h>

typedef struct
{
    bool masked;
} fw_selfflash_mask_state_t;

/** Reset state to "not masked" (idle, no session in progress). */
void fw_selfflash_mask_init(fw_selfflash_mask_state_t *st);

/**
 * @brief  Enter the masked window. Call exactly once, right before the very
 *         first hardware operation of the session (the bank erase), and
 *         before sending the START ACK. The window then spans every DATA
 *         chunk and ends only via fw_selfflash_mask_end().
 */
void fw_selfflash_mask_begin(fw_selfflash_mask_state_t *st);

/**
 * @brief  Attempt to end the masked window (erase failure, DATA write
 *         failure, FINISH success/failure, or session teardown/abandon).
 * @return true  if the caller must invoke unmask() — the session was masked
 *               and is now unmasked.
 *         false if the session was already unmasked (e.g. this is a second
 *               call, or the erase failed before begin() was ever reached) —
 *               the caller must NOT invoke unmask() again.
 *
 * Idempotent: safe to call more than once for the same session; only the
 * first call after begin() returns true.
 */
bool fw_selfflash_mask_end(fw_selfflash_mask_state_t *st);

#endif /* M1_FW_SELFFLASH_MASK_H */
