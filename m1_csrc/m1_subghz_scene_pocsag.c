/* See COPYING.txt for license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_subghz_button_bar.h"
#include "m1_subghz_scene.h"
#include "subghz_freq_presets.h"
#include "m1_settings.h"

extern const char *subghz_freq_labels[];
extern const char *subghz_mod_labels[];
extern S_M1_SubGHz_Scan_Config subghz_scan_config;
extern int16_t subghz_read_rssi_ext(void);
extern uint32_t subghz_get_freq_hz_ext(uint8_t freq_idx);
extern uint32_t subghz_get_user_custom_freq_ext(void);
extern void subghz_set_user_custom_freq_ext(uint32_t frequency_hz);
extern void subghz_set_freq_idx_ext(uint8_t idx);
extern void subghz_set_mod_idx_ext(uint8_t idx);
extern void subghz_apply_config_ext(uint8_t freq_idx, uint8_t mod_idx);
extern void sub_ghz_rx_init_ext(void);
extern void sub_ghz_rx_start_ext(void);
extern void sub_ghz_rx_pause_ext(void);
extern void sub_ghz_rx_deinit_ext(void);
extern void sub_ghz_set_opmode_ext(uint8_t opmode, uint8_t band,
                                   uint8_t channel, uint8_t tx_power);
extern void SI446x_Change_Modem_OOK_PDTC(uint8_t value);

#define POCSAG_DEFAULT_FREQUENCY_HZ 439987500UL
#define POCSAG_FSK_MODULATION_IDX  2U
#define POCSAG_GAP_RESET_US        16000U
#define POCSAG_HISTORY_ROWS        4U

static const uint32_t pager_channels[] = {
    439987500UL, 448425000UL, 465970000UL, 466075000UL,
    929362500UL, 929387500UL, 929462500UL, 929537500UL,
    929587500UL, 929612500UL, 929662500UL, 929937500UL,
    931062500UL, 931212500UL, 931337500UL, 931862500UL, 931937500UL,
};
#define PAGER_CHANNEL_COUNT (sizeof(pager_channels) / sizeof(pager_channels[0]))

static void stop_rx(pocsag_receiver_app_state_t *state)
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

static void pager_scene_message(void *context, const pager_message_t *message)
{
    SubGhzApp *app = context;
    (void)pocsag_history_add(&app->pocsag_state->history, message,
                             app->current_freq_hz);
    pocsag_receiver_app_state_select_newest(app->pocsag_state);
    app->need_redraw = true;
}

static void start_rx(SubGhzApp *app)
{
    pocsag_receiver_app_state_t *state = app->pocsag_state;
    if (state == NULL)
        return;

    stop_rx(state);
    pocsag_receiver_reset(&state->receiver);
    flex_receiver_reset(&state->flex);
    subghz_record_mode_flag = 0;
    subghz_set_user_custom_freq_ext(
        (app->freq_idx == SUBGHZ_FREQ_PRESET_CUSTOM) ?
            subghz_get_user_custom_freq_ext() :
            subghz_get_freq_hz_ext(app->freq_idx));
    subghz_apply_config_ext(app->freq_idx, POCSAG_FSK_MODULATION_IDX);
    menu_sub_ghz_init();

    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE;
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_RX, subghz_scan_config.band, 0, 0);
    sub_ghz_rx_init_ext();
    sub_ghz_rx_start_ext();
    state->radio_active = true;
    app->current_freq_hz = subghz_get_freq_hz_ext(app->freq_idx);
}

static void scene_on_enter(SubGhzApp *app)
{
    bool resume_from_child = app->resume_from_child;
    if (app->pocsag_state == NULL)
    {
        /* Bounded ~8 KB: both decoders, fragments, and ten long messages. */
        app->pocsag_state = malloc(sizeof(*app->pocsag_state));
        if (app->pocsag_state == NULL)
        {
            app->need_redraw = true;
            return;
        }
        memset(app->pocsag_state, 0, sizeof(*app->pocsag_state));
        pocsag_history_reset(&app->pocsag_state->history);
    }

    pocsag_receiver_app_state_enter(app->pocsag_state, resume_from_child,
                                    subghz_get_user_custom_freq_ext(),
                                    app->freq_idx, app->mod_idx);
    if (!resume_from_child)
    {
        app->pocsag_state->channel_index = 0;
        subghz_set_user_custom_freq_ext(POCSAG_DEFAULT_FREQUENCY_HZ);
        app->freq_idx = SUBGHZ_FREQ_PRESET_CUSTOM;
    }

    app->resume_from_child = false;
    app->mod_idx = POCSAG_FSK_MODULATION_IDX;
    flex_receiver_init(&app->pocsag_state->flex, pager_scene_message, app);
    start_rx(app);
    subghz_scene_set_tick_period(app, 200U);
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    pocsag_receiver_app_state_t *state = app->pocsag_state;
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
            if (pocsag_history_get(&state->history, state->selected_message) != NULL)
            {
                state->detail_view = !state->detail_view;
                state->detail_offset = 0;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventLeft:
            if (!state->detail_view)
            {
                stop_rx(state);
                app->resume_from_child = true;
                app->config_filter_mode = SubGhzConfigFilterNone;
                subghz_scene_push(app, SubGhzSceneConfig);
            }
            return true;

        case SubGhzEventRight:
            if (!state->detail_view)
            {
                state->channel_index = (state->channel_index + 1U) % PAGER_CHANNEL_COUNT;
                subghz_set_user_custom_freq_ext(pager_channels[state->channel_index]);
                app->freq_idx = SUBGHZ_FREQ_PRESET_CUSTOM;
                start_rx(app);
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventUp:
            if (state->detail_view && state->detail_offset >= 21U)
            {
                state->detail_offset -= 21U;
                app->need_redraw = true;
            }
            else if (!state->detail_view && state->selected_message > 0U)
            {
                state->selected_message--;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventDown:
            if (state->detail_view)
            {
                const pocsag_message_t *message =
                    pocsag_history_get(&state->history, state->selected_message);
                if (message != NULL && state->detail_offset + 21U < strlen(message->text))
                {
                    state->detail_offset += 21U;
                    app->need_redraw = true;
                }
            }
            else if (state->selected_message + 1U < state->history.count)
            {
                state->selected_message++;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventRxData:
            if (state->radio_active)
            {
                pocsag_message_t message;
                uint16_t duration = app->pocsag_pulse_duration_us;
                if (duration > POCSAG_GAP_RESET_US)
                    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_EOP;
                flex_receiver_feed(&state->flex, app->pocsag_pulse_level,
                                   duration, HAL_GetTick());
                if (pocsag_receiver_feed(&state->receiver,
                                         app->pocsag_pulse_level,
                                         duration) &&
                    pocsag_receiver_take_message(&state->receiver, &message))
                {
                    pager_scene_message(app, &message);
                }
            }
            return true;

        case SubGhzEventTick:
            if (state->radio_active)
            {
                flex_receiver_poll(&state->flex, HAL_GetTick());
                app->rssi = subghz_read_rssi_ext();
                app->need_redraw = true;
            }
            return true;

        default:
            return false;
    }
}

static void scene_on_exit(SubGhzApp *app)
{
    pocsag_receiver_app_state_t *state = app->pocsag_state;
    if (state == NULL)
        return;

    stop_rx(state);
    if (!app->resume_from_child)
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

static void draw_detail(const pocsag_message_t *message, uint16_t detail_offset)
{
    char line[40];
    const bool flex = message->protocol == PAGER_PROTOCOL_FLEX;
    const char *kind = "Alphanumeric";
    if (flex)
    {
        if (message->function == 0U) kind = "Secure";
        else if (message->function == 2U) kind = "Tone";
        else if (message->function == 3U || message->function == 4U ||
                 message->function == 7U) kind = "Numeric";
        else if (message->function == 6U) kind = "Binary";
    }
    else
    {
        kind = message->function == 0U ? "Numeric" :
               message->function == 1U ? "Alert" : "Alphanumeric";
    }

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    if (flex)
    {
        (void)snprintf(line, sizeof(line), "CAP: %lu  %u/%u",
                       (unsigned long)message->ric, message->baud, message->levels);
    }
    else
    {
        (void)snprintf(line, sizeof(line), "RIC: %lu  %u baud",
                       (unsigned long)message->ric, message->baud);
    }
    u8g2_DrawStr(&m1_u8g2, 0, 24, line);

    (void)snprintf(line, sizeof(line), "%s%s %lu.%03lu MHz", kind,
                   message->partial ? " partial" : "",
                   (unsigned long)(message->frequency_hz / 1000000UL),
                   (unsigned long)((message->frequency_hz % 1000000UL) / 1000UL));
    u8g2_DrawStr(&m1_u8g2, 0, 34, line);

    const size_t length = strlen(message->text);
    for (uint8_t row = 0; row < 3U; ++row)
    {
        const size_t offset = (size_t)detail_offset + (size_t)row * 21U;
        if (offset >= length)
            break;
        char text[22];
        (void)snprintf(text, sizeof(text), "%.*s", 21, message->text + offset);
        u8g2_DrawStr(&m1_u8g2, 0, 44 + row * 8, text);
    }
}

static void draw(SubGhzApp *app)
{
    pocsag_receiver_app_state_t *state = app->pocsag_state;
    char frequency[16];
    if (app->freq_idx == SUBGHZ_FREQ_PRESET_CUSTOM)
    {
        uint32_t hz = subghz_get_freq_hz_ext(app->freq_idx);
        (void)snprintf(frequency, sizeof(frequency), "%lu.%03lu",
                       (unsigned long)(hz / 1000000UL),
                       (unsigned long)((hz % 1000000UL) / 1000UL));
    }
    else
    {
        (void)snprintf(frequency, sizeof(frequency), "%s",
                       (subghz_freq_labels != NULL) ?
                           subghz_freq_labels[app->freq_idx] : "???");
    }

    m1_u8g2_firstpage();
    do
    {
        subghz_status_bar_draw(frequency, "FM238", "PAGER", false);
        subghz_rssi_bar_draw(app->rssi);
        if (state == NULL)
        {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 0, 32, "Pager unavailable");
            u8g2_DrawStr(&m1_u8g2, 0, 42, "Not enough memory");
        }
        else if (state->detail_view)
        {
            const pocsag_message_t *message =
                pocsag_history_get(&state->history, state->selected_message);
            if (message != NULL)
                draw_detail(message, state->detail_offset);
        }
        else
        {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            if (state->history.count == 0U)
            {
                u8g2_DrawStr(&m1_u8g2, 0, 32, "Listening for pages");
                u8g2_DrawStr(&m1_u8g2, 0, 42, "Default: 439.9875 MHz");
            }
            else
            {
                uint8_t first = (state->selected_message >= POCSAG_HISTORY_ROWS) ?
                    (uint8_t)(state->selected_message - POCSAG_HISTORY_ROWS + 1U) : 0U;
                uint8_t visible = (state->history.count < POCSAG_HISTORY_ROWS) ?
                                  state->history.count : POCSAG_HISTORY_ROWS;
                for (uint8_t row = 0; row < visible; ++row)
                {
                    uint8_t index = (uint8_t)(first + row);
                    const pocsag_message_t *message =
                        pocsag_history_get(&state->history, index);
                    char line[32];
                    (void)snprintf(line, sizeof(line), "%c%c%lu %.18s",
                                   index == state->selected_message ? '>' : ' ',
                                   message->protocol == PAGER_PROTOCOL_FLEX ? 'F' : 'P',
                                   (unsigned long)message->ric,
                                   message->text[0] ? message->text : "Alert");
                    u8g2_DrawStr(&m1_u8g2, 0, 24 + row * 8, line);
                }
            }
        }
        subghz_button_bar_draw(NULL,
                               (state != NULL && state->detail_view) ? NULL : "CFG",
                               NULL,
                               (state != NULL && state->detail_view) ? NULL : "OK:INFO",
                               NULL,
                               (state != NULL && state->detail_view) ? "U/D:TEXT" : "FREQ");
    } while (m1_u8g2_nextpage());
}

const SubGhzSceneHandlers subghz_scene_pocsag_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit = scene_on_exit,
    .draw = draw,
};

void subghz_pocsag_scene_deinit(SubGhzApp *app)
{
    if (app != NULL)
    {
        free(app->pocsag_state);
        app->pocsag_state = NULL;
    }
}
