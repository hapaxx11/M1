/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_read_raw.c
 * @brief  Sub-GHz Read Raw Scene — raw RF capture with RSSI waveform.
 *
 * Momentum-aligned state machine and button layout.
 *
 * States: START (passive listen, radio on) → RECORDING (SD file open, TIM1 armed)
 *         → IDLE (capture on SD) → SENDING (blocking TX)
 *         START is also re-entered after Erase.
 *         LOADED (pre-existing file from Saved browser) → SENDING (blocking TX)
 *
 * On fresh entry the scene enters START state with the radio in passive listen.
 * Pressing OK begins recording (opens the SD file, arms TIM1 ISR).
 * Pressing OK again stops recording and enters IDLE (Erase/Send/Save).
 * If recording init fails (OOM or SD error) the scene stays in START so
 * the user can configure and retry.
 *
 * Button mapping (matches physical D-pad left/OK/right):
 *   START:     LEFT = Config  |  OK = ⊙ REC  |  RIGHT = (none)
 *   RECORDING: LEFT = Config  |  OK = ⊙ Stop  |  (none)
 *   IDLE:      LEFT = Erase   |  OK = ⊙ Send  |  RIGHT = Save
 *   LOADED:    LEFT = New     |  OK = ⊙ Send  |  RIGHT = (none)
 *   SENDING:   (none)         |  (none)        |  (none)   [TX blocking; returns to IDLE or LOADED]
 *
 *   BACK = Exit (START/IDLE/LOADED) or Stop recording (RECORDING → IDLE)
 *
 * Display:
 *   START:     RSSI spectrogram.  Cursor advances (trace=true) when RSSI is above
 *              the noise threshold; when signal drops away a short debounce window
 *              (RAW_DEBOUNCE_MAX × 200ms) of low-RSSI columns is still pushed so
 *              that successive signal bursts are visually separated by a gap — the
 *              same behaviour as Momentum Read Raw.  After debounce expires the
 *              cursor freezes and only the live RSSI indicator is refreshed.
 *   RECORDING: Cursor slides right as captured edges arrive (SubGhzEventRxData).
 *              When the signal ends, the same debounce mechanism inserts gap
 *              columns between successive bursts within a single recording session.
 *              The raw_rx_pending flag prevents the periodic draw tick from adding
 *              a duplicate column while the signal is still active.
 *   IDLE:      Spectrogram frozen; "X spl." sample count, filename in waveform area.
 *              LEFT = Erase, ⊙ OK = Send (replay), RIGHT = Save (rename via VKB).
 *   LOADED:    Spectrogram reset (empty); "RAW" status, filename in waveform area.
 *              LEFT = New (discard loaded file, start fresh), ⊙ OK = Send (replay).
 *              This is Momentum's LoadKeyIDLE state — entered when a pre-recorded
 *              RAW file is opened from the Saved browser.
 *   SENDING:   Spectrogram frozen; "TX..." label in waveform area; no button bar.
 *              Entered from either IDLE or LOADED just before the blocking replay;
 *              draw() is forced once so the display shows the TX indicator for the
 *              entire duration of the blocking send.  Returns to whichever state
 *              triggered the send (IDLE → IDLE, LOADED → LOADED).
 *              This matches Momentum's TX/TXRepeat and LoadKeyTX/LoadKeyTXRepeat
 *              state concepts, adapted for M1's blocking-TX architecture.
 *
 *   scene_on_enter → START state (passive listen, radio on, no file open).
 *   OK → RECORDING (opens SD file, arms TIM1 ISR).
 *   OK → IDLE (Erase / Send / Save).
 *   scene_on_exit → radio fully stopped.
 *
 * Recording data path:
 *   ISR → ring buffer → sub_ghz_rx_raw_save() → SD card (.sub file)
 *   Data is streamed to SD during recording.  After stop the file exists
 *   on SD and can be replayed (Send) or renamed (Save).
 *   The saved file is Flipper-compatible (RAW_Data: signed integers).
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_tasks.h"
#include "m1_system.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_lp5814.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_ring_buffer.h"
#include "m1_storage.h"
#include "m1_sdcard_man.h"
#include "m1_virtual_kb.h"
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"
#include "ff.h"

