/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_read_raw_state.h
 * @brief  Pure-logic enum + inline helpers for the Sub-GHz Read Raw scene
 *         state machine.  Extracted from m1_subghz_scene.h so that host
 *         unit tests can exercise the dispatch logic without pulling in the
 *         full scene context (SubGhzApp, decenc state, etc.).
 *
 *  Convention: this header has no dependencies beyond <stdbool.h> so it can
 *  be included in both firmware and tests.  Do NOT add anything here that
 *  references hardware, FreeRTOS, or the scene framework.
 */

#ifndef M1_SUBGHZ_READ_RAW_STATE_H_
#define M1_SUBGHZ_READ_RAW_STATE_H_

#include <stdbool.h>

typedef enum {
    SubGhzReadRawStateStart = 0,   /**< Fresh entry — no capture exists */
    SubGhzReadRawStateRecording,   /**< Actively recording raw RF data */
    SubGhzReadRawStateIdle,        /**< Recording done — capture file on SD */
    SubGhzReadRawStateLoaded,      /**< Pre-existing RAW file loaded from Saved browser (Momentum: LoadKeyIDLE) */

    /* --- Momentum-aligned async TX states (Phase 1) --- */
    SubGhzReadRawStateTX,                  /**< Async TX from freshly-recorded capture (Idle → TX → Idle) */
    SubGhzReadRawStateTXRepeat,            /**< Reserved for Phase 2 hold-to-repeat (Idle origin) */
    SubGhzReadRawStateLoadKeyTX,           /**< Async TX from pre-loaded file (Loaded → LoadKeyTX → Loaded) */
    SubGhzReadRawStateLoadKeyTXRepeat,     /**< Reserved for Phase 2 hold-to-repeat (Loaded origin) */
} SubGhzReadRawState;

/**
 * @brief  Pure-logic test: true if the state is one of the four TX states.
 *
 * Extracted as a named function so callers do not inline four-way OR checks
 * that would silently drift from the enum when new TX states are added.
 */
static inline bool subghz_read_raw_state_is_tx(SubGhzReadRawState s)
{
    return s == SubGhzReadRawStateTX
        || s == SubGhzReadRawStateTXRepeat
        || s == SubGhzReadRawStateLoadKeyTX
        || s == SubGhzReadRawStateLoadKeyTXRepeat;
}

/**
 * @brief  Pure-logic: post-TX return state.
 *
 * @param  tx_state  Any of the four TX states (or a non-TX state).
 * @retval  SubGhzReadRawStateLoaded  when the TX originated from a pre-loaded file
 *                                     (LoadKeyTX / LoadKeyTXRepeat).
 * @retval  SubGhzReadRawStateIdle    otherwise (TX / TXRepeat — freshly-recorded
 *                                     capture).  Also the safe fall-through when
 *                                     called with a non-TX state.
 */
static inline SubGhzReadRawState subghz_read_raw_state_after_tx(SubGhzReadRawState tx_state)
{
    if (tx_state == SubGhzReadRawStateLoadKeyTX ||
        tx_state == SubGhzReadRawStateLoadKeyTXRepeat)
        return SubGhzReadRawStateLoaded;
    return SubGhzReadRawStateIdle;
}

#endif /* M1_SUBGHZ_READ_RAW_STATE_H_ */
