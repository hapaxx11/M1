/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_read_raw_state.h
 * @brief  Pure-logic enum + inline helpers for the Sub-GHz Read Raw scene
 *         state machine.  Extracted from m1_subghz_scene.h so that host
 *         unit tests can exercise the dispatch logic without pulling in the
 *         full scene context (SubGhzApp, decenc state, etc.).
 *
 *  Convention: this header has no dependencies beyond <stdbool.h>/<stdint.h> so it can
 *  be included in both firmware and tests.  Do NOT add anything here that
 *  references hardware, FreeRTOS, or the scene framework.
 */

#ifndef M1_SUBGHZ_READ_RAW_STATE_H_
#define M1_SUBGHZ_READ_RAW_STATE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SubGhzReadRawStateStart = 0,   /**< Fresh entry — no capture exists */
    SubGhzReadRawStateRecording,   /**< Actively recording raw RF data */
    SubGhzReadRawStateIdle,        /**< Recording done — capture file on SD */
    SubGhzReadRawStateLoaded,      /**< Pre-existing RAW file loaded from Saved browser (Momentum: LoadKeyIDLE) */

    /* --- Momentum-aligned async TX states --- */
    SubGhzReadRawStateTX,                  /**< Async TX from freshly-recorded capture (Idle → TX → Idle) */
    SubGhzReadRawStateTXRepeat,            /**< Hold-to-repeat from Idle (TX → TXRepeat while OK held) */
    SubGhzReadRawStateLoadKeyTX,           /**< Async TX from pre-loaded file (Loaded → LoadKeyTX → Loaded) */
    SubGhzReadRawStateLoadKeyTXRepeat,     /**< Hold-to-repeat from Loaded (LoadKeyTX → LoadKeyTXRepeat while OK held) */
} SubGhzReadRawState;

static inline bool subghz_read_raw_consume_resume(bool *resume_from_child)
{
    if (resume_from_child == NULL)
        return false;
    const bool resume = *resume_from_child;
    *resume_from_child = false;
    return resume;
}

static inline bool subghz_read_raw_should_load_preloaded(bool is_resume,
                                                        bool has_raw_path)
{
    return !is_resume && has_raw_path;
}

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

/**
 * @brief  Pure-logic: TX → repeat-state transition for Phase 2 hold-to-repeat.
 *
 * Called from the Read Raw scene's SubGhzEventTxComplete handler when the OK
 * button is still held at the end of a TX burst.  Promotes the one-shot TX
 * states to their hold-to-repeat counterparts and leaves the already-repeating
 * states (or any non-TX state) unchanged.
 *
 * @param  s  Current scene state.
 * @retval  SubGhzReadRawStateTXRepeat         when s was TX or TXRepeat
 *          SubGhzReadRawStateLoadKeyTXRepeat  when s was LoadKeyTX or LoadKeyTXRepeat
 *          s (unchanged)                       for any non-TX state
 */
static inline SubGhzReadRawState subghz_read_raw_state_to_repeat(SubGhzReadRawState s)
{
    switch (s)
    {
        case SubGhzReadRawStateTX:
        case SubGhzReadRawStateTXRepeat:
            return SubGhzReadRawStateTXRepeat;
        case SubGhzReadRawStateLoadKeyTX:
        case SubGhzReadRawStateLoadKeyTXRepeat:
            return SubGhzReadRawStateLoadKeyTXRepeat;
        default:
            return s;
    }
}

/**
 * @brief  Pure-logic: should the TX burst auto-restart after completion?
 *
 * Called from the Read Raw scene's SubGhzEventTxComplete handler.  Returns
 * true when the burst should immediately restart — either because the user
 * is physically holding the OK button, or because the persistent repeat-mode
 * toggle is active.
 *
 * Extracted as a named function so host tests can verify all four
 * (toggle, ok_held) combinations without touching hardware.
 *
 * @param  repeat_toggle  Value of `SubGhzApp::raw_tx_repeat_mode`.
 * @param  ok_held        true when the OK GPIO reads pressed at burst end.
 * @retval true  if the TX engine should loop.
 * @retval false if one-shot: the TX engine should stop after this burst.
 */
static inline bool subghz_raw_tx_should_repeat(bool repeat_toggle, bool ok_held)
{
    return repeat_toggle || ok_held;
}

/* The displayed count is the actual number of durations captured, independent
 * of the RSSI-gated waveform cursor. */
static inline uint32_t subghz_read_raw_display_count_update(uint32_t new_total)
{
    return new_total;
}

static inline uint32_t subghz_read_raw_recording_total_update(
    uint32_t current_total, uint32_t flushed)
{
    return current_total + flushed;
}

static inline bool subghz_read_raw_capture_active(bool rssi_signal, bool edges_seen)
{
    return rssi_signal || edges_seen;
}

#endif /* M1_SUBGHZ_READ_RAW_STATE_H_ */