extern const char *subghz_freq_labels[];
extern const char *subghz_mod_labels[];
extern void subghz_apply_config_ext(uint8_t freq_idx, uint8_t mod_idx);
extern void sub_ghz_rx_init_ext(void);
extern void sub_ghz_rx_start_ext(void);
extern void sub_ghz_rx_pause_ext(void);
extern void sub_ghz_rx_deinit_ext(void);
extern void sub_ghz_set_opmode_ext(uint8_t opmode, uint8_t band, uint8_t channel, uint8_t tx_power);
extern uint8_t sub_ghz_ring_buffers_init_ext(void);
extern void sub_ghz_ring_buffers_deinit_ext(void);
extern void sub_ghz_tx_raw_deinit_ext(void);
extern S_M1_SubGHz_Scan_Config subghz_scan_config;
extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern uint8_t subghz_record_mode_flag;
extern S_M1_RingBuffer subghz_rx_rawdata_rb;
extern void SI446x_Change_Modem_OOK_PDTC(uint8_t value);
extern int16_t subghz_read_rssi_ext(void);
extern int8_t  subghz_get_rssi_threshold_ext(void);

/* RAW RSSI visualization helpers from m1_sub_ghz.c */
extern void subghz_raw_rssi_draw_ext(void);
extern void subghz_raw_rssi_reset_ext(void);
extern void subghz_raw_rssi_push_ext(float rssi_dbm, bool trace);
extern void subghz_raw_rssi_set_current_ext(float rssi_dbm);
extern void subghz_raw_draw_frame_ext(void);

/* Raw recording file management from m1_sub_ghz.c */
extern uint8_t  sub_ghz_raw_recording_init_ext(void);
extern uint32_t sub_ghz_raw_recording_flush_ext(void);
extern void     sub_ghz_raw_recording_stop_ext(void);
extern const char *sub_ghz_raw_recording_get_filename_ext(void);
extern uint32_t sub_ghz_raw_recording_get_total_samples_ext(void);

/* Path to last recorded file — kept for Send/Save/Erase after recording.
 * Filesystem operations prepend "0:" to build FatFS paths, so keep
 * raw_filepath short enough that "0:" + path fits in the derived buffers. */
#define RAW_FILEPATH_MAX  70   /* 70 chars + NUL = 71 bytes */

/* Number of 200ms draw ticks spent pushing low-RSSI "gap" columns after the
 * signal drops below threshold.  This creates visible empty space between
 * successive signal bursts in the waveform, matching Momentum Read Raw. */
#define RAW_DEBOUNCE_MAX  4    /* ~800 ms of gap columns */

/* RSSI threshold for cursor advancement in the Start state.
 * Uses the user-configurable value (subghz_get_rssi_threshold_ext()) so the
 * cursor starts scrolling at exactly the moment the signal crosses the
 * horizontal dashed line that is drawn at the same threshold. */
static char raw_filepath[RAW_FILEPATH_MAX + 1];

/* Forward declarations — these functions are defined after scene_on_enter() but
 * called from it.  Without these, the compiler creates implicit non-static
 * declarations that conflict with the actual static definitions. */
static void draw(SubGhzApp *app);
static void start_raw_rx(SubGhzApp *app);

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/** Extract bare filename (with extension, without directory path) from a full path.
 *  Handles both '/' and '\\' separators. */
static const char *extract_filename(const char *fullpath)
{
    const char *fwd = strrchr(fullpath, '/');
    const char *bck = strrchr(fullpath, '\\');
    const char *p = (fwd && bck) ? (fwd > bck ? fwd : bck)
                  : (fwd ? fwd : bck);
    return p ? p + 1 : fullpath;
}

