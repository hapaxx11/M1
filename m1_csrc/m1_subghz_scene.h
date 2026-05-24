/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene.h
 * @brief  Sub-GHz Scene Manager — Flipper-inspired scene architecture for M1.
 *
 * Provides a scene stack, event dispatch, and application context that replaces
 * the monolithic state machine in m1_sub_ghz.c.  Each scene owns on_enter(),
 * on_event(), on_exit(), and draw() callbacks with clean separation of concerns.
 *
 * Button model (enforced across ALL scenes):
 *   OK    = primary action (start, select, confirm, send)
 *   BACK  = go back / exit current scene
 *   L/R   = change value (frequency, modulation) in selector contexts
 *   U/D   = scroll list items, navigate menu
 *
 * Button bar column mapping (subghz_button_bar_draw):
 *   LEFT column  = LEFT button action
 *   CENTER column = OK button action
 *   RIGHT column  = RIGHT button action
 *
 * Exception: Read Raw intercepts BACK during Recording to stop capture
 * and transition to Idle (preserving the file) instead of exiting the
 * scene.  This matches Momentum firmware behaviour — exiting mid-capture
 * would discard or corrupt the partially-written file.
 */

#ifndef M1_SUBGHZ_SCENE_H_
#define M1_SUBGHZ_SCENE_H_

#include <stdint.h>
#include <stdbool.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_scene_state.h"
#include "subghz_scene_polish.h"

/*============================================================================*/
/* Scene identifiers                                                          */
/*============================================================================*/

typedef enum {
    SubGhzSceneMenu = 0,      /**< Entry menu (Read, Read Raw, Saved, …) */
    SubGhzSceneRead,           /**< Protocol-aware receive + decode */
    SubGhzSceneReadRaw,        /**< Raw RF capture with waveform */
    SubGhzSceneReceiverInfo,   /**< Decoded signal detail + save/send */
    SubGhzSceneConfig,         /**< Frequency / modulation / hopping settings */
    SubGhzSceneSaveName,       /**< Virtual keyboard filename entry */
    SubGhzSceneSaveSuccess,    /**< Brief save confirmation */
    SubGhzSceneNeedSaving,     /**< "Unsaved signals — save?" prompt */
    SubGhzSceneSaved,          /**< Browse saved files + action menu */
    SubGhzSceneFreqAnalyzer,   /**< Frequency analyzer / scanner */
    SubGhzScenePlaylist,       /**< Sub-GHz playlist player */
    SubGhzSceneSpectrumAnalyzer, /**< Bar-graph spectrum display */
    SubGhzSceneRssiMeter,      /**< Continuous RSSI signal strength */
    SubGhzSceneFreqScanner,    /**< Frequency scanner with hit list */
    SubGhzSceneWeatherStation, /**< Weather protocol decoder */
    SubGhzSceneBruteForce,     /**< Brute-force code transmitter */
    SubGhzSceneAddManually,    /**< Manual signal entry and TX */
    SubGhzSceneRemote,         /**< Multi-button RF remote control */
    SubGhzSceneBindWizard,     /**< Bind New Remote step-by-step wizard */
    SubGhzSceneTransmitter,    /**< Generic key-file Transmitter (Phase 3b-2b) */
    SubGhzSceneSavedMenu,      /**< Action menu (Decode/Emulate/Info/Rename/Delete) for a selected Saved file */
    SubGhzSceneDelete,         /**< Delete-file confirmation dialog */
    SubGhzSceneCount           /**< Number of scenes */
} SubGhzSceneId;

/*============================================================================*/
/* Constants                                                                  */
/*============================================================================*/

#define SUBGHZ_REMOTE_BUTTON_COUNT  5   /**< UP / DOWN / LEFT / RIGHT / OK */

/*============================================================================*/
/* Custom events (inter-scene communication)                                  */
/*============================================================================*/

