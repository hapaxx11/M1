/* See COPYING.txt for license details. */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ff.h"
#include "m1_display.h"
#include "m1_file_util.h"
#include "m1_lcd.h"
#include "m1_settings.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "m1_sub_ghz_decenc.h"
#include "m1_subghz_button_bar.h"
#include "m1_subghz_scene.h"
#include "m1_virtual_kb.h"
#include "flipper_subghz.h"
#include "restaurant_pager.h"
#include "restaurant_pager_settings.h"
#include "subghz_freq_presets.h"
#include "subghz_protocol_registry.h"

#define RESTAURANT_PAGER_HISTORY_CAPACITY 10U
#define RESTAURANT_PAGER_DEFAULT_MAX      255U
#define RESTAURANT_PAGER_DEFAULT_REPEAT   10U
#define RESTAURANT_PAGER_TX_PATH          "0:/SUBGHZ/_restaurant_pager_tx.sub"
#define RESTAURANT_PAGER_SAVE_ROOT        "/SUBGHZ/Pagers"
#define RESTAURANT_PAGER_SETTINGS_PATH    "0:/System/restaurant_pager.cfg"
#define RESTAURANT_PAGER_OOK_PRESET       "FuriHalSubGhzPresetOok650Async"
#define RESTAURANT_PAGER_ACTION_COUNT     5U

extern const char *subghz_freq_labels[];
extern int16_t subghz_read_rssi_ext(void);
extern uint32_t subghz_get_freq_hz_ext(uint8_t freq_idx);
extern void subghz_apply_config_ext(uint8_t freq_idx, uint8_t mod_idx);
extern void sub_ghz_rx_init_ext(void);
extern void sub_ghz_rx_start_ext(void);
extern void sub_ghz_rx_pause_ext(void);
extern void sub_ghz_rx_deinit_ext(void);
extern void sub_ghz_set_opmode_ext(uint8_t opmode, uint8_t band,
                                   uint8_t channel, uint8_t tx_power);
extern void SI446x_Change_Modem_OOK_PDTC(uint8_t value);
extern bool subghz_decenc_read(SubGHz_Dec_Info_t *out, bool raw_mode);
extern void subghz_pulse_handler_reset(void);
extern uint8_t subghz_record_mode_flag;
extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern S_M1_SubGHz_Scan_Config subghz_scan_config;

typedef struct {
    uint64_t key;
    uint32_t frequency_hz;
    uint16_t protocol;
    uint16_t bit_len;
    uint16_t te;
    uint8_t repeats;
    restaurant_pager_format_t format;
} restaurant_pager_entry_t;

struct restaurant_pager_app_state {
    restaurant_pager_entry_t history[RESTAURANT_PAGER_HISTORY_CAPACITY];
    uint8_t count;
    uint8_t selected;
    uint16_t max_pager;
    uint8_t tx_repeat_count;
    uint8_t action_selection;
    uint8_t setting_selection;
    restaurant_pager_view_t view;
    bool radio_active;
    bool waiting_for_tx;
    bool batch_active;
    uint16_t batch_next;
    uint16_t batch_max;
    restaurant_pager_entry_t batch_template;
    char status[24];
};

static const char *const action_labels[RESTAURANT_PAGER_ACTION_COUNT] = {
    "Edit / send",
    "Send to all",
    "Save station",
    "Browse saved",
    "Pager settings",
};

static void load_settings(struct restaurant_pager_app_state *state)
{
    FIL file;
    char text[24];
    UINT bytes_read = 0U;
    restaurant_pager_settings_t settings;

    if (f_open(&file, RESTAURANT_PAGER_SETTINGS_PATH, FA_READ) != FR_OK)
        return;
    FRESULT read_result = f_read(&file, text, sizeof(text) - 1U, &bytes_read);
    (void)f_close(&file);
    if (read_result != FR_OK || bytes_read >= sizeof(text))
        return;

    text[bytes_read] = '\0';
    if (restaurant_pager_settings_parse(text, &settings))
    {
        state->max_pager = settings.max_pager;
        state->tx_repeat_count = settings.tx_repeat_count;
    }
}

