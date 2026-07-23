/* See COPYING.txt for license details. */

/**
 * @file   subghz_static_tx.h
 * @brief  Blocking static-code TX burst/wait policy — pure logic, host-testable.
 *
 * The Sub-GHz "Send" action on the Receiver-Info detail view transmits a
 * decoded static-code signal (Princeton, CAME, Nice, …) by encoding the key
 * into an OOK PWM buffer and firing it over TIM1 + DMA.  For reliable reception
 * the frame must be repeated a few times, and the caller must wait for each
 * DMA pass to finish before re-arming the next one.
 *
 * This module isolates the two decisions that historically went wrong in that
 * loop so they can be unit-tested on the host:
 *
 *   1. How many bursts to emit in total (1 initial + N extra repeats).
 *   2. Whether to keep polling for the current burst, stop because the DMA
 *      transfer-complete was observed, or bail out on a safety timeout.
 *
 * The previous implementation polled `subghz_decenc_ctl.ntx_raw_repeat` — a
 * counter only ever decremented by the file-based replay engine, never by the
 * direct-buffer static path — so the wait always ran its full fixed timeout
 * (the UI appeared to "hang" on the Sending overlay) and only a single frame
 * was ever transmitted.  Driving the loop from this policy fixes both issues.
 */

#ifndef SUBGHZ_STATIC_TX_H_
#define SUBGHZ_STATIC_TX_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Per-burst wait decision returned by @ref subghz_static_tx_wait_step(). */
typedef enum {
    SUBGHZ_STATIC_TX_WAIT = 0,  /**< Burst still in flight — keep polling.      */
    SUBGHZ_STATIC_TX_DONE,      /**< DMA transfer-complete observed for burst.  */
    SUBGHZ_STATIC_TX_TIMEOUT    /**< Safety timeout elapsed with no completion. */
} subghz_static_tx_wait_t;

/**
 * @brief  Total number of frames to transmit for a static-code send.
 *
 * @param  extra_repeats  Additional replays after the first transmit.
 * @retval 1 + extra_repeats, saturating at UINT16_MAX.
 */
uint16_t subghz_static_tx_total_bursts(uint8_t extra_repeats);

/**
 * @brief  Decide whether to keep waiting on the in-flight burst.
 *
 * A completion always wins over the timeout, so a burst that finishes exactly
 * as the safety window expires is still reported as DONE rather than TIMEOUT.
 *
 * @param  burst_completed  true once the DMA transfer-complete for the current
 *                          burst has been observed (e.g. the monotonic TX
 *                          completion counter advanced).
 * @param  elapsed_ms       Time waited on the current burst so far.
 * @param  timeout_ms       Per-burst safety cap.  0 disables the cap, so the
 *                          caller waits indefinitely for completion.
 * @retval SUBGHZ_STATIC_TX_DONE     completion observed — advance to next burst
 * @retval SUBGHZ_STATIC_TX_TIMEOUT  safety window elapsed — abort the send
 * @retval SUBGHZ_STATIC_TX_WAIT     neither yet — poll again
 */
subghz_static_tx_wait_t subghz_static_tx_wait_step(bool burst_completed,
                                                   uint32_t elapsed_ms,
                                                   uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SUBGHZ_STATIC_TX_H_ */