typedef enum {
    /* Generic */
    SubGhzEventNone = 0,
    SubGhzEventBack,           /**< BACK button pressed */
    SubGhzEventOk,             /**< OK button pressed */
    SubGhzEventLeft,           /**< LEFT button pressed */
    SubGhzEventRight,          /**< RIGHT button pressed */
    SubGhzEventUp,             /**< UP button pressed */
    SubGhzEventDown,           /**< DOWN button pressed */

    /* Radio / decode */
    SubGhzEventRxData,         /**< Raw RX samples available (Q_EVENT_SUBGHZ_RX) */
    SubGhzEventTxComplete,     /**< TX DMA finished (Q_EVENT_SUBGHZ_TX) */
    SubGhzEventHopperTick,     /**< Hopper dwell timeout — time to hop */
    SubGhzEventDecode,         /**< New protocol decode added to history */

    /* Save flow */
    SubGhzEventSaveOk,         /**< File saved successfully */
    SubGhzEventSaveFail,       /**< File save failed */
    SubGhzEventSaveCancel,     /**< User cancelled save */

    /* Timeout */
    SubGhzEventTimeout,        /**< Screen timeout / auto-dismiss */

    /* Phase 3b-2a — scene-manager polish */
    SubGhzEventTick,           /**< Periodic tick (cadence set per-scene via
                                    subghz_scene_set_tick_period) */
    SubGhzEventCustom,         /**< Scene-specific custom event; payload
                                    accessible via subghz_scene_custom_payload() */
} SubGhzEvent;

/*============================================================================*/
/* Read scene sub-states                                                      */
/*============================================================================*/

typedef enum {
    SubGhzReadStateIdle = 0,   /**< Ready screen, not recording */
    SubGhzReadStateRx,         /**< Actively receiving */
    SubGhzReadStateStopped,    /**< Recording stopped, pending save */
} SubGhzReadState;

/*============================================================================*/
/* Read Raw sub-states                                                        */
/*============================================================================*/

/* Read Raw state enum + pure-logic helpers live in their own header so they
 * can be unit-tested on the host without pulling in the full scene context. */
#include "m1_subghz_read_raw_state.h"

/* The legacy SubGhzReadRawStateSending alias was removed once all callers
 * migrated to the explicit four-state TX enum.  Do not reintroduce it —
 * see CLAUDE.md "Async / Non-Blocking RTOS Best Practices". */

/*============================================================================*/
/* Application context — replaces scattered globals                           */
/*============================================================================*/

/** Maximum scene stack depth (sufficient for Menu → Read → Info → SaveName) */
#define SUBGHZ_SCENE_STACK_MAX  8

/** Frequency presets (defined in m1_sub_ghz.c, referenced here) */
#define SUBGHZ_FREQ_PRESET_CNT  17
#define SUBGHZ_FREQ_DEFAULT_ID  10  /* 433.92 MHz */
#define SUBGHZ_MOD_PRESET_CNT   4

