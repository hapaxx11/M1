/* See COPYING.txt for license details. */

/**
 * @file   subghz_static_tx.c
 * @brief  Blocking static-code TX burst/wait policy — pure logic.
 *
 * See subghz_static_tx.h for the design rationale.
 */

#include "subghz_static_tx.h"

uint16_t subghz_static_tx_total_bursts(uint8_t extra_repeats)
{
    /* 1 initial frame + N extra replays.  uint8_t + 1 can never overflow a
     * uint16_t, but keep the intent explicit. */
    return (uint16_t)(1u + (uint16_t)extra_repeats);
}

subghz_static_tx_wait_t subghz_static_tx_wait_step(bool burst_completed,
                                                   uint32_t elapsed_ms,
                                                   uint32_t timeout_ms)
{
    /* Completion takes priority: a burst that finishes right as the safety
     * window expires must still be reported DONE, otherwise a perfectly good
     * send would be misclassified as a timeout on the last poll. */
    if (burst_completed)
        return SUBGHZ_STATIC_TX_DONE;

    /* timeout_ms == 0 means "no safety cap" — wait indefinitely for the DMA
     * completion.  Otherwise bail once the window has fully elapsed. */
    if (timeout_ms != 0u && elapsed_ms >= timeout_ms)
        return SUBGHZ_STATIC_TX_TIMEOUT;

    return SUBGHZ_STATIC_TX_WAIT;
}
