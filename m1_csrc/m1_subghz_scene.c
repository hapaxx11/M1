/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene.c
 * @brief  Sub-GHz Scene Manager — stack-based scene dispatch engine.
 *
 * Manages a scene stack where BACK always pops to the previous scene.
 * Each scene registers on_enter/on_event/on_exit/draw callbacks via
 * the SubGhzSceneHandlers table.  The main loop translates FreeRTOS
 * queue events into SubGhzEvent values and dispatches to the active scene.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_tasks.h"
#include "m1_system.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_subghz_scene.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_lp5814.h"
#include "m1_buzzer.h"
#include "uiView.h"

#define M1_LOGDB_TAG  "SubGhzScene"

/* Shared RX state (defined in m1_sub_ghz.c / m1_sub_ghz_decenc.c) */
extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern uint8_t subghz_record_mode_flag;

/* Persisted radio config accessors (defined in m1_sub_ghz.c) */
extern uint8_t subghz_get_freq_idx_ext(void);
extern uint8_t subghz_get_mod_idx_ext(void);
extern bool    subghz_get_hopping_ext(void);
extern bool    subghz_get_sound_ext(void);
extern uint8_t subghz_get_tx_power_idx_ext(void);

/*============================================================================*/
/* Scene handler registry — indexed by SubGhzSceneId                          */
/*============================================================================*/

static const SubGhzSceneHandlers *scene_registry[SubGhzSceneCount] = {
    [SubGhzSceneMenu]             = &subghz_scene_menu_handlers,
    [SubGhzSceneRead]             = &subghz_scene_read_handlers,
    [SubGhzSceneReadRaw]          = &subghz_scene_read_raw_handlers,
    [SubGhzSceneReceiverInfo]     = &subghz_scene_receiver_info_handlers,
    [SubGhzSceneConfig]           = &subghz_scene_config_handlers,
    [SubGhzSceneSaveName]         = &subghz_scene_save_name_handlers,
    [SubGhzSceneSaveSuccess]      = &subghz_scene_save_success_handlers,
    [SubGhzSceneNeedSaving]       = &subghz_scene_need_saving_handlers,
    [SubGhzSceneSaved]            = &subghz_scene_saved_handlers,
    [SubGhzSceneFreqAnalyzer]     = &subghz_scene_freq_analyzer_handlers,
    [SubGhzScenePlaylist]         = &subghz_scene_playlist_handlers,
    [SubGhzSceneSpectrumAnalyzer] = &subghz_scene_spectrum_analyzer_handlers,
    [SubGhzSceneRssiMeter]        = &subghz_scene_rssi_meter_handlers,
    [SubGhzSceneFreqScanner]      = &subghz_scene_freq_scanner_handlers,
    [SubGhzSceneWeatherStation]   = &subghz_scene_weather_station_handlers,
    [SubGhzSceneBruteForce]       = &subghz_scene_brute_force_handlers,
    [SubGhzSceneAddManually]      = &subghz_scene_add_manually_handlers,
    [SubGhzSceneRemote]           = &subghz_scene_remote_handlers,
    [SubGhzSceneBindWizard]       = &subghz_scene_bind_wizard_handlers,
};

/*============================================================================*/
/* Scene Manager API                                                          */
/*============================================================================*/

void subghz_scene_init(SubGhzApp *app)
{
    memset(app, 0, sizeof(*app));

    /* Load persisted radio config (set at boot from settings.cfg,
     * defaults to compile-time values if settings file is absent) */
    app->freq_idx     = subghz_get_freq_idx_ext();
    app->mod_idx      = subghz_get_mod_idx_ext();
    app->hopping      = subghz_get_hopping_ext();
    app->sound        = subghz_get_sound_ext();
    app->tx_power_idx = subghz_get_tx_power_idx_ext();

    app->running      = true;
    app->scene_depth  = 0;
}

static const SubGhzSceneHandlers *get_handlers(SubGhzSceneId id)
{
    if (id < SubGhzSceneCount && scene_registry[id])
        return scene_registry[id];
    return NULL;
}

SubGhzSceneId subghz_scene_current(const SubGhzApp *app)
{
    if (app->scene_depth == 0)
        return SubGhzSceneMenu;  /* fallback */
    return app->scene_stack[app->scene_depth - 1];
}

void subghz_scene_push(SubGhzApp *app, SubGhzSceneId scene)
{
    /* Exit current scene if any */
    if (app->scene_depth > 0)
    {
        const SubGhzSceneHandlers *cur = get_handlers(subghz_scene_current(app));
        if (cur && cur->on_exit)
            cur->on_exit(app);
    }

    /* Push new scene */
    if (app->scene_depth < SUBGHZ_SCENE_STACK_MAX)
    {
        app->scene_stack[app->scene_depth++] = scene;
    }

    /* Enter new scene */
    const SubGhzSceneHandlers *next = get_handlers(scene);
    if (next && next->on_enter)
        next->on_enter(app);

    app->need_redraw = true;
}