typedef struct {
    /* --- Scene manager state --- */
    SubGhzSceneId scene_stack[SUBGHZ_SCENE_STACK_MAX];
    uint8_t       scene_depth;        /**< Current stack depth (0 = empty) */

    /** Per-scene 32-bit state slots, keyed by SubGhzSceneId.  Persisted
     *  across push/pop so scenes can stash UI state (cursor position,
     *  sub-mode, last-target) without resorting to file-scope statics.
     *  See Phase 2 of the Momentum-parity migration plan. */
    subghz_scene_state_array_t scene_state;

    /* --- Radio config --- */
    uint8_t  freq_idx;                /**< Index into frequency presets */
    uint8_t  mod_idx;                 /**< Index into modulation presets */
    bool     hopping;                 /**< Frequency hopping enabled */
    bool     sound;                   /**< Beep on decode */
    bool     autosave;                /**< Auto-save decoded signals to SD */
    bool     bin_raw;                 /**< BIN_RAW decoder enabled */
    uint8_t  tx_power_idx;            /**< TX power level index */

    /* --- Hopper state (active during RX) --- */
    bool     hopper_active;
    uint8_t  hopper_idx;
    uint32_t hopper_freq;             /**< Current hopper frequency (Hz) */

    /* --- Read scene state --- */
    SubGhzReadState read_state;
    SubGHz_History_t history;         /**< Decoded signal history ring */
    uint8_t  history_sel;             /**< Selected history index */
    uint8_t  history_scroll;          /**< Top visible index */
    bool     history_view;            /**< Showing history list */
    bool     detail_view;             /**< Showing signal detail */
    SubGHz_Dec_Info_t last_decoded;   /**< Most recent decode result */
    bool     has_decoded;             /**< At least one decode received */
    int16_t  rssi;                    /**< Current RSSI (dBm) */

    /* --- Read Raw state --- */
    SubGhzReadRawState raw_state;
    uint32_t raw_sample_count;        /**< Total RAW samples received */
    char     raw_load_path[72];       /**< Pre-loaded filepath for Loaded state (set by Saved scene) */
    bool     raw_load_is_native;      /**< true = .sgh (use sub_ghz_replay_datafile) */
    uint32_t raw_load_freq_hz;        /**< Frequency for native replay */
    uint8_t  raw_load_mod;            /**< Modulation for native replay */

    /* --- Save flow --- */
    char     file_path[64];           /**< Current file path for save */
    char     file_name[32];           /**< Suggested filename */

    /* --- Computed / derived --- */
    uint32_t current_freq_hz;         /**< Active frequency in Hz */

    /* --- Playlist state --- */
    char     playlist_path[64];       /**< Path to current .txt playlist */
    char     playlist_files[16][64];  /**< Up to 16 .sub file paths */
    uint16_t playlist_delays[16];     /**< Per-entry delay in ms (0 = no delay) */
    uint8_t  playlist_count;          /**< Number of entries loaded */
    uint8_t  playlist_current;        /**< Index of file being transmitted */
    uint8_t  playlist_repeat_total;   /**< Total repeats (0 = infinite) */
    uint8_t  playlist_repeat_done;    /**< Completed repeat passes */
    bool     playlist_running;        /**< Playback active */

    /* --- Remote state (SubGHz Remote scene) --- */
    char remote_path[64];             /**< Path to loaded .rem manifest */
    char remote_label[SUBGHZ_REMOTE_BUTTON_COUNT][24];  /**< Button label */
    char remote_files[SUBGHZ_REMOTE_BUTTON_COUNT][64];  /**< .sub path per button */
    bool remote_loaded;               /**< true if a manifest is loaded **/

    /* --- Flags --- */
    bool     need_redraw;             /**< Scene requests display update */
    bool     running;                 /**< false = exit scene manager loop */
    bool     resume_from_child;       /**< Set when Read pushes a child scene;
                                           cleared after resume_rx() uses it */

    /* --- Phase 3b-2a — scene-manager polish state --- */
    /** Wall-clock cadence (ms) for SubGhzEventTick.  Set via
     *  subghz_scene_set_tick_period(); 0 disables ticks.  Reset to 0
     *  automatically on every scene push/pop/replace so a scene that
     *  forgot to enable ticks does not inherit a parent's cadence. */
    uint32_t tick_period_ms;
    /** HAL_GetTick() value at the most recent SubGhzEventTick dispatch.
     *  Reset to current time on scene push/pop/replace so the first tick
     *  after a scene transition fires a full `tick_period_ms` later. */
    uint32_t last_tick_ms;
    /** Payload carried with the next SubGhzEventCustom dispatch.  Written
     *  by subghz_scene_send_custom_event(); read by scenes via
     *  subghz_scene_custom_payload(). */
    subghz_scene_custom_payload_t custom_event_payload;

    /* --- Phase 3b-2b — Transmitter scene input parameters --- */
    /** Path of the key/packet/Flipper file to transmit.  Set by the
     *  caller before pushing SubGhzSceneTransmitter; remains valid for
     *  the lifetime of that scene.  Empty string = no file (the scene
     *  shows a "Load a file" error and BACK exits). */
    char     tx_path[72];
    /** Engine repeat count (tail bursts) — typical: 1 for static OOK,
     *  3 for rolling codes.  Clamped to >= 1 by the controller. */
    uint16_t tx_repeat_count;
    /** TX mode for the Transmitter scene.  Stored as uint8_t (avoids
     *  pulling subghz_endless_tx.h into the public scene header):
     *  0 = SUBGHZ_TX_MODE_SINGLE, 1 = SUBGHZ_TX_MODE_ENDLESS.  The
     *  scene casts to subghz_endless_tx_mode_t at init time. */
    uint8_t  tx_mode;
    /** Set by the Transmitter scene before popping so the parent scene
     *  can distinguish a natural TX-complete pop from a user-initiated
     *  BACK abort.  Initialised to true at Transmitter scene_on_enter
     *  and overridden to false in the BACK event handler.  Read by
     *  parent scenes (e.g. Playlist) in their scene_on_enter after
     *  pop-back to decide whether to advance or stop a sequence. */
    bool     tx_completed_naturally;
    /** Auto-start hint for the Transmitter scene.  When true, the scene
     *  synthesizes an OK_PRESS during scene_on_enter so TX starts
     *  immediately without requiring the user to confirm on the READY
     *  screen.  Used by the Remote scene (Phase 3b-2b-iv) so that
     *  pressing a mapped button fires the signal in one press, matching
     *  the legacy blocking-wrapper behaviour.  The Transmitter scene
     *  clears this back to false after consuming it so a later pop-back
     *  won't auto-restart. */
    bool     tx_autostart;
    /** Phase 4b — protocol name of the file pointed to by `tx_path`,
     *  used by the Transmitter scene to query
     *  @ref subghz_button_caps_for_protocol() and decide whether to
     *  enable LEFT/RIGHT button cycling.  Set by the caller before
     *  pushing the Transmitter scene; empty string disables cycling
     *  (used by Playlist / Remote where button cycling is not part of
     *  the UX).  Sized to match @ref FLIPPER_SUBGHZ_PROTO_MAX_LEN. */
    char     tx_protocol_name[32];

    /* --- Phase 5 — shared file-selection state for Saved/SavedMenu/Delete --- */
    /** Path of the currently-selected saved file, in storage form
     *  ("/SUBGHZ/<name>" — without the "0:" volume prefix).  Set by the
     *  Saved file-browser scene when a file is chosen, consumed by the
     *  SavedMenu scene (for action handling) and the Delete scene (for
     *  the unlink target).  Empty string when no file is selected. */
    char     saved_filepath[64];
    /** Bare filename (basename only, no directory) of the currently-
     *  selected saved file.  Set together with `saved_filepath`. */
    char     saved_filename[32];
} SubGhzApp;