/**
 * Start the radio in passive listen mode (no recording).
 * Called from scene_on_enter() and from the Send path after replay completes.
 * subghz_record_mode_flag stays 0 — ISR edges are NOT written to the ring buffer.
 *
 * TIM1 input-capture is NOT started here.  Starting it in passive mode would
 * cause the ISR to enqueue a Q_EVENT_SUBGHZ_RX for every noise edge (thousands/sec
 * at 433 MHz), flooding the main queue and preventing the 200 ms timeout that
 * drives RSSI refreshes and the sine-wave animation.  The timer is deferred to
 * start_raw_rx() so interrupts only fire when recording is actually in progress.
 */
static void start_passive_rx(SubGhzApp *app)
{
    sub_ghz_tx_raw_deinit_ext();
    subghz_apply_config_ext(app->freq_idx, app->mod_idx);
    menu_sub_ghz_init();
    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE;
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_RX, subghz_scan_config.band, 0, 0);
    SI446x_Change_Modem_OOK_PDTC(0x6C);
    sub_ghz_rx_init_ext();
    /* TIM1 ISR deliberately NOT started here — deferred to start_raw_rx(). */
    /* subghz_record_mode_flag remains 0 */
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    /* Detect whether we're returning from a child scene (Config) vs. entering
     * fresh from the Menu.  The flag is set by event handlers before they push
     * a child scene; it ensures we don't reset state on return.
     * Consume the flag immediately so it does not persist to future entries. */
    bool is_resume = app->resume_from_child;
    app->resume_from_child = false;

    /* ------------------------------------------------------------------ */
    /* Resume path: returning from a child scene (Config).                 */
    /* Preserve raw_state and raw_filepath — do NOT reset or auto-restart. */
    /* Just bring the radio back up in passive listen and leave state as-is.*/
    /* ------------------------------------------------------------------ */
    if (is_resume)
    {
        app->raw_debounce    = 0;
        app->raw_rx_pending  = false;
        app->rssi            = -120;

        /* Restart passive RX (radio was torn down by scene_on_exit when the
         * child scene was pushed).  State and filepath are preserved.
         * Stay in whatever state we left — the user presses OK to start
         * recording if we returned to START, or has a capture to work with
         * if we returned to IDLE/LOADED. */
        start_passive_rx(app);

        app->need_redraw = true;
        return;
    }

    /* ------------------------------------------------------------------ */
    /* Fresh entry path.                                                   */
    /* ------------------------------------------------------------------ */
    app->raw_sample_count = 0;
    app->raw_debounce = 0;
    app->raw_rx_pending = false;
    app->rssi = -120;
    subghz_raw_rssi_reset_ext();

    /* If the Saved scene pre-loaded a file path, enter in Loaded state.
     * This is Momentum's LoadKeyIDLE path — the user opened a pre-recorded
     * RAW file from the Saved browser and wants to review / replay it.
     * raw_load_path is consumed here and cleared so subsequent scene
     * entries start fresh. */
    if (app->raw_load_path[0] != '\0')
    {
        strncpy(raw_filepath, app->raw_load_path, RAW_FILEPATH_MAX);
        raw_filepath[RAW_FILEPATH_MAX] = '\0';
        app->raw_load_path[0] = '\0';
        /* Keep raw_load_is_native / raw_load_freq_hz / raw_load_mod — consumed by TX handler */
        app->raw_state = SubGhzReadRawStateLoaded;
        /* Radio in passive listen for the Loaded state (statusbar shows live freq/mod) */
        start_passive_rx(app);
    }
    else
    {
        app->raw_state = SubGhzReadRawStateStart;
        raw_filepath[0] = '\0';
        app->raw_load_is_native = false;
        app->raw_load_freq_hz = 0;
        app->raw_load_mod = 0;

        /* START state: radio in passive listen.  User presses OK to begin
         * recording.  start_raw_rx() is called from the OK handler. */
        start_passive_rx(app);
    }

    app->need_redraw = true;
}