void subghz_scene_pop(SubGhzApp *app)
{
    if (app->scene_depth == 0)
    {
        app->running = false;
        return;
    }

    /* Exit current scene */
    const SubGhzSceneHandlers *cur = get_handlers(subghz_scene_current(app));
    if (cur && cur->on_exit)
        cur->on_exit(app);

    app->scene_depth--;

    if (app->scene_depth == 0)
    {
        app->running = false;
        return;
    }

    /* Re-enter the scene that's now on top */
    const SubGhzSceneHandlers *prev = get_handlers(subghz_scene_current(app));
    if (prev && prev->on_enter)
        prev->on_enter(app);

    app->need_redraw = true;
}

void subghz_scene_replace(SubGhzApp *app, SubGhzSceneId scene)
{
    /* Exit current scene */
    if (app->scene_depth > 0)
    {
        const SubGhzSceneHandlers *cur = get_handlers(subghz_scene_current(app));
        if (cur && cur->on_exit)
            cur->on_exit(app);
        app->scene_stack[app->scene_depth - 1] = scene;
    }
    else
    {
        app->scene_stack[app->scene_depth++] = scene;
    }

    /* Enter replacement scene */
    const SubGhzSceneHandlers *next = get_handlers(scene);
    if (next && next->on_enter)
        next->on_enter(app);

    app->need_redraw = true;
}

bool subghz_scene_send_event(SubGhzApp *app, SubGhzEvent event)
{
    const SubGhzSceneHandlers *cur = get_handlers(subghz_scene_current(app));
    if (cur && cur->on_event)
        return cur->on_event(app, event);
    return false;
}

void subghz_scene_draw(SubGhzApp *app)
{
    const SubGhzSceneHandlers *cur = get_handlers(subghz_scene_current(app));
    if (cur && cur->draw)
        cur->draw(app);
}

/*============================================================================*/
/* Main event loop                                                            */
/*============================================================================*/

/**
 * @brief  Translate a FreeRTOS button event into SubGhzEvent.
 */
static SubGhzEvent translate_button(void)
{
    S_M1_Buttons_Status btn;
    BaseType_t ret = xQueueReceive(button_events_q_hdl, &btn, 0);
    if (ret != pdTRUE)
        return SubGhzEventNone;

    if (btn.event[BUTTON_OK_KP_ID]    == BUTTON_EVENT_CLICK) return SubGhzEventOk;
    if (btn.event[BUTTON_BACK_KP_ID]  == BUTTON_EVENT_CLICK) return SubGhzEventBack;
    if (btn.event[BUTTON_LEFT_KP_ID]  == BUTTON_EVENT_CLICK) return SubGhzEventLeft;
    if (btn.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK) return SubGhzEventRight;
    if (btn.event[BUTTON_UP_KP_ID]    == BUTTON_EVENT_CLICK) return SubGhzEventUp;
    if (btn.event[BUTTON_DOWN_KP_ID]  == BUTTON_EVENT_CLICK) return SubGhzEventDown;

    return SubGhzEventNone;
}

