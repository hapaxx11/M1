/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_read_raw.c
 * @brief  Sub-GHz Read Raw Scene — raw RF capture with RSSI waveform.
 *
 * Momentum-aligned state machine and button layout.
 *
 * States: START → RECORDING → IDLE (capture on SD)
 *
 * Button mapping (matches physical D-pad left/OK/right):
 *   START:     LEFT = Config  |  OK = REC    |  RIGHT = (none)
 *   RECORDING: (none)         |  OK = Stop   |  (none)
 *   IDLE:      LEFT = Erase   |  OK = Send   |  RIGHT = Save
 *
 *   BACK = Exit (START/IDLE) or Stop recording (RECORDING)
 *
 * Recording data path:
 *   ISR → ring buffer → sub_ghz_rx_raw_save_ext() → SD card (.sgh file)
 *   Data is streamed to SD during recording.  After stop the file exists
 *   on SD and can be replayed (Send) or renamed (Save).
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
#include "m1_file_browser.h"
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

/* RAW RSSI visualization helpers from m1_sub_ghz.c */
extern void subghz_raw_rssi_draw_ext(void);
extern void subghz_raw_rssi_reset_ext(void);
extern void subghz_raw_rssi_push_ext(float rssi_dbm, bool trace);
extern void subghz_raw_draw_frame_ext(void);
extern void subghz_raw_draw_sin_ext(void);
extern void subghz_raw_sin_advance_ext(void);

/* Raw recording file management from m1_sub_ghz.c */
extern uint8_t  sub_ghz_raw_recording_init_ext(void);
extern uint32_t sub_ghz_raw_recording_flush_ext(void);
extern void     sub_ghz_raw_recording_stop_ext(void);
extern const char *sub_ghz_raw_recording_get_filename_ext(void);
extern uint32_t sub_ghz_raw_recording_get_total_samples_ext(void);

/* Path to last recorded file — kept for Send/Save/Erase after recording */
static char raw_filepath[72];

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/** Extract bare filename (without path or extension) from a full path.
 *  Handles both '/' and '\\' separators. */
static const char *extract_filename(const char *fullpath)
{
    const char *fwd = strrchr(fullpath, '/');
    const char *bck = strrchr(fullpath, '\\');
    const char *p = (fwd && bck) ? (fwd > bck ? fwd : bck)
                  : (fwd ? fwd : bck);
    return p ? p + 1 : fullpath;
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    app->raw_state = SubGhzReadRawStateStart;
    app->raw_sample_count = 0;
    app->rssi = -120;
    raw_filepath[0] = '\0';
    subghz_raw_rssi_reset_ext();
    app->need_redraw = true;
}

static void start_raw_rx(SubGhzApp *app)
{
    /* Clean up any previous TX raw state BEFORE radio RX setup.
     * sub_ghz_tx_raw_deinit puts the radio to SLEEP via
     * sub_ghz_set_opmode(ISOLATED) — if called AFTER starting RX it
     * would undo the RX start, leaving TIM1 capturing zero edges. */
    sub_ghz_tx_raw_deinit_ext();

    subghz_apply_config_ext(app->freq_idx, app->mod_idx);

    /* Ensure radio is in a clean, known state — matches the Spectrum
     * Analyzer pattern of always doing a full radio reset + config load
     * before starting RX.  This guarantees the SI4463 recovers properly
     * even if a previous blocking delegate powered the radio off via
     * menu_sub_ghz_exit(). */
    menu_sub_ghz_init();

    /* Initialize ring buffers */
    if (sub_ghz_ring_buffers_init_ext())
    {
        /* Memory error — stay in Start */
        return;
    }

    /* Initialize SD card file and write header */
    if (sub_ghz_raw_recording_init_ext())
    {
        /* SD card error — clean up and stay in Start */
        sub_ghz_ring_buffers_deinit_ext();
        return;
    }

    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE;
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_RX, subghz_scan_config.band, 0, 0);
    SI446x_Change_Modem_OOK_PDTC(0x6C);
    sub_ghz_rx_init_ext();
    sub_ghz_rx_start_ext();

    subghz_record_mode_flag = 1;
    app->raw_state = SubGhzReadRawStateRecording;
    app->raw_sample_count = 0;
    subghz_raw_rssi_reset_ext();

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
}

