/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_read_raw.c
 * @brief  Sub-GHz Read Raw Scene — raw RF capture with oscilloscope waveform.
 *
 * States: IDLE → RECORDING → STOPPED
 * Navigation:
 *   OK   = Start recording (idle) / Stop (recording)
 *   BACK = Exit (idle) / Stop (recording)
 *   DOWN = Config (idle) / Save (stopped)
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
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"

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

/* RAW waveform helpers from m1_sub_ghz.c */
extern void subghz_raw_waveform_draw_ext(void);
extern void subghz_raw_waveform_reset_ext(void);
extern void subghz_raw_waveform_push_ext(uint16_t duration_us, uint8_t level);

/* SD card manager */
extern uint8_t sub_ghz_rx_raw_save_ext(bool header_init, bool last_data);

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    app->raw_state = SubGhzReadRawStateIdle;
    app->raw_sample_count = 0;
    app->rssi = -120;
    subghz_raw_waveform_reset_ext();
    app->need_redraw = true;
}

static void start_raw_rx(SubGhzApp *app)
{
    subghz_apply_config_ext(app->freq_idx, app->mod_idx);

    /* Initialize ring buffers and SD card */
    if (sub_ghz_ring_buffers_init_ext())
    {
        /* Memory error — stay idle */
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
    subghz_raw_waveform_reset_ext();

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
}

static void stop_raw_rx(SubGhzApp *app)
{
    sub_ghz_rx_pause_ext();
    sub_ghz_rx_deinit_ext();
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_ISOLATED, subghz_scan_config.band, 0, 0);
    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_IDLE;
    subghz_record_mode_flag = 0;

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);

    app->raw_state = SubGhzReadRawStateStopped;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            if (app->raw_state == SubGhzReadRawStateRecording)
            {
                stop_raw_rx(app);
                app->need_redraw = true;
            }
            else
            {
                /* Clean up and exit */
                if (app->raw_state == SubGhzReadRawStateStopped)
                    sub_ghz_ring_buffers_deinit_ext();
                subghz_scene_pop(app);
            }
            return true;

        case SubGhzEventOk:
            if (app->raw_state == SubGhzReadRawStateIdle)
            {
                start_raw_rx(app);
                app->need_redraw = true;
            }
            else if (app->raw_state == SubGhzReadRawStateRecording)
            {
                stop_raw_rx(app);
                app->need_redraw = true;
            }
            else if (app->raw_state == SubGhzReadRawStateStopped)
            {
                /* Reset and go back to idle */
                sub_ghz_ring_buffers_deinit_ext();
                app->raw_state = SubGhzReadRawStateIdle;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventDown:
            if (app->raw_state == SubGhzReadRawStateIdle)
            {
                subghz_scene_push(app, SubGhzSceneConfig);
            }
            else if (app->raw_state == SubGhzReadRawStateStopped)
            {
                /* Save the raw recording */
                subghz_scene_push(app, SubGhzSceneSaveName);
            }
            return true;

        case SubGhzEventRxData:
            if (app->raw_state == SubGhzReadRawStateRecording)
            {
                app->rssi = subghz_read_rssi_ext();
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

    /* Status bar */
    const char *freq = subghz_freq_labels ? subghz_freq_labels[app->freq_idx] : "???";
    const char *mod  = subghz_mod_labels  ? subghz_mod_labels[app->mod_idx]   : "???";
    const char *state = NULL;

    switch (app->raw_state)
    {
        case SubGhzReadRawStateIdle:      state = "IDLE"; break;
        case SubGhzReadRawStateRecording: state = "REC";  break;
        case SubGhzReadRawStateStopped:   state = "DONE"; break;
    }

    subghz_status_bar_draw(freq, mod, state, false);

    /* RSSI bar when recording */
    if (app->raw_state == SubGhzReadRawStateRecording)
        subghz_rssi_bar_draw(app->rssi);

    /* Waveform area */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    if (app->raw_state == SubGhzReadRawStateIdle)
    {
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 20, 30, "Press OK to");
        u8g2_DrawStr(&m1_u8g2, 20, 40, "start recording");
    }
    else
    {
        /* Draw waveform */
        subghz_raw_waveform_draw_ext();

        /* Sample count in top corner */
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        snprintf(line, sizeof(line), "%luk", (unsigned long)(app->raw_sample_count / 1000));
        u8g2_DrawStr(&m1_u8g2, 100, 22, line);
    }

    /* Bottom bar */
    switch (app->raw_state)
    {
        case SubGhzReadRawStateIdle:
            subghz_button_bar_draw(
                arrowleft_8x8, "Back",
                arrowdown_8x8, "Config",
                NULL, "OK:Rec");
            break;
        case SubGhzReadRawStateRecording:
            subghz_button_bar_draw(
                arrowleft_8x8, "Stop",
                NULL, NULL,
                NULL, "OK:Stop");
            break;
        case SubGhzReadRawStateStopped:
            subghz_button_bar_draw(
                arrowleft_8x8, "Exit",
                arrowdown_8x8, "Save",
                NULL, "OK:New");
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
