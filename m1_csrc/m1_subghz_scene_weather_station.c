/* See COPYING.txt for license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_settings.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_subghz_button_bar.h"
#include "m1_subghz_scene.h"
#include "subghz_freq_presets.h"
#include "subghz_weather_app.h"
#include "subghz_weather_parse.h"

#define WX_DEFAULT_FREQUENCY_HZ 433920000UL
#define WX_HISTORY_ROWS 4U

extern S_M1_SubGHz_Scan_Config subghz_scan_config;
extern int16_t subghz_read_rssi_ext(void);
extern uint32_t subghz_get_freq_hz_ext(uint8_t freq_idx);
extern uint32_t subghz_get_user_custom_freq_ext(void);
extern void subghz_set_user_custom_freq_ext(uint32_t frequency_hz);
extern void subghz_apply_config_ext(uint8_t freq_idx, uint8_t mod_idx);
extern void subghz_set_freq_idx_ext(uint8_t idx);
extern void subghz_set_mod_idx_ext(uint8_t idx);
extern void subghz_rx_pause_ext(void);
extern void subghz_rx_deinit_ext(void);
extern void subghz_set_opmode_ext(uint8_t opmode, uint8_t band,
                                 uint8_t channel, uint8_t tx_power);

static uint32_t current_time_ms(void)
{
    return HAL_GetTick();
}

static void stop_rx(subghz_weather_app_state_t *state)
{
    if (state->radio_active)
    {
        subghz_rx_pause_ext();
        subghz_rx_deinit_ext();
        subghz_set_opmode_ext(SUB_GHZ_OPMODE_ISOLATED,
                              subghz_scan_config.band, 0, 0);
        subghz_decenc_ctl.pulse_det_stat = PULSE_DET_IDLE;
        state->radio_active = false;
    }
}

static void start_rx(SubGhzApp *app)
{
    subghz_weather_app_state_t *state = app->weather_state;
    uint32_t frequency_hz = subghz_get_freq_hz_ext(app->freq_idx);

    stop_rx(state);
    subghz_record_mode_flag = 0;
    subghz_set_user_custom_freq_ext(frequency_hz);
    subghz_apply_config_ext(app->freq_idx, app->mod_idx);
    menu_sub_ghz_init();

    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE;
    subghz_weather_rx_arm(state->scan.mod, frequency_hz);
    state->radio_active = true;
    app->current_freq_hz = frequency_hz;
}

static void scene_on_enter(SubGhzApp *app)
{
    bool resume_from_child = app->resume_from_child;
    if (app->weather_state == NULL)
    {
        app->weather_state = malloc(sizeof(*app->weather_state));
        if (app->weather_state == NULL)
        {
            app->need_redraw = true;
            return;
        }
        memset(app->weather_state, 0, sizeof(*app->weather_state));
        subghz_weather_history_reset(&app->weather_state->history);
    }

    subghz_weather_app_state_t *state = app->weather_state;
    if (!resume_from_child)
    {
        state->saved_custom_frequency_hz = subghz_get_user_custom_freq_ext();
        state->saved_frequency_index = app->freq_idx;
        state->saved_modulation_index = app->mod_idx;
        subghz_set_user_custom_freq_ext(WX_DEFAULT_FREQUENCY_HZ);
        app->freq_idx = SUBGHZ_FREQ_PRESET_CUSTOM;
        state->selected_sensor = 0;
        state->first_visible_sensor = 0;
        state->detail_view = false;
    }

    app->resume_from_child = false;
    subghz_decenc_init();
    subghz_decenc_set_weather_only(true);
    subghz_weather_scan_init(&state->scan, 60000U, WX_SCAN_MOD_OOK, true,
                             current_time_ms());
    state->last_age_tick_ms = current_time_ms();
    start_rx(app);
    subghz_scene_set_tick_period(app, 200U);
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    subghz_weather_app_state_t *state = app->weather_state;
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
            subghz_decenc_set_weather_only(false);
            app->resume_from_child = true;
            app->config_filter_mode = SubGhzConfigFilterFullRegistry;
            subghz_scene_push(app, SubGhzSceneConfig);
            return true;

        case SubGhzEventUp:
            if (!state->detail_view && state->selected_sensor > 0U)
            {
                state->selected_sensor--;
                if (state->selected_sensor < state->first_visible_sensor)
                    state->first_visible_sensor = state->selected_sensor;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventDown:
            if (!state->detail_view &&
                state->selected_sensor + 1U < state->history.count)
            {
                state->selected_sensor++;
                if (state->selected_sensor >=
                    state->first_visible_sensor + WX_HISTORY_ROWS)
                    state->first_visible_sensor =
                        state->selected_sensor - WX_HISTORY_ROWS + 1U;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventRxData:
        {
            SubGHz_Dec_Info_t decoded;
            if (state->radio_active && subghz_decenc_read(&decoded, false) &&
                subghz_protocol_is_weather(decoded.protocol))
            {
                SubGhzWeatherFields fields;
                SubGhzWeatherSensor sensor;
                bool valid = subghz_weather_parse(decoded.protocol, decoded.key,
                                                   decoded.bit_len, &fields);
                if (!valid)
                {
                    const SubGHz_Weather_Data_t *legacy =
                        subghz_get_weather_data();
                    fields.id = legacy->id;
                    fields.channel = legacy->channel;
                    fields.button = WX_NO_BUTTON;
                    fields.battery_low = legacy->battery_low ? 1U : 0U;
                    fields.humidity = legacy->humidity;
                    fields.temp_d10 = legacy->temp_raw;
                    fields.has_temp = true;
                    valid = (legacy->id != 0U) || (legacy->temp_raw != 0);
                }

                if (valid)
                {
                    memset(&sensor, 0, sizeof(sensor));
                    sensor.protocol = decoded.protocol;
                    sensor.data = decoded.key;
                    sensor.bit_len = decoded.bit_len;
                    sensor.serial = fields.id;
                    sensor.channel = fields.channel;
                    sensor.button = fields.button;
                    sensor.battery_low = fields.battery_low;
                    sensor.humidity = fields.humidity;
                    sensor.temp_raw = fields.temp_d10;
                    sensor.has_temp = fields.has_temp;
                    sensor.rssi = decoded.rssi;
                    (void)subghz_weather_history_add(&state->history, &sensor,
                                                     current_time_ms());
                    if (state->selected_sensor >= state->history.count)
                        state->selected_sensor = state->history.count - 1U;
                    if (state->selected_sensor < state->first_visible_sensor)
                        state->first_visible_sensor = state->selected_sensor;
                    app->need_redraw = true;
                }
            }
            return true;
        }

        case SubGhzEventTick:
        {
            uint32_t now = current_time_ms();
            if (state->radio_active)
                app->rssi = subghz_read_rssi_ext();
            if (subghz_weather_scan_tick(&state->scan, now))
                start_rx(app);
            if ((uint32_t)(now - state->last_age_tick_ms) >= 60000U)
            {
                state->last_age_tick_ms = now;
                if (state->history.count > 0U)
                    app->need_redraw = true;
            }
            app->need_redraw = true;
            return true;
        }

        default:
            return false;
    }
}

static void scene_on_exit(SubGhzApp *app)
{
    subghz_weather_app_state_t *state = app->weather_state;
    if (state != NULL)
        stop_rx(state);
    subghz_decenc_set_weather_only(false);

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

static void draw(SubGhzApp *app)
{
    subghz_weather_app_state_t *state = app->weather_state;
    if (state == NULL)
    {
        m1_u8g2_firstpage();
        do
        {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 0, 32, "Weather unavailable");
            u8g2_DrawStr(&m1_u8g2, 0, 42, "Not enough memory");
        } while (m1_u8g2_nextpage());
        return;
    }

    if (state->detail_view &&
        state->selected_sensor < state->history.count)
    {
        sub_ghz_weather_draw_detail(
            &state->history.items[state->selected_sensor], current_time_ms());
    }
    else
    {
        sub_ghz_weather_draw_list(&state->history, state->selected_sensor,
                                  state->first_visible_sensor, state->scan.mod,
                                  current_time_ms());
    }
    subghz_button_bar_draw(NULL, state->detail_view ? NULL : "CFG", NULL,
                           state->detail_view ? NULL : "OK:INFO", NULL,
                           state->detail_view ? NULL : "U/D:LIST");
}

const SubGhzSceneHandlers subghz_scene_weather_station_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit = scene_on_exit,
    .draw = draw,
};

void subghz_weather_scene_deinit(SubGhzApp *app)
{
    if (app != NULL)
    {
        free(app->weather_state);
        app->weather_state = NULL;
    }
}