static void stop_raw_rx(SubGhzApp *app)
{
    sub_ghz_rx_pause_ext();
    subghz_record_mode_flag = 0;

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);

    /* Flush remaining ring buffer data to SD and close the file */
    sub_ghz_raw_recording_stop_ext();

    sub_ghz_rx_deinit_ext();
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_ISOLATED, subghz_scan_config.band, 0, 0);
    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_IDLE;

    app->raw_sample_count = sub_ghz_raw_recording_get_total_samples_ext();

    /* Remember the auto-generated filepath for Send/Save/Erase */
    const char *fname = sub_ghz_raw_recording_get_filename_ext();
    strncpy(raw_filepath, fname ? fname : "", sizeof(raw_filepath) - 1);
    raw_filepath[sizeof(raw_filepath) - 1] = '\0';

    app->raw_state = SubGhzReadRawStateIdle;
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
                /* Exit scene — clean up ring buffers if capture exists */
                if (app->raw_state == SubGhzReadRawStateIdle)
                    sub_ghz_ring_buffers_deinit_ext();
                subghz_scene_pop(app);
            }
            return true;

        case SubGhzEventOk:
            if (app->raw_state == SubGhzReadRawStateStart)
            {
                /* Start recording */
                start_raw_rx(app);
                app->need_redraw = true;
            }
            else if (app->raw_state == SubGhzReadRawStateRecording)
            {
                /* Stop recording — transition to Idle (capture on SD) */
                stop_raw_rx(app);
                app->need_redraw = true;
            }
            else if (app->raw_state == SubGhzReadRawStateIdle)
            {
                /* Send — replay the captured raw file (blocking delegate).
                 * sub_ghz_replay_flipper_file() calls menu_sub_ghz_exit()
                 * internally, so we MUST call menu_sub_ghz_init() after —
                 * unconditionally, even on failure — to restore radio state. */
                if (raw_filepath[0] != '\0')
                {
                    sub_ghz_replay_flipper_file(raw_filepath);
                    menu_sub_ghz_init();
                }
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventLeft:
            if (app->raw_state == SubGhzReadRawStateStart)
            {
                /* Config */
                subghz_scene_push(app, SubGhzSceneConfig);
            }
            else if (app->raw_state == SubGhzReadRawStateIdle)
            {
                /* Erase — delete the recorded file and go back to Start */
                if (raw_filepath[0] != '\0')
                {
                    char del_path[72];
                    snprintf(del_path, sizeof(del_path), "0:%s", raw_filepath);
                    f_unlink(del_path);
                    raw_filepath[0] = '\0';
                }
                sub_ghz_ring_buffers_deinit_ext();
                subghz_raw_rssi_reset_ext();
                app->raw_state = SubGhzReadRawStateStart;
                app->raw_sample_count = 0;
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
                    /* Determine extension from original */
                    const char *ext = strrchr(fname, '.');
                    if (!ext) ext = ".sgh";

                    char old_path[72], new_path[72];
                    snprintf(old_path, sizeof(old_path), "0:%s", raw_filepath);
                    snprintf(new_path, sizeof(new_path), "0:/SUBGHZ/%s%s", new_name, ext);
                    if (f_rename(old_path, new_path) == FR_OK)
                    {
                        /* Update stored path to the new name */
                        snprintf(raw_filepath, sizeof(raw_filepath),
                                 "/SUBGHZ/%s%s", new_name, ext);
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
        stop_raw_rx(app);
}

static void draw(SubGhzApp *app)
{
    char line[32];

    m1_u8g2_firstpage();

    /* Status bar: freq (left), mod (center), state+samples (right) */
    const char *freq = subghz_freq_labels ? subghz_freq_labels[app->freq_idx] : "???";
    const char *mod  = subghz_mod_labels  ? subghz_mod_labels[app->mod_idx]   : "???";

    /* Build the right-side status string:
     *   START:     "RAW"
     *   RECORDING: "REC Xk"  (compact state + sample count)
     *   IDLE:      "Xk spl"  (sample count only) */
    const char *right_status = NULL;
    char right_buf[16];

    switch (app->raw_state)
    {
        case SubGhzReadRawStateStart:
            right_status = "RAW";
            break;
        case SubGhzReadRawStateRecording:
            if (app->raw_sample_count >= 1000)
                snprintf(right_buf, sizeof(right_buf), "REC %luk",
                         (unsigned long)(app->raw_sample_count / 1000));
            else
                snprintf(right_buf, sizeof(right_buf), "REC %lu",
                         (unsigned long)app->raw_sample_count);
            right_status = right_buf;
            break;
        case SubGhzReadRawStateIdle:
            if (app->raw_sample_count >= 1000)
                snprintf(right_buf, sizeof(right_buf), "%luk spl",
                         (unsigned long)(app->raw_sample_count / 1000));
            else
                snprintf(right_buf, sizeof(right_buf), "%lu spl",
                         (unsigned long)app->raw_sample_count);
            right_status = right_buf;
            break;
    }

    subghz_status_bar_draw(freq, mod, right_status, false);

    /* Waveform area frame — always visible */
    subghz_raw_draw_frame_ext();

    if (app->raw_state == SubGhzReadRawStateStart)
    {
        /* Animated sine wave (Flipper-style Lissajous) when no capture */
        subghz_raw_draw_sin_ext();
        subghz_raw_sin_advance_ext();
    }
    else
    {
        /* During recording: update current RSSI in display without advancing
         * (trace=false just overwrites the current position for live feedback) */
        if (app->raw_state == SubGhzReadRawStateRecording)
        {
            app->rssi = subghz_read_rssi_ext();
            subghz_raw_rssi_push_ext((float)app->rssi, false);
        }

        /* Draw RSSI history spectrogram */
        subghz_raw_rssi_draw_ext();
    }

    /* Filename display centered in the waveform area when capture exists */
    if (app->raw_state == SubGhzReadRawStateIdle && raw_filepath[0] != '\0')
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
                NULL, "REC",
                NULL, NULL);
            break;
        case SubGhzReadRawStateRecording:
            subghz_button_bar_draw(
                NULL, NULL,
                NULL, "Stop",
                NULL, NULL);
            break;
        case SubGhzReadRawStateIdle:
            subghz_button_bar_draw(
                arrowleft_8x8, "Erase",
                NULL, "Send",
                arrowright_8x8, "Save");
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