/*============================================================================*/
/* Scene callback signatures                                                  */
/*============================================================================*/

/**
 * @brief  Called when scene becomes active (pushed or returned to).
 * @param  app  Application context
 */
typedef void (*SubGhzSceneOnEnter)(SubGhzApp *app);

/**
 * @brief  Called for each event while scene is active.
 * @param  app    Application context
 * @param  event  The event to handle
 * @retval true   Event was consumed
 * @retval false  Event not consumed (pass to parent or ignore)
 */
typedef bool (*SubGhzSceneOnEvent)(SubGhzApp *app, SubGhzEvent event);

/**
 * @brief  Called when scene is being replaced (popped or overlaid).
 * @param  app  Application context
 */
typedef void (*SubGhzSceneOnExit)(SubGhzApp *app);

/**
 * @brief  Called to render the scene's display.
 * @param  app  Application context
 */
typedef void (*SubGhzSceneDraw)(SubGhzApp *app);

/** Scene descriptor — one per SubGhzSceneId */
typedef struct {
    SubGhzSceneOnEnter on_enter;
    SubGhzSceneOnEvent on_event;
    SubGhzSceneOnExit  on_exit;
    SubGhzSceneDraw    draw;
} SubGhzSceneHandlers;

/*============================================================================*/
/* Scene Manager API                                                          */
/*============================================================================*/

/**
 * @brief  Initialize the scene manager and app context.
 *         Call once before entering the scene loop.
 */
void subghz_scene_init(SubGhzApp *app);

/**
 * @brief  Push a new scene onto the stack.
 *         Calls on_exit() on the current scene and on_enter() on the new one.
 */
void subghz_scene_push(SubGhzApp *app, SubGhzSceneId scene);

/**
 * @brief  Pop the current scene from the stack.
 *         Calls on_exit() on the current scene and on_enter() on the previous one.
 *         If the stack is empty, sets app->running = false (exit).
 */
void subghz_scene_pop(SubGhzApp *app);

/**
 * @brief  Replace the current scene (pop + push without re-entering parent).
 */
void subghz_scene_replace(SubGhzApp *app, SubGhzSceneId scene);

/**
 * @brief  Send an event to the current scene's on_event handler.
 * @retval true if consumed, false otherwise
 */
bool subghz_scene_send_event(SubGhzApp *app, SubGhzEvent event);

/**
 * @brief  Draw the current scene.
 */
void subghz_scene_draw(SubGhzApp *app);

/**
 * @brief  Get the currently active scene ID.
 */
SubGhzSceneId subghz_scene_current(const SubGhzApp *app);

/*============================================================================*/
/* Per-scene state accessors (Phase 2 — Momentum parity)                      */
/*============================================================================*/

/**
 * @brief  Store a 32-bit value in the slot for @p scene.
 *
 * Use to stash UI state (cursor index, sub-mode, last-selected target)
 * that must persist across child-scene pushes and pops.  Slots are
 * zero-initialised by `subghz_scene_init()`.
 *
 * Out-of-range @p scene is a silent no-op (matches Flipper semantics).
 */
void subghz_scene_set_state(SubGhzApp *app, SubGhzSceneId scene, uint32_t value);

/**
 * @brief  Read the 32-bit state slot for @p scene.
 * @return The stored value, or 0 if no value has been stored yet (or @p scene
 *         is out of range).
 */
uint32_t subghz_scene_get_state(const SubGhzApp *app, SubGhzSceneId scene);

/*============================================================================*/
/* Polish API (Phase 3b-2a — Momentum parity)                                 */
/*============================================================================*/