/**
 * Arm recording — radio is already initialised by start_passive_rx().
 * On success sets raw_state = RECORDING.  On failure (OOM / SD error)
 * returns early without changing raw_state (stays as Start).
 */
static void start_raw_rx(SubGhzApp *app)
{
    /* Allocate ring buffers for edge capture */
    if (sub_ghz_ring_buffers_init_ext())
        return;  /* OOM — stay in Start, radio keeps listening passively */

    /* Create the output .sub file and write the Flipper-compatible header */
    if (sub_ghz_raw_recording_init_ext())
    {
        sub_ghz_ring_buffers_deinit_ext();
        return;  /* SD error — stay in Start, radio keeps listening passively */
    }

    /* Arm ISR: start TIM1 input-capture then flag ring-buffer writes */
    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE;
    sub_ghz_rx_start_ext();
    subghz_record_mode_flag = 1;

    app->raw_state = SubGhzReadRawStateRecording;
    app->raw_sample_count = 0;
    app->raw_debounce = 0;
    app->raw_rx_pending = false;
    subghz_raw_rssi_reset_ext();

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
}

/**
 * Stop recording — flush to SD and return to passive listen (Idle).
 * The radio IC stays on so RSSI is live in the Idle state.
 */
static void stop_raw_rx(SubGhzApp *app)
{
    /* Pause edge-capture timer to prevent a race condition during the flush */
    sub_ghz_rx_pause_ext();
    subghz_record_mode_flag = 0;

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);

    /* Flush remaining ring buffer data to SD and close the file */
    sub_ghz_raw_recording_stop_ext();

    /* Free ring buffers — reclaim ~128 KB of RAM */
    sub_ghz_ring_buffers_deinit_ext();

    app->raw_sample_count = sub_ghz_raw_recording_get_total_samples_ext();

    /* Remember the auto-generated filepath for Send / Save / Erase */
    const char *fname = sub_ghz_raw_recording_get_filename_ext();
    strncpy(raw_filepath, fname ? fname : "", RAW_FILEPATH_MAX);
    raw_filepath[RAW_FILEPATH_MAX] = '\0';

    app->raw_state = SubGhzReadRawStateIdle;

    app->raw_debounce = 0;
    app->raw_rx_pending = false;

    /* Timer remains paused.  Passive listen does not run TIM1 so the queue
     * is not flooded by noise edges while in the Idle state. */
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            if (app->raw_state == SubGhzReadRawStateRecording)
            {
                /* Stop recording and keep capture (Momentum: BACK = stop, not exit) */
                stop_raw_rx(app);
                app->need_redraw = true;
            }
            else
            {
                /* Exit scene */
                subghz_scene_pop(app);
            }
            return true;

        case SubGhzEventOk:
            if (app->raw_state == SubGhzReadRawStateStart)
            {
                /* Start recording — fast because radio is already listening */
                start_raw_rx(app);
                app->need_redraw = true;
            }
            else if (app->raw_state == SubGhzReadRawStateRecording)
            {
                /* Stop recording — transition to Idle (capture on SD) */
                stop_raw_rx(app);
                app->need_redraw = true;
            }
            else if (app->raw_state == SubGhzReadRawStateIdle ||
                     app->raw_state == SubGhzReadRawStateLoaded)
            {
                /* Send — replay the raw file (blocking delegate).
                 *
                 * Works for both freshly-recorded (Idle) and pre-loaded (Loaded)
                 * files.  After TX, returns to whichever state triggered the send:
                 *   Idle   → Idle   (Momentum: TX   → IDLE)
                 *   Loaded → Loaded (Momentum: LoadKeyTX → LoadKeyIDLE)
                 *
                 * Transition to Sending state and force a draw() call so the
                 * display shows the TX indicator BEFORE the blocking replay call.
                 * This is the M1 analogue of Momentum's TX/TXRepeat and
                 * LoadKeyTX/LoadKeyTXRepeat states — the difference is that M1's
                 * TX is blocking so there is no animation, but the correct UI
                 * state (no action buttons, "TX..." label) is shown for the
                 * entire duration of the send.
                 *
                 * The replay path (sub_ghz_replay_flipper_file) uses TIM1 for
                 * TX PWM, which conflicts with the RX input-capture on TIM1 CH1.
                 * In Idle/Loaded state the timer is already paused (stop_raw_rx()
                 * left it in READY state, or it was never started in Loaded state),
                 * so only de-init is needed before replay. */
                bool from_loaded = (app->raw_state == SubGhzReadRawStateLoaded);
                if (raw_filepath[0] != '\0')
                {
                    app->raw_state = SubGhzReadRawStateSending;
                    draw(app);  /* Force one frame showing TX indicator before blocking */

                    sub_ghz_rx_deinit_ext();

                    uint8_t ret;
                    if (from_loaded && app->raw_load_is_native && app->raw_load_freq_hz != 0)
                    {
                        /* M1 native .sgh file — use direct streaming replay */
                        ret = sub_ghz_replay_datafile(raw_filepath,
                                                      app->raw_load_freq_hz,
                                                      app->raw_load_mod);
                    }
                    else
                    {
                        /* Flipper-format .sub RAW file or freshly-recorded capture */
                        ret = sub_ghz_replay_flipper_file(raw_filepath);
                    }

                    /* Restart passive RX using the user-selected config.
                     * start_passive_rx() re-applies app->freq_idx/mod_idx so the
                     * scene returns to the correct band/modulation even if replay
                     * mutated subghz_scan_config for a CUSTOM band. */
                    start_passive_rx(app);

                    /* TX complete — return to the originating state */
                    app->raw_state = from_loaded ? SubGhzReadRawStateLoaded
                                                 : SubGhzReadRawStateIdle;

                    if (ret != 0)
                    {
                        char err_buf[32];
                        const char *err = err_buf;
                        snprintf(err_buf, sizeof(err_buf), "Error code: %u", (unsigned)ret);
                        switch (ret)
                        {
                            case 1: err = "File/IO error";              break;
                            case 2: err = "Missing data/frequency";     break;
                            case 3: err = "Unsupported freq";           break;
                            case 4: /* fall through */
                            case 5: err = "Memory error";               break;
                            case 6: err = "Dynamic/RAW-only protocol";  break;
                            case 7: err = "Unsupported protocol";       break;
                        }
                        m1_message_box(&m1_u8g2, "Send failed", err, "",
                                       "BACK to return");
                    }
                }
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventLeft:
            if (app->raw_state == SubGhzReadRawStateStart)
            {
                /* Config — scene_on_exit tears down RX; scene_on_enter re-inits
                 * on return via the resume path.  User presses OK to start
                 * recording on the newly-configured frequency. */
                app->resume_from_child = true;
                subghz_scene_push(app, SubGhzSceneConfig);
            }
            else if (app->raw_state == SubGhzReadRawStateRecording)
            {
                /* Config during recording: stop the current capture (→ Idle),
                 * then push Config.  On return, scene_on_enter sees is_resume=true
                 * and raw_state=Idle, so it restarts passive RX and preserves
                 * the capture — the user can Send/Save/Erase it. */
                stop_raw_rx(app);
                app->resume_from_child = true;
                subghz_scene_push(app, SubGhzSceneConfig);
                app->need_redraw = true;
            }
            else if (app->raw_state == SubGhzReadRawStateIdle)
            {
                /* Erase — delete the recorded file and return to Start */
                if (raw_filepath[0] != '\0')
                {
                    char del_path[RAW_FILEPATH_MAX + 3]; /* "0:" + path + NUL */
                    snprintf(del_path, sizeof(del_path), "0:%s", raw_filepath);
                    FRESULT fres = f_unlink(del_path);
                    if (fres != FR_OK && fres != FR_NO_FILE)
                    {
                        /* Unlink failed — keep Idle state so user can retry */
                        m1_message_box(&m1_u8g2, "Erase failed",
                                       "Could not delete file", "",
                                       "BACK to return");
                        app->need_redraw = true;
                        return true;
                    }
                    raw_filepath[0] = '\0';
                }
                subghz_raw_rssi_reset_ext();
                app->raw_state = SubGhzReadRawStateStart;
                app->raw_sample_count = 0;
                app->raw_debounce = 0;
                app->raw_rx_pending = false;
                app->need_redraw = true;
            }
            else if (app->raw_state == SubGhzReadRawStateLoaded)
            {
                /* New — discard the loaded file (without deleting from SD) and
                 * return to Start so the user can make a fresh recording.
                 * This mirrors Momentum's "New" action in LoadKeyIDLE state. */
                raw_filepath[0] = '\0';
                subghz_raw_rssi_reset_ext();
                app->raw_state = SubGhzReadRawStateStart;
                app->raw_sample_count = 0;
                app->raw_debounce = 0;
                app->raw_rx_pending = false;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventRight:
            if (app->raw_state == SubGhzReadRawStateIdle && raw_filepath[0] != '\0')
            {
                /* Save — rename the auto-generated file via VKB */
                const char *fname = extract_filename(raw_filepath);
                char base[32];
                strncpy(base, fname, sizeof(base) - 1);
                base[sizeof(base) - 1] = '\0';
                char *dot = strrchr(base, '.');
                if (dot) *dot = '\0';

                char new_name[32];
                if (m1_vkb_get_filename("Save RAW as:", base, new_name))
                {
                    /* Preserve the original extension (.sub) */
                    const char *ext = strrchr(fname, '.');
                    if (!ext) ext = ".sub";

                    char old_path[RAW_FILEPATH_MAX + 3], new_path[RAW_FILEPATH_MAX + 3];
                    snprintf(old_path, sizeof(old_path), "0:%s", raw_filepath);
                    snprintf(new_path, sizeof(new_path), "0:/SUBGHZ/%s%s", new_name, ext);

                    FRESULT rename_result = f_rename(old_path, new_path);
                    if (rename_result == FR_OK)
                    {
                        /* Update stored path to the new name */
                        snprintf(raw_filepath, sizeof(raw_filepath),
                                 "/SUBGHZ/%s%s", new_name, ext);
                    }
                    else
                    {
                        /* Leave raw_filepath unchanged so the current capture remains valid. */
                        printf("Save RAW rename failed (FRESULT=%d): '%s' -> '%s'\n",
                               (int)rename_result, old_path, new_path);
                    }
                }
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventRxData:
            if (app->raw_state == SubGhzReadRawStateRecording)
            {
                /* Flush ring buffer data to SD card */
                uint32_t flushed = sub_ghz_raw_recording_flush_ext();
                if (flushed > 0)
                    app->raw_sample_count = sub_ghz_raw_recording_get_total_samples_ext();

                /* Read RSSI and push to history (trace=true advances cursor) */
                app->rssi = subghz_read_rssi_ext();
                subghz_raw_rssi_push_ext((float)app->rssi, true);

                /* Signal is active: reset the debounce gap timer and mark
                 * raw_rx_pending so the next draw() tick does not push an
                 * additional column on top of what we just pushed here. */
                app->raw_debounce = RAW_DEBOUNCE_MAX;
                app->raw_rx_pending = true;

                app->need_redraw = true;
            }
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    if (app->raw_state == SubGhzReadRawStateRecording)
    {
        /* Recording was in progress — use the normal stop path so all
         * recording side effects are cleaned up, including LED blink state.
         * stop_raw_rx() pauses the timer (without restarting it) and flushes
         * the file; the teardown below then de-initialises the timer. */
        stop_raw_rx(app);
    }
    /* In Start or Idle: TIM1 is either never started (Start fallback) or
     * already paused by stop_raw_rx() (Idle).  sub_ghz_rx_deinit_ext()
     * checks the timer state before de-initialising and is safe either way. */

    /* Full radio teardown — powers off SI4463 and resets STM32 timer */
    sub_ghz_rx_deinit_ext();
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_ISOLATED, subghz_scan_config.band, 0, 0);
    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_IDLE;
}

static void draw(SubGhzApp *app)
{
    m1_u8g2_firstpage();

    /* Status bar: freq (left), mod (center), state+samples (right) */
    const char *freq = subghz_freq_labels ? subghz_freq_labels[app->freq_idx] : "???";
    const char *mod  = subghz_mod_labels  ? subghz_mod_labels[app->mod_idx]   : "???";

    /* Build the right-side status string:
     *   START:     "RAW"
     *   LOADED:    "RAW"
     *   RECORDING: "N spl."  (full integer, trailing period — matches Momentum)
     *   IDLE:      "N spl."  (frozen)
     *   SENDING:   "N spl."  (frozen, TX in progress) */
    const char *right_status = NULL;
    char right_buf[16];

    switch (app->raw_state)
    {
        case SubGhzReadRawStateStart:
        case SubGhzReadRawStateLoaded:
            right_status = "RAW";
            break;
        case SubGhzReadRawStateRecording:
            snprintf(right_buf, sizeof(right_buf), "%lu spl.",
                     (unsigned long)app->raw_sample_count);
            right_status = right_buf;
            break;
        case SubGhzReadRawStateIdle:
        case SubGhzReadRawStateSending:
            snprintf(right_buf, sizeof(right_buf), "%lu spl.",
                     (unsigned long)app->raw_sample_count);
            right_status = right_buf;
            break;
    }

    subghz_status_bar_draw(freq, mod, right_status, false);

    /* Waveform area frame — always visible */
    subghz_raw_draw_frame_ext();

    /* Live RSSI update per draw tick.
     *
     * Start:     Advance cursor (trace=true) when signal exceeds threshold.
     *            After signal drops, a debounce window of RAW_DEBOUNCE_MAX
     *            ticks (each ~200ms) pushes low-RSSI columns with trace=true
     *            to create a visible gap before the cursor freezes.  This
     *            matches Momentum's visual separation between signal bursts.
     * Recording: RxData events push columns with trace=true (cursor advances).
     *            raw_rx_pending prevents the draw tick from adding a duplicate
     *            column while the signal is active.  When RxData stops arriving
     *            the same debounce mechanism inserts gap columns for
     *            RAW_DEBOUNCE_MAX ticks; after that, the trace no longer
     *            advances and the last committed column continues to be
     *            updated with the live RSSI value.
     */
    if (app->raw_state == SubGhzReadRawStateStart)
    {
        app->rssi = subghz_read_rssi_ext();
        bool signal_present = ((float)app->rssi > (float)subghz_get_rssi_threshold_ext());
        if (signal_present)
        {
            /* Signal above threshold — push bar and keep debounce primed */
            app->raw_debounce = RAW_DEBOUNCE_MAX;
            subghz_raw_rssi_push_ext((float)app->rssi, true);
        }
        else if (app->raw_debounce > 0)
        {
            /* Signal just ended — push low-RSSI gap column then count down */
            app->raw_debounce--;
            subghz_raw_rssi_push_ext((float)app->rssi, true);
        }
        else
        {
            /* Silence: update cursor indicator only, preserve history */
            subghz_raw_rssi_set_current_ext((float)app->rssi);
        }
    }
    else if (app->raw_state == SubGhzReadRawStateRecording)
    {
        app->rssi = subghz_read_rssi_ext();
        bool signal_present = ((float)app->rssi > (float)subghz_get_rssi_threshold_ext());
        if (app->raw_rx_pending)
        {
            /* RxData fired since last draw — signal is active.
             * The event handler already pushed a column; just refresh cursor and
             * keep debounce primed for the eventual end-of-burst gap. */
            app->raw_rx_pending = false;
            app->raw_debounce = RAW_DEBOUNCE_MAX;
            subghz_raw_rssi_set_current_ext((float)app->rssi);
        }
        else if (signal_present)
        {
            /* No RxData batch yet, but RSSI is still above threshold.
             * Hold debounce so slow/long bursts are not split by synthetic gaps. */
            app->raw_debounce = RAW_DEBOUNCE_MAX;
            subghz_raw_rssi_set_current_ext((float)app->rssi);
        }
        else if (app->raw_debounce > 0)
        {
            /* RSSI dropped below threshold and no RxData arrived — signal ended.
             * Push gap column then count down debounce. */
            app->raw_debounce--;
            subghz_raw_rssi_push_ext((float)app->rssi, true);
        }
        else
        {
            /* No signal — refresh cursor display without advancing */
            subghz_raw_rssi_push_ext((float)app->rssi, false);
        }
    }

    /* Draw the RSSI spectrogram for all states.
     * Start:     Scrolling live ambient RSSI (built above).
     * Recording: Captured edges with cursor advancing on RxData.
     * Idle:      Frozen spectrogram of the completed capture. */
    subghz_raw_rssi_draw_ext();

    /* Text overlay in the waveform area.
     * Idle/Loaded: Filename centered (so the user knows which capture they have).
     * Sending:     "TX..." centered (Momentum TX state equivalent — no action buttons). */
    if (app->raw_state == SubGhzReadRawStateSending)
    {
        const char *lbl = "TX...";
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        uint8_t tw = u8g2_GetStrWidth(&m1_u8g2, lbl);
        uint8_t fx = (tw < 115) ? (115 - tw) / 2 : 0;
        u8g2_DrawStr(&m1_u8g2, fx, 35, lbl);
    }
    else if ((app->raw_state == SubGhzReadRawStateIdle ||
              app->raw_state == SubGhzReadRawStateLoaded) && raw_filepath[0] != '\0')
    {
        const char *fname = extract_filename(raw_filepath);
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        /* Center horizontally within waveform box (x=0..115) and
         * vertically (frame y=14..48, midpoint y=31, baseline ~y=35) */
        uint8_t tw = u8g2_GetStrWidth(&m1_u8g2, fname);
        uint8_t fx = (tw < 115) ? (115 - tw) / 2 : 0;
        u8g2_DrawStr(&m1_u8g2, fx, 35, fname);
    }

    /* Bottom bar — columns match physical buttons: LEFT | OK | RIGHT */
    switch (app->raw_state)
    {
        case SubGhzReadRawStateStart:
            subghz_button_bar_draw(
                arrowleft_8x8, "Config",
                ok_circle_8x8, "REC",
                NULL, NULL);
            break;
        case SubGhzReadRawStateRecording:
            subghz_button_bar_draw(
                arrowleft_8x8, "Config",
                ok_circle_8x8, "Stop",
                NULL, NULL);
            break;
        case SubGhzReadRawStateIdle:
            subghz_button_bar_draw(
                arrowleft_8x8, "Erase",
                ok_circle_8x8, "Send",
                arrowright_8x8, "Save");
            break;
        case SubGhzReadRawStateLoaded:
            /* Momentum's LoadKeyIDLE: "New" = start fresh, "Send" = replay loaded file.
             * No right button (M1 has no MoreRAW scene). */
            subghz_button_bar_draw(
                arrowleft_8x8, "New",
                ok_circle_8x8, "Send",
                NULL, NULL);
            break;
        case SubGhzReadRawStateSending:
            /* No button bar during blocking TX — matches Momentum's TX state which
             * hides all action buttons and shows only the sine wave animation. */
            break;
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_read_raw_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
