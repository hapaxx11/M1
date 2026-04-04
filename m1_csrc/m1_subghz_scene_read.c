/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_read.c
 * @brief  Sub-GHz Read Scene — protocol-aware receive with RSSI bar,
 *         scrollable history list, and decode feedback.
 *
 * Layout (128x64):
 *   Row 0-9:    Status bar (freq, mod, state)
 *   Row 10-13:  RSSI bar
 *   Row 14-51:  Content area (history list or "listening...")
 *   Row 52-63:  Bottom bar (button hints)
 *
 * Navigation:
 *   OK    = Start listening (idle) / View detail (history) / Stop (active)
 *   BACK  = Stop (active) / Exit to menu (idle)
 *   L/R   = Cycle frequency preset (idle only)
 *   U/D   = Scroll history list (when active and history non-empty)
 *   DOWN  = Config (idle)
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
#include "m1_buzzer.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_protocol_registry.h"
#include "m1_ring_buffer.h"
#include "m1_storage.h"
#include "m1_sdcard_man.h"
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"

/*============================================================================*/
/* Extern references to shared radio state (defined in m1_sub_ghz.c)          */
/*============================================================================*/

/* Frequency & modulation preset tables */
extern const char *subghz_freq_labels[];
extern const char *subghz_mod_labels[];

/* Protocol name table */

/* Hopper frequencies */
#define READ_HOPPER_FREQ_COUNT  6
extern const uint32_t subghz_hopper_freqs_ext[];

/* RSSI read helper */
extern int16_t subghz_read_rssi_ext(void);

/* Apply config to radio hardware */
extern void subghz_apply_config_ext(uint8_t freq_idx, uint8_t mod_idx);

/* Radio control */
extern void sub_ghz_rx_init(void);
extern void sub_ghz_rx_start(void);
extern void sub_ghz_rx_pause(void);
extern void sub_ghz_rx_deinit(void);
extern void sub_ghz_set_opmode(uint8_t opmode, uint8_t band, uint8_t channel, uint8_t tx_power);
extern uint8_t sub_ghz_ring_buffers_init(void);
extern void sub_ghz_ring_buffers_deinit(void);
extern void sub_ghz_tx_raw_deinit(void);

/* Raw save helpers */
extern S_M1_SDM_DatFileInfo_t datfile_info;
extern S_M1_SubGHz_Scan_Config subghz_scan_config;
extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern uint8_t subghz_record_mode_flag;

/* Ring buffer */
extern S_M1_RingBuffer subghz_rx_rawdata_rb;

/* Decoder polling */
extern bool subghz_decenc_read(SubGHz_Dec_Info_t *out, bool raw_mode);

/* SI446x control */
extern void SI446x_Change_Modem_OOK_PDTC(uint8_t value);

/*============================================================================*/
/* Display constants                                                          */
/*============================================================================*/

#define CONTENT_Y_START   15  /* Below RSSI bar */
#define CONTENT_Y_END     51  /* Above bottom bar */
#define HISTORY_ROW_H      8  /* Pixels per history row */
#define HISTORY_VISIBLE    4  /* Number of visible history items */

/* Hopper */
#define HOPPER_RSSI_THRESHOLD  -70
#define HOPPER_DWELL_MS        150

/* SI4463 config */
#define OOK_PDTC_VALUE  0x6C

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    app->read_state    = SubGhzReadStateIdle;
    app->history_sel   = 0;
    app->history_scroll = 0;
    app->history_view  = false;
    app->detail_view   = false;
    app->has_decoded   = false;
    app->rssi          = -120;
    subghz_history_reset(&app->history);
    app->need_redraw = true;
}

static void start_rx(SubGhzApp *app)
{
    /* Apply config to radio */
    subghz_apply_config_ext(app->freq_idx, app->mod_idx);

    /* Initialize hopper if enabled */
    app->hopper_active = app->hopping;
    app->hopper_idx = 0;
    if (app->hopper_active)
        app->hopper_freq = subghz_hopper_freqs_ext[0];

    /* Start RX */
    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE;
    sub_ghz_set_opmode(SUB_GHZ_OPMODE_RX, subghz_scan_config.band, 0, 0);
    SI446x_Change_Modem_OOK_PDTC(OOK_PDTC_VALUE);
    sub_ghz_rx_init();
    sub_ghz_rx_start();

    subghz_record_mode_flag = 1;
    app->read_state = SubGhzReadStateRx;

    /* LED: cyan fast blink */
    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
}