/**
 * @brief  Pop scenes one at a time until @p target is on top of the stack.
 *
 * Calls `on_exit` on every scene popped (excluding @p target) and `on_enter`
 * on @p target.  If @p target is not currently on the stack the call is a
 * no-op and the stack is unchanged.
 *
 * Backed by the pure-logic helper `subghz_scene_stack_pop_to_depth` so the
 * search semantics (topmost match wins) are unit-tested independently of
 * any hardware.
 *
 * @return  true if @p target was found and the stack was popped to it;
 *          false if @p target was not on the stack.
 */
bool subghz_scene_search_and_pop_to(SubGhzApp *app, SubGhzSceneId target);

/**
 * @brief  Send a custom event to the current scene with a 32-bit payload.
 *
 * Stores @p payload in `app->custom_event_payload`, then dispatches a
 * `SubGhzEventCustom` event to the current scene's `on_event` handler.
 * The receiving scene reads the payload via `subghz_scene_custom_payload()`.
 *
 * @return  Whatever the scene's `on_event` returned (true = consumed).
 */
bool subghz_scene_send_custom_event(SubGhzApp *app,
                                     subghz_scene_custom_payload_t payload);

/**
 * @brief  Read the payload of the currently-dispatching custom event.
 *
 * Only meaningful from inside an `on_event` handler that received
 * `SubGhzEventCustom`.  Outside that context the value is whatever was
 * last passed to `subghz_scene_send_custom_event()`.
 */
subghz_scene_custom_payload_t subghz_scene_custom_payload(const SubGhzApp *app);

/**
 * @brief  Configure the per-scene tick cadence.
 *
 * When @p period_ms > 0, the scene-manager loop dispatches
 * `SubGhzEventTick` to the current scene every @p period_ms milliseconds
 * of wall-clock time.  Use for animation (sine-wave preview, burst-count
 * blink) or timed auto-dismiss.  Setting @p period_ms = 0 disables ticks.
 *
 * The cadence is reset to 0 automatically on every scene push/pop/replace
 * so a child scene cannot accidentally inherit its parent's tick rate.
 * Scenes that want ticks must call this from their `on_enter` handler.
 *
 * The first tick after this call fires @p period_ms after `HAL_GetTick()`
 * at the moment of the call, not immediately, so callers do not have to
 * suppress a spurious first tick.
 */
void subghz_scene_set_tick_period(SubGhzApp *app, uint32_t period_ms);

/**
 * @brief  Main entry point — runs the scene manager loop.
 *         Initializes the app, pushes SubGhzSceneMenu, and processes events
 *         until app->running becomes false.
 */
void subghz_scene_app_run(void);

/*============================================================================*/
/* Scene handler tables (defined in each scene_*.c file)                      */
/*============================================================================*/

extern const SubGhzSceneHandlers subghz_scene_menu_handlers;
extern const SubGhzSceneHandlers subghz_scene_read_handlers;
extern const SubGhzSceneHandlers subghz_scene_read_raw_handlers;
extern const SubGhzSceneHandlers subghz_scene_receiver_info_handlers;
extern const SubGhzSceneHandlers subghz_scene_config_handlers;
extern const SubGhzSceneHandlers subghz_scene_save_name_handlers;
extern const SubGhzSceneHandlers subghz_scene_save_success_handlers;
extern const SubGhzSceneHandlers subghz_scene_need_saving_handlers;
extern const SubGhzSceneHandlers subghz_scene_saved_handlers;
extern const SubGhzSceneHandlers subghz_scene_freq_analyzer_handlers;
extern const SubGhzSceneHandlers subghz_scene_playlist_handlers;
extern const SubGhzSceneHandlers subghz_scene_spectrum_analyzer_handlers;
extern const SubGhzSceneHandlers subghz_scene_rssi_meter_handlers;
extern const SubGhzSceneHandlers subghz_scene_freq_scanner_handlers;
extern const SubGhzSceneHandlers subghz_scene_weather_station_handlers;
extern const SubGhzSceneHandlers subghz_scene_brute_force_handlers;
extern const SubGhzSceneHandlers subghz_scene_add_manually_handlers;
extern const SubGhzSceneHandlers subghz_scene_remote_handlers;
extern const SubGhzSceneHandlers subghz_scene_bind_wizard_handlers;
extern const SubGhzSceneHandlers subghz_scene_transmitter_handlers;
extern const SubGhzSceneHandlers subghz_scene_saved_menu_handlers;
extern const SubGhzSceneHandlers subghz_scene_delete_handlers;

#endif /* M1_SUBGHZ_SCENE_H_ */