static bool save_settings(const struct restaurant_pager_app_state *state)
{
    restaurant_pager_settings_t settings = {
        .max_pager = state->max_pager,
        .tx_repeat_count = state->tx_repeat_count,
    };
    char text[16];
    if (!restaurant_pager_settings_format(&settings, text, sizeof(text)) ||
        fs_directory_ensure("0:/System") != FR_OK)
        return false;

    FIL file;
    if (f_open(&file, RESTAURANT_PAGER_SETTINGS_PATH,
               FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
        return false;

    UINT bytes_written = 0U;
    FRESULT write_result = f_write(&file, text, strlen(text), &bytes_written);
    FRESULT close_result = f_close(&file);
    return write_result == FR_OK && close_result == FR_OK &&
           bytes_written == strlen(text);
}

static void set_status(struct restaurant_pager_app_state *state,
                       const char *status)
{
    if (state == NULL)
        return;
    (void)snprintf(state->status, sizeof(state->status), "%s",
                   (status != NULL) ? status : "");
}

static restaurant_pager_entry_t *selected_entry(SubGhzApp *app)
{
    struct restaurant_pager_app_state *state = app->restaurant_pager_state;
    if (state == NULL || state->selected >= state->count)
        return NULL;
    return &state->history[state->selected];
}

static const char *entry_protocol_name(const restaurant_pager_entry_t *entry)
{
    if (entry == NULL)
        return NULL;
    const char *name = subghz_protocol_get_name(entry->protocol);
    if (name == NULL ||
        (strcmp(name, "Princeton") != 0 && strcmp(name, "SMC5326") != 0))
        return NULL;
    return name;
}

static void stop_rx(struct restaurant_pager_app_state *state)
{
    if (state != NULL && state->radio_active)
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
    struct restaurant_pager_app_state *state = app->restaurant_pager_state;
    if (state == NULL)
        return;

    stop_rx(state);
    subghz_record_mode_flag = 0;
    app->mod_idx = 0U;
    subghz_pulse_handler_reset();
    subghz_apply_config_ext(app->freq_idx, app->mod_idx);
    menu_sub_ghz_init();
    app->current_freq_hz = subghz_get_freq_hz_ext(app->freq_idx);

    subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE;
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_RX, subghz_scan_config.band, 0, 0);
    SI446x_Change_Modem_OOK_PDTC(0x6CU);
    sub_ghz_rx_init_ext();
    sub_ghz_rx_start_ext();
    state->radio_active = true;
}

static void add_capture(struct restaurant_pager_app_state *state,
                        const SubGHz_Dec_Info_t *decoded,
                        uint32_t frequency_hz)
{
    if (state == NULL || decoded == NULL)
        return;
    const char *protocol = subghz_protocol_get_name(decoded->protocol);
    if (protocol == NULL ||
        (strcmp(protocol, "Princeton") != 0 &&
         strcmp(protocol, "SMC5326") != 0) ||
        decoded->key > 0x1FFFFFFUL ||
        (decoded->bit_len != 24U && decoded->bit_len != 25U))
        return;

    uint8_t index = state->count;
    for (uint8_t i = 0U; i < state->count; ++i)
    {
        if (state->history[i].protocol == decoded->protocol &&
            state->history[i].key == decoded->key)
        {
            index = i;
            break;
        }
    }

    restaurant_pager_entry_t entry = {
        .key = decoded->key,
        .frequency_hz = frequency_hz,
        .protocol = decoded->protocol,
        .bit_len = decoded->bit_len,
        .te = decoded->te,
        .repeats = 1U,
        .format = restaurant_pager_auto_detect(
            (uint32_t)decoded->key, state->max_pager),
    };

    if (index < state->count)
    {
        entry.repeats = (state->history[index].repeats < UINT8_MAX) ?
            (uint8_t)(state->history[index].repeats + 1U) : UINT8_MAX;
        for (uint8_t i = index; i > 0U; --i)
            state->history[i] = state->history[i - 1U];
    }
    else
    {
        if (state->count < RESTAURANT_PAGER_HISTORY_CAPACITY)
            state->count++;
        for (uint8_t i = (uint8_t)(state->count - 1U); i > 0U; --i)
            state->history[i] = state->history[i - 1U];
    }

    state->history[0] = entry;
    state->selected = 0U;
    set_status(state, "");
}

static void change_format(restaurant_pager_entry_t *entry, int8_t delta)
{
    if (entry == NULL)
        return;
    int16_t next = (int16_t)entry->format + delta;
    if (next < 0)
        next = (int16_t)RESTAURANT_PAGER_FORMAT_COUNT - 1;
    else if (next >= (int16_t)RESTAURANT_PAGER_FORMAT_COUNT)
        next = 0;
    entry->format = (restaurant_pager_format_t)next;
}

static void change_pager(restaurant_pager_entry_t *entry, int8_t delta)
{
    if (entry == NULL)
        return;
    restaurant_pager_fields_t fields;
    if (!restaurant_pager_decode(entry->format, (uint32_t)entry->key, &fields))
        return;

    if ((delta < 0 && fields.pager == 0U) ||
        (delta > 0 && fields.pager == UINT16_MAX))
        return;
    uint16_t next = (uint16_t)(fields.pager + delta);
    uint32_t key = (uint32_t)entry->key;
    if (restaurant_pager_set_pager(entry->format, &key, next))
        entry->key = key;
}

static void change_action(restaurant_pager_entry_t *entry)
{
    if (entry == NULL)
        return;
    restaurant_pager_fields_t fields;
    if (!restaurant_pager_decode(entry->format, (uint32_t)entry->key, &fields))
        return;

    for (uint8_t offset = 1U;
         offset < (uint8_t)RESTAURANT_PAGER_ACTION_ALL_OFF + 1U;
         ++offset)
    {
        restaurant_pager_action_t action = (restaurant_pager_action_t)(
            (fields.action + offset) %
            ((uint8_t)RESTAURANT_PAGER_ACTION_ALL_OFF + 1U));
        uint32_t key = (uint32_t)entry->key;
        if (restaurant_pager_set_action(entry->format, &key, action))
        {
            entry->key = key;
            return;
        }
    }
}

static bool push_transmitter(SubGhzApp *app, restaurant_pager_entry_t *entry)
{
    const char *protocol = entry_protocol_name(entry);
    if (protocol == NULL)
        return false;

    if (fs_directory_ensure("/SUBGHZ") != FR_OK)
        return false;

    (void)f_unlink(RESTAURANT_PAGER_TX_PATH);
    if (!flipper_subghz_save_key(RESTAURANT_PAGER_TX_PATH,
                                 entry->frequency_hz,
                                 RESTAURANT_PAGER_OOK_PRESET,
                                 protocol,
                                 entry->bit_len,
                                 entry->key,
                                 entry->te))
        return false;

    (void)snprintf(app->tx_path, SUBGHZ_SAVED_PATH_MAX, "%s",
                   RESTAURANT_PAGER_TX_PATH);
    app->tx_repeat_count = app->restaurant_pager_state->tx_repeat_count;
    app->tx_mode = 0U;
    app->tx_autostart = true;
    (void)snprintf(app->tx_protocol_name, sizeof(app->tx_protocol_name),
                   "%s", protocol);
    app->restaurant_pager_state->waiting_for_tx = true;
    app->resume_from_child = true;
    subghz_scene_push(app, SubGhzSceneTransmitter);
    return true;
}

static bool valid_path_component(const char *name)
{
    if (name == NULL || name[0] == '\0' || name[0] == ' ' ||
        strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return false;

    size_t length = strlen(name);
    if (length > M1_VIRTUAL_KB_INPUT_MAX || name[length - 1U] == ' ')
        return false;

    for (const char *ch = name; *ch != '\0'; ++ch)
    {
        if (!((*ch >= 'a' && *ch <= 'z') ||
              (*ch >= 'A' && *ch <= 'Z') ||
              (*ch >= '0' && *ch <= '9') ||
              *ch == ' ' || *ch == '_' || *ch == '-'))
            return false;
    }
    return true;
}

static void save_station(SubGhzApp *app, restaurant_pager_entry_t *entry)
{
    struct restaurant_pager_app_state *state = app->restaurant_pager_state;
    const char *protocol = entry_protocol_name(entry);
    restaurant_pager_fields_t fields;
    char category[M1_VIRTUAL_KB_INPUT_MAX + 1U];
    char name[M1_VIRTUAL_KB_INPUT_MAX + 1U];
    char default_name[M1_VIRTUAL_KB_INPUT_MAX + 1U];
    char directory[64];
    char path[96];
    FILINFO info;

    stop_rx(state);
    if (protocol == NULL ||
        !restaurant_pager_decode(entry->format, (uint32_t)entry->key, &fields))
    {
        set_status(state, "Invalid station");
        start_rx(app);
        return;
    }

    (void)snprintf(default_name, sizeof(default_name), "Station_%u",
                   fields.pager);
    if (!m1_vkb_get_filename("Category:", "Restaurant", category) ||
        !valid_path_component(category) ||
        !m1_vkb_get_filename("Station name:", default_name, name) ||
        !valid_path_component(name))
    {
        set_status(state, "Save cancelled");
        start_rx(app);
        return;
    }

    int directory_len = snprintf(directory, sizeof(directory),
                                 RESTAURANT_PAGER_SAVE_ROOT "/%s", category);
    int path_len = snprintf(path, sizeof(path), "%s/%s.sub", directory, name);
    if (directory_len < 0 || (size_t)directory_len >= sizeof(directory) ||
        path_len < 0 || (size_t)path_len >= sizeof(path) ||
        fs_directory_ensure("/SUBGHZ") != FR_OK ||
        fs_directory_ensure(RESTAURANT_PAGER_SAVE_ROOT) != FR_OK ||
        fs_directory_ensure(directory) != FR_OK)
    {
        set_status(state, "Storage unavailable");
        start_rx(app);
        return;
    }

    FRESULT stat_result = f_stat(path, &info);
    if (stat_result == FR_OK)
    {
        set_status(state, "Name already used");
        start_rx(app);
        return;
    }
    if (stat_result != FR_NO_FILE && stat_result != FR_NO_PATH)
    {
        set_status(state, "Storage unavailable");
        start_rx(app);
        return;
    }

    if (!flipper_subghz_save_key(path, entry->frequency_hz,
                                 RESTAURANT_PAGER_OOK_PRESET,
                                 protocol, entry->bit_len, entry->key,
                                 entry->te))
    {
        set_status(state, "Save failed");
        start_rx(app);
        return;
    }

    set_status(state, "Station saved");
    app->resume_from_child = true;
    subghz_scene_push(app, SubGhzSceneSaved);
}

static bool start_batch_transmit(SubGhzApp *app)
{
    struct restaurant_pager_app_state *state = app->restaurant_pager_state;
    restaurant_pager_entry_t entry = state->batch_template;
    uint32_t key = (uint32_t)entry.key;

    if (state->batch_next > state->batch_max ||
        !restaurant_pager_set_pager(entry.format, &key, state->batch_next))
        return false;
    entry.key = key;
    if (!push_transmitter(app, &entry))
        return false;
    set_status(state, "Sending to all");
    return true;
}

static void scene_on_enter(SubGhzApp *app)
{
    bool resume_from_child = app->resume_from_child;
    bool restart_batch = false;
    app->resume_from_child = false;

    if (app->restaurant_pager_state == NULL)
    {
        /* The 10-entry history and scene state are bounded to a few hundred bytes. */
        app->restaurant_pager_state =
            malloc(sizeof(*app->restaurant_pager_state));
        if (app->restaurant_pager_state == NULL)
        {
            app->need_redraw = true;
            return;
        }
        memset(app->restaurant_pager_state, 0,
               sizeof(*app->restaurant_pager_state));
    }

    struct restaurant_pager_app_state *state = app->restaurant_pager_state;
    if (!resume_from_child)
    {
        memset(state, 0, sizeof(*state));
        state->max_pager = RESTAURANT_PAGER_DEFAULT_MAX;
        state->tx_repeat_count = RESTAURANT_PAGER_DEFAULT_REPEAT;
        load_settings(state);
        app->freq_idx = SUBGHZ_FREQ_DEFAULT_IDX;
        app->mod_idx = 0U;
    }
    else
    {
        app->mod_idx = 0U;
        if (state->waiting_for_tx)
        {
            set_status(state, app->tx_completed_naturally ?
                       "TX complete" : "TX cancelled");
            state->waiting_for_tx = false;
            (void)f_unlink(RESTAURANT_PAGER_TX_PATH);
            if (state->batch_active)
            {
                if (!app->tx_completed_naturally)
                {
                    state->batch_active = false;
                    set_status(state, "Batch cancelled");
                }
                else if (state->batch_next >= state->batch_max)
                {
                    state->batch_active = false;
                    set_status(state, "Batch complete");
                }
                else
                {
                    state->batch_next++;
                    restart_batch = true;
                    set_status(state, "Sending to all");
                }
            }
            else
                set_status(state, app->tx_completed_naturally ?
                           "TX complete" : "TX cancelled");
        }
    }

    start_rx(app);
    subghz_scene_set_tick_period(app, 200U);
    app->need_redraw = true;
    if (restart_batch && !start_batch_transmit(app))
    {
        state->batch_active = false;
        set_status(state, "Batch stopped");
        app->need_redraw = true;
    }
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    struct restaurant_pager_app_state *state = app->restaurant_pager_state;
    if (state == NULL)
    {
        if (event == SubGhzEventBack)
            subghz_scene_pop(app);
        return true;
    }

    restaurant_pager_entry_t *entry = selected_entry(app);
    switch (event)
    {
        case SubGhzEventBack:
            if (state->view == RESTAURANT_PAGER_VIEW_EDIT)
            {
                state->view = RESTAURANT_PAGER_VIEW_SCAN;
                set_status(state, "");
                app->need_redraw = true;
            }
            else if (state->view == RESTAURANT_PAGER_VIEW_ACTIONS ||
                     state->view == RESTAURANT_PAGER_VIEW_SETTINGS ||
                     state->view == RESTAURANT_PAGER_VIEW_BATCH_CONFIRM)
            {
                state->view = (state->view == RESTAURANT_PAGER_VIEW_SETTINGS ||
                               state->view == RESTAURANT_PAGER_VIEW_BATCH_CONFIRM) ?
                                  RESTAURANT_PAGER_VIEW_ACTIONS :
                                  RESTAURANT_PAGER_VIEW_SCAN;
                app->need_redraw = true;
            }
            else
                subghz_scene_pop(app);
            return true;

        case SubGhzEventLeft:
            if (state->view == RESTAURANT_PAGER_VIEW_EDIT)
            {
                change_format(entry, -1);
                app->need_redraw = true;
            }
            else if (state->view == RESTAURANT_PAGER_VIEW_SETTINGS)
            {
                if (state->setting_selection == 0U)
                    state->max_pager = (state->max_pager > 10U) ?
                        (uint16_t)(state->max_pager - 10U) : 0U;
                else if (state->tx_repeat_count > 1U)
                    state->tx_repeat_count--;
                if (!save_settings(state))
                    set_status(state, "Settings save failed");
                else
                    set_status(state, "Settings saved");
                app->need_redraw = true;
            }
            else
            {
                app->resume_from_child = true;
                app->config_filter_mode = SubGhzConfigFilterFullRegistry;
                stop_rx(state);
                subghz_scene_push(app, SubGhzSceneConfig);
            }
            return true;

        case SubGhzEventRight:
            if (state->view == RESTAURANT_PAGER_VIEW_EDIT)
            {
                change_action(entry);
                app->need_redraw = true;
            }
            else if (state->view == RESTAURANT_PAGER_VIEW_SETTINGS)
            {
                if (state->setting_selection == 0U)
                    state->max_pager = (state->max_pager <= 1013U) ?
                        (uint16_t)(state->max_pager + 10U) : 1023U;
                else if (state->tx_repeat_count < 50U)
                    state->tx_repeat_count++;
                if (!save_settings(state))
                    set_status(state, "Settings save failed");
                else
                    set_status(state, "Settings saved");
                app->need_redraw = true;
            }
            else if (state->view == RESTAURANT_PAGER_VIEW_SCAN &&
                     entry != NULL)
            {
                state->view = RESTAURANT_PAGER_VIEW_ACTIONS;
                state->action_selection = 0U;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventUp:
            if (state->view == RESTAURANT_PAGER_VIEW_EDIT)
                change_pager(entry, 1);
            else if (state->view == RESTAURANT_PAGER_VIEW_ACTIONS &&
                     state->action_selection > 0U)
                state->action_selection--;
            else if (state->view == RESTAURANT_PAGER_VIEW_SETTINGS)
                state->setting_selection = 0U;
            else if (state->view == RESTAURANT_PAGER_VIEW_SCAN &&
                     state->selected > 0U)
                state->selected--;
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            if (state->view == RESTAURANT_PAGER_VIEW_EDIT)
                change_pager(entry, -1);
            else if (state->view == RESTAURANT_PAGER_VIEW_ACTIONS &&
                     state->action_selection + 1U < RESTAURANT_PAGER_ACTION_COUNT)
                state->action_selection++;
            else if (state->view == RESTAURANT_PAGER_VIEW_SETTINGS)
                state->setting_selection = 1U;
            else if (state->view == RESTAURANT_PAGER_VIEW_SCAN &&
                     state->selected + 1U < state->count)
                state->selected++;
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
            if (state->view == RESTAURANT_PAGER_VIEW_EDIT)
            {
                if (!push_transmitter(app, entry))
                    set_status(state, "TX setup failed");
            }
            else if (state->view == RESTAURANT_PAGER_VIEW_SCAN &&
                     entry != NULL)
            {
                state->view = RESTAURANT_PAGER_VIEW_ACTIONS;
                state->action_selection = 0U;
            }
            else if (state->view == RESTAURANT_PAGER_VIEW_ACTIONS)
            {
                switch (state->action_selection)
                {
                    case 0U:
                        state->view = RESTAURANT_PAGER_VIEW_EDIT;
                        break;
                    case 1U:
                        state->batch_template = *entry;
                        {
                            uint32_t batch_key =
                                (uint32_t)state->batch_template.key;
                            bool batch_action_supported =
                                restaurant_pager_set_pager(
                                    state->batch_template.format, &batch_key, 0U) &&
                                restaurant_pager_set_action(
                                    state->batch_template.format, &batch_key,
                                    RESTAURANT_PAGER_ACTION_RING);
                            state->batch_template.key = batch_key;
                            if (!batch_action_supported)
                            {
                                set_status(state, "Unsupported format");
                                break;
                            }
                        }
                        state->batch_next = 0U;
                        state->batch_max = state->max_pager;
                        {
                            uint16_t limit = restaurant_pager_pager_limit(
                                state->batch_template.format);
                            if (state->batch_max > limit)
                                state->batch_max = limit;
                        }
                        state->view = RESTAURANT_PAGER_VIEW_BATCH_CONFIRM;
                        break;
                    case 2U:
                        save_station(app, entry);
                        break;
                    case 3U:
                        stop_rx(state);
                        app->resume_from_child = true;
                        subghz_scene_push(app, SubGhzSceneSaved);
                        break;
                    case 4U:
                        state->view = RESTAURANT_PAGER_VIEW_SETTINGS;
                        state->setting_selection = 0U;
                        break;
                    default:
                        break;
                }
            }
            else if (state->view == RESTAURANT_PAGER_VIEW_SETTINGS)
                state->view = RESTAURANT_PAGER_VIEW_ACTIONS;
            else if (state->view == RESTAURANT_PAGER_VIEW_BATCH_CONFIRM)
            {
                state->batch_active = true;
                state->view = RESTAURANT_PAGER_VIEW_SCAN;
                if (!start_batch_transmit(app))
                {
                    state->batch_active = false;
                    set_status(state, "Batch setup failed");
                }
            }
            app->need_redraw = true;
            return true;

        case SubGhzEventRxData:
            if (state->radio_active)
            {
                SubGHz_Dec_Info_t decoded;
                if (subghz_decenc_read(&decoded, false))
                {
                    if (restaurant_pager_capture_updates_history(state->view))
                        add_capture(state, &decoded, app->current_freq_hz);
                }
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventTick:
            if (state->radio_active)
            {
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
    struct restaurant_pager_app_state *state = app->restaurant_pager_state;
    stop_rx(state);
    if (!app->resume_from_child)
    {
        (void)f_unlink(RESTAURANT_PAGER_TX_PATH);
        settings_save_to_sd();
    }
}

static void draw_entry(const restaurant_pager_entry_t *entry)
{
    restaurant_pager_fields_t fields;
    char line[32];
    const char *protocol;
    if (entry == NULL ||
        !restaurant_pager_decode(entry->format, (uint32_t)entry->key, &fields))
        return;

    protocol = entry_protocol_name(entry);
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    (void)snprintf(line, sizeof(line), "S:%u P:%u %s",
                   fields.station, fields.pager,
                   restaurant_pager_action_name(fields.action));
    u8g2_DrawStr(&m1_u8g2, 0, 25, line);
    (void)snprintf(line, sizeof(line), "%s %s x%u",
                   restaurant_pager_format_name(entry->format),
                   (protocol != NULL) ? protocol : "?", entry->repeats);
    u8g2_DrawStr(&m1_u8g2, 0, 35, line);
    (void)snprintf(line, sizeof(line), "Data:%06lX",
                   (unsigned long)entry->key);
    u8g2_DrawStr(&m1_u8g2, 0, 45, line);
}

static void draw(SubGhzApp *app)
{
    struct restaurant_pager_app_state *state = app->restaurant_pager_state;
    char frequency[16];
    uint32_t hz = subghz_get_freq_hz_ext(app->freq_idx);
    (void)snprintf(frequency, sizeof(frequency), "%lu.%03lu",
                   (unsigned long)(hz / 1000000UL),
                   (unsigned long)((hz % 1000000UL) / 1000UL));

    m1_u8g2_firstpage();
    do
    {
        const char *screen = "PAGER";
        if (state != NULL)
        {
            if (state->view == RESTAURANT_PAGER_VIEW_EDIT)
                screen = "EDIT";
            else if (state->view == RESTAURANT_PAGER_VIEW_ACTIONS)
                screen = "ACTIONS";
            else if (state->view == RESTAURANT_PAGER_VIEW_SETTINGS)
                screen = "SET";
            else if (state->view == RESTAURANT_PAGER_VIEW_BATCH_CONFIRM)
                screen = "BATCH";
        }
        subghz_status_bar_draw(frequency, "OOK", screen, false);
        subghz_rssi_bar_draw(app->rssi);
        if (state == NULL)
        {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 0, 32, "Pager unavailable");
            u8g2_DrawStr(&m1_u8g2, 0, 42, "Not enough memory");
        }
        else if (state->view == RESTAURANT_PAGER_VIEW_EDIT)
        {
            draw_entry(selected_entry(app));
            if (state->status[0] != '\0')
            {
                u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                u8g2_DrawStr(&m1_u8g2, 0, 53, state->status);
            }
        }
        else if (state->view == RESTAURANT_PAGER_VIEW_ACTIONS)
        {
            char index[12];
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 0, 28,
                         action_labels[state->action_selection]);
            (void)snprintf(index, sizeof(index), "%u/%u",
                           (unsigned)(state->action_selection + 1U),
                           (unsigned)RESTAURANT_PAGER_ACTION_COUNT);
            u8g2_DrawStr(&m1_u8g2, 0, 40, index);
            if (state->status[0] != '\0')
                u8g2_DrawStr(&m1_u8g2, 0, 50, state->status);
        }
        else if (state->view == RESTAURANT_PAGER_VIEW_SETTINGS)
        {
            char setting[24];
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            (void)snprintf(setting, sizeof(setting), "%c Max pager: %u",
                           (state->setting_selection == 0U) ? '>' : ' ',
                           state->max_pager);
            u8g2_DrawStr(&m1_u8g2, 0, 30, setting);
            (void)snprintf(setting, sizeof(setting), "%c TX repeat: %u",
                           (state->setting_selection == 1U) ? '>' : ' ',
                           state->tx_repeat_count);
            u8g2_DrawStr(&m1_u8g2, 0, 42, setting);
            if (state->status[0] != '\0')
                u8g2_DrawStr(&m1_u8g2, 0, 52, state->status);
        }
        else if (state->view == RESTAURANT_PAGER_VIEW_BATCH_CONFIRM)
        {
            char batch[24];
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            (void)snprintf(batch, sizeof(batch), "Send pages 0-%u?",
                           state->batch_max);
            u8g2_DrawStr(&m1_u8g2, 0, 29, batch);
            u8g2_DrawStr(&m1_u8g2, 0, 42, "OK confirms; BACK cancels");
        }
        else if (state->count == 0U)
        {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 0, 32, "Listening for pages");
            u8g2_DrawStr(&m1_u8g2, 0, 42, "Princeton / SMC5326");
            if (state->status[0] != '\0')
                u8g2_DrawStr(&m1_u8g2, 0, 50, state->status);
        }
        else
        {
            draw_entry(selected_entry(app));
            if (state->status[0] != '\0')
            {
                u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                u8g2_DrawStr(&m1_u8g2, 0, 53, state->status);
            }
        }
        if (state != NULL && state->view == RESTAURANT_PAGER_VIEW_EDIT)
            subghz_button_bar_draw(NULL, "ENC", NULL, "OK:SEND", NULL,
                                   "ACT/PAGER");
        else if (state != NULL &&
                 state->view == RESTAURANT_PAGER_VIEW_ACTIONS)
            subghz_button_bar_draw(NULL, NULL, NULL, "OK:SELECT", NULL,
                                   "UP/DN BACK");
        else if (state != NULL &&
                 state->view == RESTAURANT_PAGER_VIEW_SETTINGS)
            subghz_button_bar_draw(NULL, "DEC", NULL, "INC", NULL,
                                   "OK:DONE");
        else if (state != NULL &&
                 state->view == RESTAURANT_PAGER_VIEW_BATCH_CONFIRM)
            subghz_button_bar_draw(NULL, NULL, NULL, "OK:SEND", NULL,
                                   "BACK:CANCEL");
        else
            subghz_button_bar_draw(NULL, "CFG", NULL, "OK:MENU", NULL,
                                   "ACT");
    } while (m1_u8g2_nextpage());
}

const SubGhzSceneHandlers subghz_scene_restaurant_pager_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit = scene_on_exit,
    .draw = draw,
};

void subghz_restaurant_pager_scene_deinit(SubGhzApp *app)
{
    if (app != NULL)
    {
        free(app->restaurant_pager_state);
        app->restaurant_pager_state = NULL;
    }
}
