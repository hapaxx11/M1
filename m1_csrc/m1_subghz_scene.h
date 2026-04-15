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
    SubGhzSceneCount           /**< Number of scenes */
} SubGhzSceneId;

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

typedef enum {
    SubGhzReadRawStateIdle = 0,
    SubGhzReadRawStateRecording,
    SubGhzReadRawStateStopped,
} SubGhzReadRawState;

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

    /* --- Radio config --- */
    uint8_t  freq_idx;                /**< Index into frequency presets */
    uint8_t  mod_idx;                 /**< Index into modulation presets */
    bool     hopping;                 /**< Frequency hopping enabled */
    bool     sound;                   /**< Beep on decode */
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

    /* --- Save flow --- */
    char     file_path[64];           /**< Current file path for save */
    char     file_name[32];           /**< Suggested filename */

    /* --- Computed / derived --- */
    uint32_t current_freq_hz;         /**< Active frequency in Hz */

    /* --- Playlist state --- */
    char     playlist_path[64];       /**< Path to current .txt playlist */
    char     playlist_files[16][64];  /**< Up to 16 .sub file paths */
    uint8_t  playlist_count;          /**< Number of entries loaded */
    uint8_t  playlist_current;        /**< Index of file being transmitted */
    uint8_t  playlist_repeat_total;   /**< Total repeats (0 = infinite) */
    uint8_t  playlist_repeat_done;    /**< Completed repeat passes */
    bool     playlist_running;        /**< Playback active */

    /* --- Flags --- */
    bool     need_redraw;             /**< Scene requests display update */
    bool     running;                 /**< false = exit scene manager loop */
    bool     resume_from_child;       /**< Set when Read pushes a child scene;
                                           cleared after resume_rx() uses it */
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

#endif /* M1_SUBGHZ_SCENE_H_ */