static void stop_rx(SubGhzApp *app)
{
    app->hopper_active = false;
    sub_ghz_rx_pause();
    sub_ghz_rx_deinit();
    sub_ghz_set_opmode(SUB_GHZ_OPMODE_ISOLATED, subghz_scan_config.band, 0, 0);
    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_IDLE;
    subghz_record_mode_flag = 0;

    /* LED off */
    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);

    app->read_state = SubGhzReadStateIdle;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            if (app->read_state == SubGhzReadStateRx)
            {
                if (app->detail_view)
                {
                    app->detail_view = false;
                    app->need_redraw = true;
                }
                else if (app->history_view)
                {
                    app->history_view = false;
                    app->need_redraw = true;
                }
                else
                {
                    /* Stop RX */
                    stop_rx(app);
                    app->need_redraw = true;
                }
            }
            else
            {
                /* Exit to menu */
                subghz_scene_pop(app);
            }
            return true;

        case SubGhzEventOk:
            if (app->read_state == SubGhzReadStateIdle)
            {
                /* Start listening */
                start_rx(app);
                app->need_redraw = true;
            }
            else if (app->read_state == SubGhzReadStateRx)
            {
                if (app->history_view && !app->detail_view)
                {
                    /* View detail of selected signal */
                    app->detail_view = true;
                    app->need_redraw = true;
                }
                else if (app->detail_view)
                {
                    /* In detail view, OK = push receiver info scene */
                    subghz_scene_push(app, SubGhzSceneReceiverInfo);
                }
                else
                {
                    /* Stop listening */
                    stop_rx(app);
                    app->need_redraw = true;
                }
            }
            return true;

        case SubGhzEventLeft:
            if (app->read_state == SubGhzReadStateIdle)
            {
                app->freq_idx = (app->freq_idx > 0) ?
                    app->freq_idx - 1 : SUBGHZ_FREQ_PRESET_CNT - 1;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventRight:
            if (app->read_state == SubGhzReadStateIdle)
            {
                app->freq_idx = (app->freq_idx + 1) % SUBGHZ_FREQ_PRESET_CNT;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventUp:
            if (app->read_state == SubGhzReadStateRx)
            {
                if (app->history_view && !app->detail_view)
                {
                    /* Scroll up in history */
                    if (app->history_sel > 0)
                        app->history_sel--;
                    if (app->history_sel < app->history_scroll)
                        app->history_scroll = app->history_sel;
                    app->need_redraw = true;
                }
                else if (!app->history_view && app->history.count > 0)
                {
                    /* Open history list */
                    app->history_view = true;
                    app->history_sel = 0;
                    app->history_scroll = 0;
                    app->need_redraw = true;
                }
            }
            return true;

        case SubGhzEventDown:
            if (app->read_state == SubGhzReadStateIdle)
            {
                /* Open config */
                subghz_scene_push(app, SubGhzSceneConfig);
            }
            else if (app->read_state == SubGhzReadStateRx)
            {
                if (app->history_view && !app->detail_view)
                {
                    /* Scroll down in history */
                    if (app->history_sel + 1 < app->history.count)
                        app->history_sel++;
                    if (app->history_sel >= app->history_scroll + HISTORY_VISIBLE)
                        app->history_scroll = app->history_sel - HISTORY_VISIBLE + 1;
                    app->need_redraw = true;
                }
                else if (app->detail_view)
                {
                    /* Save signal */
                    subghz_scene_push(app, SubGhzSceneSaveName);
                }
            }
            return true;

        case SubGhzEventHopperTick:
            if (app->hopper_active && app->read_state == SubGhzReadStateRx)
            {
                /* Read RSSI; if below threshold, hop */
                app->rssi = subghz_read_rssi_ext();
                if (app->rssi < HOPPER_RSSI_THRESHOLD)
                {
                    app->hopper_idx = (app->hopper_idx + 1) % READ_HOPPER_FREQ_COUNT;
                    app->hopper_freq = subghz_hopper_freqs_ext[app->hopper_idx];
                    /* Retune radio — done by the Read scene via shared helpers */
                }
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventRxData:
            /* Poll decoder for newly decoded signals */
            if (app->read_state == SubGhzReadStateRx)
            {
                SubGHz_Dec_Info_t decoded;
                if (subghz_decenc_read(&decoded, false) && decoded.key != 0)
                {
                    /* Add to history */
                    uint32_t freq = app->hopper_active ? app->hopper_freq :
                        0; /* freq will be filled by history_add */
                    subghz_history_add(&app->history, &decoded, freq);
                    app->last_decoded = decoded;
                    app->has_decoded = true;

                    /* Decode feedback: green LED flash + sound */
                    m1_led_fast_blink(LED_BLINK_ON_GREEN, LED_FASTBLINK_PWM_H, LED_FASTBLINK_ONTIME_H);
                    if (app->sound)
                        m1_buzzer_notification();

                    /* LED will auto-restore to RX color on next redraw cycle
                     * (fast blink timer handles the flash duration) */
                }

                /* Update RSSI */
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
    /* Ensure radio is stopped when leaving scene */
    if (app->read_state == SubGhzReadStateRx)
        stop_rx(app);
}

static void draw(SubGhzApp *app)
{
    char line[40];

    m1_u8g2_firstpage();

    /* --- Status bar --- */
    const char *freq = subghz_freq_labels ? subghz_freq_labels[app->freq_idx] : "???";
    const char *mod  = subghz_mod_labels  ? subghz_mod_labels[app->mod_idx]   : "???";
    const char *state = NULL;

    switch (app->read_state)
    {
        case SubGhzReadStateIdle:    state = "IDLE"; break;
        case SubGhzReadStateRx:      state = "RX";   break;
        case SubGhzReadStateStopped: state = "STOP"; break;
    }

    if (app->hopper_active && app->read_state == SubGhzReadStateRx)
    {
        /* Show current hopper frequency instead of preset */
        snprintf(line, sizeof(line), "%.2f", (float)app->hopper_freq / 1000000.0f);
        subghz_status_bar_draw(line, mod, state, true);
    }
    else
    {
        subghz_status_bar_draw(freq, mod, state, false);
    }

    /* --- RSSI bar (only when active) --- */
    if (app->read_state == SubGhzReadStateRx)
        subghz_rssi_bar_draw(app->rssi);

    /* --- Content area --- */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    if (app->read_state == SubGhzReadStateIdle)
    {
        /* Idle screen: show antenna icon + instructions */
        u8g2_DrawXBMP(&m1_u8g2, 0, 14, 50, 27, subghz_antenna_50x27);
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 54, 28, "Press OK");
        u8g2_DrawStr(&m1_u8g2, 54, 38, "to listen");
    }
    else if (app->detail_view && app->history.count > 0)
    {
        /* Signal detail view */
        const SubGHz_History_Entry_t *e = subghz_history_get(&app->history, app->history_sel);
        if (e)
        {
            u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 2, CONTENT_Y_START + 8, protocol_text[e->info.protocol]);

            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            snprintf(line, sizeof(line), "Key: 0x%lX %dbit", (uint32_t)e->info.key, e->info.bit_len);
            u8g2_DrawStr(&m1_u8g2, 2, CONTENT_Y_START + 18, line);

            snprintf(line, sizeof(line), "TE:%d RSSI:%ddBm", e->info.te, e->info.rssi);
            u8g2_DrawStr(&m1_u8g2, 2, CONTENT_Y_START + 27, line);

            if (e->count > 1)
            {
                snprintf(line, sizeof(line), "Received x%d", e->count);
                u8g2_DrawStr(&m1_u8g2, 2, CONTENT_Y_START + 36, line);
            }
        }
    }
    else if (app->history_view && app->history.count > 0)
    {
        /* History list */
        snprintf(line, sizeof(line), "History (%d)", app->history.count);
        u8g2_DrawStr(&m1_u8g2, 2, CONTENT_Y_START + 6, line);

        uint8_t vis = HISTORY_VISIBLE;
        if (vis > app->history.count) vis = app->history.count;

        for (uint8_t i = 0; i < vis; i++)
        {
            uint8_t idx = app->history_scroll + i;
            if (idx >= app->history.count) break;
            const SubGHz_History_Entry_t *e = subghz_history_get(&app->history, idx);
            if (!e) break;

            uint8_t y = CONTENT_Y_START + 8 + i * HISTORY_ROW_H;

            if (idx == app->history_sel)
            {
                u8g2_DrawBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH, HISTORY_ROW_H);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            }

            snprintf(line, sizeof(line), "%s 0x%lX",
                     protocol_text[e->info.protocol], (uint32_t)e->info.key);
            u8g2_DrawStr(&m1_u8g2, 2, y + 6, line);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
    }
    else if (app->has_decoded)
    {
        /* Live decode display */
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, CONTENT_Y_START + 10, protocol_text[app->last_decoded.protocol]);

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        snprintf(line, sizeof(line), "0x%lX %dbit",
                 (uint32_t)app->last_decoded.key, app->last_decoded.bit_len);
        u8g2_DrawStr(&m1_u8g2, 2, CONTENT_Y_START + 22, line);

        if (app->history.count > 0)
        {
            snprintf(line, sizeof(line), "[%d signals]", app->history.count);
            u8g2_DrawStr(&m1_u8g2, 80, CONTENT_Y_START + 10, line);
        }
    }
    else if (app->read_state == SubGhzReadStateRx)
    {
        /* Listening, no decode yet */
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        if (app->hopper_active)
            u8g2_DrawStr(&m1_u8g2, 20, 34, "Scanning...");
        else
            u8g2_DrawStr(&m1_u8g2, 20, 34, "Listening...");
    }

    /* --- Bottom bar --- */
    if (app->read_state == SubGhzReadStateIdle)
    {
        subghz_button_bar_draw(
            arrowleft_8x8, "Back",
            arrowdown_8x8, "Config",
            NULL, "OK:Listen");
    }
    else if (app->detail_view)
    {
        subghz_button_bar_draw(
            arrowleft_8x8, "Back",
            arrowdown_8x8, "Save",
            NULL, "OK:Info");
    }
    else if (app->history_view)
    {
        subghz_button_bar_draw(
            arrowleft_8x8, "Back",
            NULL, NULL,
            NULL, "OK:View");
    }
    else
    {
        /* Active RX, live view */
        subghz_button_bar_draw(
            arrowleft_8x8, "Stop",
            (app->history.count > 0) ? arrowup_8x8 : NULL,
            (app->history.count > 0) ? "Hist" : NULL,
            NULL, "OK:Stop");
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_read_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