void subghz_scene_app_run(void)
{
    SubGhzApp app;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;

    /* Initialize scene manager and app context */
    subghz_scene_init(&app);

    /* Initialize radio hardware */
    menu_sub_ghz_init();

    /* Initialize protocol decoder function pointers and pulse tables */
    subghz_decenc_init();

    /* Push initial scene */
    subghz_scene_push(&app, SubGhzSceneMenu);

    /* Draw initial screen */
    subghz_scene_draw(&app);
    app.need_redraw = false;

    /* Time-based hopper tick: tracks wall-clock time so the hopper advances
     * every 200 ms even when the queue is saturated with Q_EVENT_SUBGHZ_RX
     * noise pulses.  At 315/433 MHz the RF noise floor generates thousands of
     * edges per second, keeping xQueueReceive() returning pdTRUE continuously
     * and preventing the queue-timeout path from ever firing.
     *
     * prev_hopper_active tracks the last iteration's state so we can detect
     * the false→true activation edge and reset the dwell timer, preventing
     * an immediate first tick when hopping is enabled after a period of
     * portMAX_DELAY blocking (during which the else branch never ran). */
    uint32_t last_hop_tick_ms = HAL_GetTick();
    bool     prev_hopper_active = false;

    /* Main event loop */
    while (app.running)
    {
        /* Determine if active RX needs periodic refresh */
        bool rx_active = false;
        SubGhzSceneId cur_scene = subghz_scene_current(&app);
        if (cur_scene == SubGhzSceneRead && app.read_state == SubGhzReadStateRx)
            rx_active = true;
        else if (cur_scene == SubGhzSceneReadRaw &&
                 (app.raw_state == SubGhzReadRawStateRecording ||
                  app.raw_state == SubGhzReadRawStateStart))
            rx_active = true;  /* periodic refresh: RSSI in Recording, sine animation in Start */

        /* Wait for event: use 200ms poll during active RX for RSSI refresh */
        TickType_t wait = (app.hopper_active || rx_active) ?
            pdMS_TO_TICKS(200) : portMAX_DELAY;

        ret = xQueueReceive(main_q_hdl, &q_item, wait);

        if (ret != pdTRUE)
        {
            /* Periodic display refresh during active RX (RSSI update) */
            if (rx_active)
                app.need_redraw = true;
        }
        else
        {
            SubGhzEvent evt = SubGhzEventNone;

            switch (q_item.q_evt_type)
            {
                case Q_EVENT_KEYPAD:
                    evt = translate_button();
                    break;
                case Q_EVENT_SUBGHZ_RX:
                    if (subghz_record_mode_flag)
                    {
                        /* Raw recording mode — pass through to scene
                         * (ring buffer data handled by Read Raw scene) */
                        evt = SubGhzEventRxData;
                    }
                    else if (subghz_decenc_ctl.subghz_pulse_handler)
                    {
                        /* Protocol decode mode — batch-drain all pending
                         * pulse events in a tight loop.  The ISR queues
                         * every individual edge as a separate event; at
                         * 433 MHz with AM650 the noise floor alone can
                         * generate thousands of edges/sec.  Processing
                         * only one pulse per main-loop iteration cannot
                         * keep up, the 256-deep queue overflows, and real
                         * signal pulses are silently dropped — so nothing
                         * ever decodes.  Draining in batch keeps the queue
                         * from overflowing and feeds complete packets to
                         * the protocol decoders. */
                        for (;;)
                        {
                            S_M1_Main_Q_t peek;
                            uint16_t dur = q_item.q_data.ir_rx_data.ir_edge_te;
                            uint8_t pdet = subghz_decenc_ctl.subghz_pulse_handler(dur);

                            /* Sync ISR state machine with pulse handler
                             * output.  Without this, the ISR stays in
                             * NORMAL forever and floods the queue with
                             * noise pulses between packets. */
                            if (pdet == PULSE_DET_EOP || pdet == PULSE_DET_IDLE)
                                subghz_decenc_ctl.pulse_det_stat = pdet;

                            /* Decode ready? Stop draining — let the scene
                             * handle the result before processing more. */
                            if (subghz_decenc_ctl.subghz_data_ready &&
                                subghz_decenc_ctl.subghz_data_ready())
                            {
                                evt = SubGhzEventRxData;
                                break;
                            }

                            /* Peek at next queue item; continue only if
                             * it is another pulse event.  Non-pulse events
                             * (keypad, TX-complete) stay in the queue for
                             * the outer loop to handle. */
                            if (xQueuePeek(main_q_hdl, &peek, 0) != pdTRUE)
                                break;          /* queue empty */
                            if (peek.q_evt_type != Q_EVENT_SUBGHZ_RX)
                                break;          /* next event is not a pulse */
                            xQueueReceive(main_q_hdl, &q_item, 0);
                        }
                    }
                    break;
                case Q_EVENT_SUBGHZ_TX:
                    evt = SubGhzEventTxComplete;
                    break;
                default:
                    break;
            }

            if (evt != SubGhzEventNone)
                subghz_scene_send_event(&app, evt);
        }

        /* Time-based hopper tick: fire every 200 ms of real wall-clock time.
         * This runs after every queue receive (or timeout), so the hopper
         * advances correctly even when the queue is saturated with noise.
         * On the false→true activation edge the timer is reset to now so
         * the first hop always waits a full 200 ms dwell. */
        if (app.hopper_active)
        {
            uint32_t now = HAL_GetTick();
            if (!prev_hopper_active)
            {
                /* Fresh activation edge — start the dwell interval from now */
                last_hop_tick_ms = now;
            }
            else if ((now - last_hop_tick_ms) >= 200U)
            {
                last_hop_tick_ms = now;
                subghz_scene_send_event(&app, SubGhzEventHopperTick);
            }
        }
        prev_hopper_active = app.hopper_active;

        /* Redraw if needed */
        if (app.need_redraw)
        {
            subghz_scene_draw(&app);
            app.need_redraw = false;
        }
    }

    /* Cleanup */
    menu_sub_ghz_exit();
    xQueueReset(button_events_q_hdl);
    xQueueReset(main_q_hdl);
}
