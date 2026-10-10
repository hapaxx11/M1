/* See COPYING.txt for license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32h5xx_hal.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_settings.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_subghz_button_bar.h"
#include "m1_subghz_scene.h"
#include "subghz_freq_presets.h"
#include "subghz_protocol_registry.h"
#include "tpms_receiver.h"

#define TPMS_DEFAULT_FREQUENCY_HZ 433920000UL
#define TPMS_MODULATION_AM650 1U
#define TPMS_VISIBLE_ROWS 4U

extern S_M1_SubGHz_Scan_Config subghz_scan_config;
extern int16_t subghz_read_rssi_ext(void);
extern uint32_t subghz_get_freq_hz_ext(uint8_t freq_idx);
extern uint32_t subghz_get_user_custom_freq_ext(void);
extern void subghz_set_user_custom_freq_ext(uint32_t frequency_hz);
extern void subghz_apply_config_ext(uint8_t freq_idx, uint8_t mod_idx);
extern void subghz_set_freq_idx_ext(uint8_t idx);
extern void subghz_set_mod_idx_ext(uint8_t idx);
extern void sub_ghz_rx_init_ext(void);
extern void sub_ghz_rx_start_ext(void);
extern void sub_ghz_rx_pause_ext(void);
extern void sub_ghz_rx_deinit_ext(void);
extern void sub_ghz_set_opmode_ext(uint8_t opmode, uint8_t band,
                                   uint8_t channel, uint8_t tx_power);

static void stop_rx(tpms_receiver_app_state_t *state)
{
    if (state->radio_active)
    {
        sub_ghz_rx_pause_ext();
        sub_ghz_rx_deinit_ext();
        sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_ISOLATED,
                               subghz_scan_config.band, 0, 0);
        subghz_decenc_ctl.pulse_det_stat = PULSE_DET_IDLE;
        state->radio_active = false;
    }
}

static void start_rx(SubGhzApp *app)
{
    tpms_receiver_app_state_t *state = app->tpms_state;
    uint32_t frequency_hz = subghz_get_freq_hz_ext(app->freq_idx);
    stop_rx(state);
    subghz_record_mode_flag = 0;
    subghz_set_user_custom_freq_ext(frequency_hz);
    subghz_apply_config_ext(app->freq_idx, app->mod_idx);
    menu_sub_ghz_init();

    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE;
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_RX, subghz_scan_config.band, 0, 0);
    sub_ghz_rx_init_ext();
    sub_ghz_rx_start_ext();
    state->radio_active = true;
    app->current_freq_hz = frequency_hz;
}

static void scene_on_enter(SubGhzApp *app)
{
    bool resume_from_child = app->resume_from_child;
    if (app->tpms_state == NULL)
    {
        app->tpms_state = malloc(sizeof(*app->tpms_state));
        if (app->tpms_state == NULL)
        {
            app->need_redraw = true;
            return;
        }
        memset(app->tpms_state, 0, sizeof(*app->tpms_state));
        tpms_history_reset(&app->tpms_state->history);
    }

    tpms_receiver_app_state_t *state = app->tpms_state;
    if (!resume_from_child)
    {
        state->saved_custom_frequency_hz = subghz_get_user_custom_freq_ext();
        state->saved_frequency_index = app->freq_idx;
        state->saved_modulation_index = app->mod_idx;
        subghz_set_user_custom_freq_ext(TPMS_DEFAULT_FREQUENCY_HZ);
        app->freq_idx = SUBGHZ_FREQ_PRESET_CUSTOM;
        app->mod_idx = TPMS_MODULATION_AM650;
        state->selected_sensor = 0;
        state->detail_view = false;
    }

    app->resume_from_child = false;
    subghz_decenc_init();
    subghz_decenc_set_weather_only(false);
    subghz_decenc_set_tpms_only(true);
    start_rx(app);
    subghz_scene_set_tick_period(app, 200U);
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    tpms_receiver_app_state_t *state = app->tpms_state;
    if (state == NULL)
    {
        if (event == SubGhzEventBack)
            subghz_scene_pop(app);
        return true;
    }

    switch (event)
    {
        case SubGhzEventBack:
            if (state->detail_view)
            {
                state->detail_view = false;
                app->need_redraw = true;
            }
            else
            {
                subghz_scene_pop(app);
            }
            return true;

        case SubGhzEventOk:
            if (state->history.count > 0U)
            {
                state->detail_view = !state->detail_view;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventLeft:
            stop_rx(state);
            subghz_decenc_set_tpms_only(false);
            app->resume_from_child = true;
            app->config_filter_mode = SubGhzConfigFilterFullRegistry;
            subghz_scene_push(app, SubGhzSceneConfig);
            return true;

        case SubGhzEventUp:
            if (!state->detail_view && state->selected_sensor > 0U)
            {
                state->selected_sensor--;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventDown:
            if (!state->detail_view &&
                state->selected_sensor + 1U < state->history.count)
            {
                state->selected_sensor++;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventRxData:
        {
            SubGHz_Dec_Info_t decoded;
            if (state->radio_active && subghz_decenc_read(&decoded, false) &&
                subghz_protocol_is_tpms(decoded.protocol))
            {
                tpms_sensor_t sensor = {
                    .protocol = decoded.protocol,
                    .bit_length = decoded.bit_len,
                    .serial = decoded.serial_number,
                    .data = decoded.key,
                    .frequency_hz = app->current_freq_hz,
                    .last_seen_ms = HAL_GetTick(),
                    .receptions = 1U,
                    .rssi = decoded.rssi,
                };
                (void)tpms_history_add(&state->history, &sensor);
                if (state->selected_sensor >= state->history.count)
                    state->selected_sensor = state->history.count - 1U;
                app->need_redraw = true;
            }
            return true;
        }

        case SubGhzEventTick:
            if (state->radio_active)
                app->rssi = subghz_read_rssi_ext();
            app->need_redraw = true;
            return true;

        default:
            return false;
    }
}

static void scene_on_exit(SubGhzApp *app)
{
    tpms_receiver_app_state_t *state = app->tpms_state;
    if (state != NULL)
        stop_rx(state);
    subghz_decenc_set_tpms_only(false);

    if (!app->resume_from_child && state != NULL)
    {
        subghz_set_user_custom_freq_ext(state->saved_custom_frequency_hz);
        app->freq_idx = state->saved_frequency_index;
        app->mod_idx = state->saved_modulation_index;
        subghz_set_freq_idx_ext(app->freq_idx);
        subghz_set_mod_idx_ext(app->mod_idx);
        subghz_apply_config_ext(app->freq_idx, app->mod_idx);
        settings_save_to_sd();
    }
}

static void draw_detail(const tpms_sensor_t *sensor)
{
    char line[32];
    m1_u8g2_firstpage();
    do
    {
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        (void)snprintf(line, sizeof(line), "%s  %u bits",
                       subghz_protocol_get_name(sensor->protocol),
                       (unsigned)sensor->bit_length);
        u8g2_DrawStr(&m1_u8g2, 0, 12, line);
        (void)snprintf(line, sizeof(line), "ID: %08lX",
                       (unsigned long)sensor->serial);
        u8g2_DrawStr(&m1_u8g2, 0, 23, line);
        (void)snprintf(line, sizeof(line), "Data: %08lX%08lX",
                       (unsigned long)(sensor->data >> 32),
                       (unsigned long)sensor->data);
        u8g2_DrawStr(&m1_u8g2, 0, 34, line);
        (void)snprintf(line, sizeof(line), "%lu.%03lu MHz  %ddBm",
                       (unsigned long)(sensor->frequency_hz / 1000000UL),
                       (unsigned long)((sensor->frequency_hz % 1000000UL) / 1000UL),
                       (int)sensor->rssi);
        u8g2_DrawStr(&m1_u8g2, 0, 45, line);
        (void)snprintf(line, sizeof(line), "Seen %um  x%u",
                       (unsigned)tpms_sensor_age_min(sensor, HAL_GetTick()),
                       (unsigned)sensor->receptions);
        u8g2_DrawStr(&m1_u8g2, 0, 57, line);
    } while (m1_u8g2_nextpage());
}

static void draw(SubGhzApp *app)
{
    tpms_receiver_app_state_t *state = app->tpms_state;
    char line[32];
    if (state != NULL && state->detail_view)
    {
        const tpms_sensor_t *sensor =
            tpms_history_get(&state->history, state->selected_sensor);
        if (sensor != NULL)
            draw_detail(sensor);
    }
    else
    {
        m1_u8g2_firstpage();
        do
        {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 0, 10, "TPMS Receiver");
            if (state == NULL)
            {
                u8g2_DrawStr(&m1_u8g2, 0, 32, "TPMS unavailable");
                u8g2_DrawStr(&m1_u8g2, 0, 42, "Not enough memory");
            }
            else if (state->history.count == 0U)
            {
                u8g2_DrawStr(&m1_u8g2, 0, 32, "Listening for sensors");
                u8g2_DrawStr(&m1_u8g2, 0, 42, "Default: 433.920 MHz");
            }
            else
            {
                uint8_t first = (state->selected_sensor >= TPMS_VISIBLE_ROWS) ?
                    (uint8_t)(state->selected_sensor - TPMS_VISIBLE_ROWS + 1U) : 0U;
                uint8_t visible = (state->history.count < TPMS_VISIBLE_ROWS) ?
                    state->history.count : TPMS_VISIBLE_ROWS;
                for (uint8_t row = 0; row < visible; ++row)
                {
                    const tpms_sensor_t *sensor =
                        tpms_history_get(&state->history, first + row);
                        const char *name =
                            subghz_protocol_get_name(sensor->protocol);
                        uint32_t sensor_id = sensor->serial != 0U ?
                            sensor->serial : (uint32_t)sensor->data;
                        (void)snprintf(line, sizeof(line), "%c%.9s %08lX x%u",
                                       first + row == state->selected_sensor ? '>' : ' ',
                                       name != NULL ? name : "Unknown",
                                       (unsigned long)sensor_id,
                                       (unsigned)sensor->receptions);
                    u8g2_DrawStr(&m1_u8g2, 0, 23 + row * 9, line);
                }
            }
            subghz_button_bar_draw(NULL,
                                   (state != NULL && state->detail_view) ?
                                       NULL : "CFG",
                                   NULL,
                                   (state != NULL && state->detail_view) ?
                                       NULL : "OK:INFO",
                                   NULL,
                                   (state != NULL && state->detail_view) ?
                                       NULL : "U/D:LIST");
        } while (m1_u8g2_nextpage());
    }
}

const SubGhzSceneHandlers subghz_scene_tpms_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit = scene_on_exit,
    .draw = draw,
};

void subghz_tpms_scene_deinit(SubGhzApp *app)
{
    if (app != NULL)
    {
        free(app->tpms_state);
        app->tpms_state = NULL;
    }
}
