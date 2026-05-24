/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_saved_menu.c
 * @brief  Sub-GHz Saved-file action menu scene (Phase 5).
 *
 * Pushed by the Saved file-browser scene once a file is selected.  Owns
 * the action menu (Decode / Emulate / Info / Rename / Delete), the
 * informational "Signal Info" screen, and the offline decode-results
 * screen.
 *
 *   RAW files:    Decode, Emulate, Info, Rename, Delete
 *   Parsed files: Emulate, Info, Rename, Delete
 *
 * "Emulate" for RAW files pushes into the Read Raw scene in Loaded state
 * (Momentum's LoadKeyIDLE).  "Emulate" for parsed files pushes the
 * Transmitter scene which drives the async-TX state machine.
 *
 * "Delete" pushes the dedicated Delete confirmation scene.
 *
 * Phase 5 split: action menu / info / decode previously lived inside
 * `m1_subghz_scene_saved.c`; that scene is now a pure file browser.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_storage.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "flipper_subghz.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_protocol_registry.h"
#include "subghz_raw_decoder.h"
#include "m1_virtual_kb.h"

extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern void subghz_pulse_handler_reset(void);

/*============================================================================*/
/* Action menu — unified action IDs                                           */
/*============================================================================*/

enum {
    SAVED_ACTION_DECODE = 0,
    SAVED_ACTION_EMULATE,
    SAVED_ACTION_INFO,
    SAVED_ACTION_RENAME,
    SAVED_ACTION_DELETE,
};

static const char *raw_action_labels[]    = { "Decode", "Emulate", "Info", "Rename", "Delete" };
static const char *parsed_action_labels[] = { "Emulate", "Info", "Rename", "Delete" };

#define RAW_ACTION_COUNT    5
#define PARSED_ACTION_COUNT 4

static uint8_t action_sel;
static uint8_t action_count;
static const char **active_labels;

static bool in_info_screen;
static bool is_raw_file;

/* Cached .sub file metadata (loaded eagerly on scene entry) */
static flipper_subghz_signal_t saved_signal;

/*============================================================================*/
/* Decode results storage                                                     */
/*============================================================================*/

#define DECODE_MAX_RESULTS   16
#define DECODE_VISIBLE        3   /* visible rows in decode list */
#define DECODE_ROW_H          8

static SubGHz_Dec_Info_t decode_results[DECODE_MAX_RESULTS];
static uint8_t  decode_count;
static uint8_t  decode_sel;
static uint8_t  decode_scroll;
static bool     in_decode_screen;
static bool     decode_detail_view;

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/**
 * @brief  Map selected action index to unified action ID.
 *         RAW files have Decode at index 0; parsed files skip it.
 */
static uint8_t map_action(uint8_t sel)
{
    if (is_raw_file)
        return sel;                /* Direct mapping: 0=Decode,1=Emulate,… */
    return sel + 1;                /* Skip Decode: 0→Emulate(1), 1→Info(2),… */
}

/*============================================================================*/
/* Offline decode — feed raw pulse data through protocol decoders             */
/*============================================================================*/

/**
 * @brief  Attempt to decode protocols from the loaded RAW .sub file.
 *
 * Delegates to the extracted subghz_decode_raw_offline() engine,
 * providing an ARM-side callback that bridges to the global decoder state.
 *
 * Results are stored in decode_results[].
 *
 * @retval true   At least one protocol was decoded.
 * @retval false  No protocols found.
 */
static bool do_decode_raw(void)
{
    decode_count = 0;

    if (saved_signal.type != FLIPPER_SUBGHZ_TYPE_RAW || saved_signal.raw_count == 0)
        return false;

    /* Reset decoder global state for a clean session */
    subghz_pulse_handler_reset();
    subghz_decenc_ctl.ndecodedrssi = 0;   /* RSSI meaningless for file decode */

    SubGhzRawDecodeResult raw_results[DECODE_MAX_RESULTS];
    uint8_t count = subghz_decode_raw_offline(
        saved_signal.raw_data,
        saved_signal.raw_count,
        saved_signal.frequency,
        raw_results,
        DECODE_MAX_RESULTS,
        subghz_registry_decode_try_fn,
        NULL);

    /* Copy results into the scene-local format */
    for (uint8_t i = 0; i < count && i < DECODE_MAX_RESULTS; i++)
    {
        decode_results[i].protocol      = raw_results[i].protocol;
        decode_results[i].key           = raw_results[i].key;
        decode_results[i].bit_len       = raw_results[i].bit_len;
        decode_results[i].te            = raw_results[i].te;
        decode_results[i].frequency     = raw_results[i].frequency;
        decode_results[i].rssi          = raw_results[i].rssi;
        decode_results[i].serial_number = raw_results[i].serial_number;
        decode_results[i].rolling_code  = raw_results[i].rolling_code;
        decode_results[i].button_id     = raw_results[i].button_id;
        decode_results[i].raw           = false;
        decode_results[i].raw_data      = NULL;
    }
    decode_count = count;

    return (decode_count > 0);
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

/**
 * @brief  Load the file metadata from app->saved_filepath into saved_signal.
 *
 * Called from scene_on_enter both on first push and on resume-from-child
 * (after Rename / Delete-cancel / etc.) so the action menu always reflects
 * the up-to-date file state.
 *
 * @retval true   File loaded successfully.
 * @retval false  Load failed — caller should pop the scene.
 */
static bool load_signal(const SubGhzApp *app)
{
    if (app->saved_filepath[0] == '\0')
        return false;

    char full_path[80];
    snprintf(full_path, sizeof(full_path), "0:%s", app->saved_filepath);

    memset(&saved_signal, 0, sizeof(saved_signal));
    if (!flipper_subghz_load(full_path, &saved_signal))
        return false;

    is_raw_file = (saved_signal.type == FLIPPER_SUBGHZ_TYPE_RAW
                   && saved_signal.raw_count > 0);

    if (is_raw_file)
    {
        action_count  = RAW_ACTION_COUNT;
        active_labels = raw_action_labels;
    }
    else
    {
        action_count  = PARSED_ACTION_COUNT;
        active_labels = parsed_action_labels;
    }
    return true;
}

static void scene_on_enter(SubGhzApp *app)
{
    in_info_screen     = false;
    in_decode_screen   = false;
    decode_detail_view = false;
    decode_sel    = 0;
    decode_scroll = 0;
    action_sel    = 0;

    if (!load_signal(app))
    {
        /* File missing / unreadable (e.g. deleted) — return to file browser. */
        subghz_scene_pop(app);
        return;
    }
    app->need_redraw = true;
}

static bool handle_action(SubGhzApp *app, uint8_t action)
{
    switch (action)
    {
        case SAVED_ACTION_DECODE:
        {
            /* Offline protocol decode of RAW signal data. */
            do_decode_raw();
            decode_sel = 0;
            decode_scroll = 0;
            decode_detail_view = false;
            in_decode_screen = true;
            app->need_redraw = true;
            return true;
        }
        case SAVED_ACTION_EMULATE:
        {
            if (is_raw_file)
            {
                /* RAW files: open in the Read Raw scene in Loaded state.
                 * This is Momentum's LoadKeyIDLE path — the user gets a proper
                 * waveform viewer with Send / New buttons rather than a blind
                 * one-shot replay that returns immediately.
                 *
                 * Pass replay metadata via app context so Read Raw knows which
                 * replay function to use (direct for .sgh, flipper for .sub). */
                strncpy(app->raw_load_path, app->saved_filepath, sizeof(app->raw_load_path) - 1);
                app->raw_load_path[sizeof(app->raw_load_path) - 1] = '\0';
                app->raw_load_is_native = saved_signal.is_m1_native;
                app->raw_load_freq_hz   = saved_signal.frequency;
                {
                    uint8_t mod = flipper_subghz_preset_to_modulation(saved_signal.preset);
                    app->raw_load_mod = (mod == MODULATION_UNKNOWN) ? MODULATION_OOK : mod;
                }
                subghz_scene_push(app, SubGhzSceneReadRaw);
                return true;
            }

            /* Parsed (static-key) files: push the Transmitter scene which
             * drives the async-TX state machine on top of our scene. */
            strncpy(app->tx_path, app->saved_filepath, sizeof(app->tx_path) - 1);
            app->tx_path[sizeof(app->tx_path) - 1] = '\0';
            app->tx_repeat_count = 1U;             /* static keys: 1 burst */
            app->tx_mode         = 0U;             /* SUBGHZ_TX_MODE_SINGLE */
            /* Phase 4b — propagate the parsed protocol name so the
             * Transmitter scene can query button-cycling capability. */
            strncpy(app->tx_protocol_name, saved_signal.protocol,
                    sizeof(app->tx_protocol_name) - 1);
            app->tx_protocol_name[sizeof(app->tx_protocol_name) - 1] = '\0';
            subghz_scene_push(app, SubGhzSceneTransmitter);
            return true;
        }
        case SAVED_ACTION_INFO:
        {
            in_info_screen = true;
            app->need_redraw = true;
            return true;
        }
        case SAVED_ACTION_RENAME:
        {
            char new_name[32];
            /* Strip extension for rename prompt */
            char base[32];
            strncpy(base, app->saved_filename, sizeof(base) - 1);
            base[sizeof(base) - 1] = '\0';
            char *dot = strrchr(base, '.');
            if (dot) *dot = '\0';

            if (m1_vkb_get_filename("Rename to:", base, new_name))
            {
                char old_path[80], new_path[80];
                snprintf(old_path, sizeof(old_path), "0:%s", app->saved_filepath);
                /* Determine extension */
                const char *ext = strrchr(app->saved_filename, '.');
                if (!ext) ext = ".sub";
                snprintf(new_path, sizeof(new_path), "0:/SUBGHZ/%s%s", new_name, ext);
                f_rename(old_path, new_path);
            }
            /* The renamed (or unchanged) file is no longer guaranteed to
             * exist at app->saved_filepath — return to the file browser. */
            subghz_scene_pop(app);
            return true;
        }
        case SAVED_ACTION_DELETE:
        {
            /* Hand off to the dedicated confirmation scene.  On confirm,
             * the Delete scene unlinks the file and pops back through us
             * to the Saved file browser; on cancel, it pops back to us. */
            subghz_scene_push(app, SubGhzSceneDelete);
            return true;
        }
    }
    return false;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    /* --- Decode results screen --- */
    if (in_decode_screen)
    {
        switch (event)
        {
            case SubGhzEventBack:
                if (decode_detail_view)
                {
                    decode_detail_view = false;
                    app->need_redraw = true;
                }
                else
                {
                    in_decode_screen = false;
                    app->need_redraw = true;
                }
                return true;

            case SubGhzEventOk:
                if (!decode_detail_view && decode_count > 0)
                {
                    decode_detail_view = true;
                    app->need_redraw = true;
                }
                return true;

            case SubGhzEventUp:
                if (!decode_detail_view && decode_count > 0)
                {
                    if (decode_sel > 0)
                        decode_sel--;
                    if (decode_sel < decode_scroll)
                        decode_scroll = decode_sel;
                    app->need_redraw = true;
                }
                return true;

            case SubGhzEventDown:
                if (!decode_detail_view && decode_count > 0)
                {
                    if (decode_sel + 1 < decode_count)
                        decode_sel++;
                    if (decode_sel >= decode_scroll + DECODE_VISIBLE)
                        decode_scroll = decode_sel - DECODE_VISIBLE + 1;
                    app->need_redraw = true;
                }
                return true;

            default:
                break;
        }
        return false;
    }

    /* --- Info screen --- */
    if (in_info_screen)
    {
        if (event == SubGhzEventBack)
        {
            in_info_screen = false;
            app->need_redraw = true;
            return true;
        }
        return false;
    }

    /* --- Action menu --- */
    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            action_sel = (action_sel > 0) ? action_sel - 1 : action_count - 1;
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            action_sel = (action_sel + 1) % action_count;
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
            handle_action(app, map_action(action_sel));
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    (void)app;
}

/*============================================================================*/
/* Draw helpers                                                               */
/*============================================================================*/

static void draw_info_screen(void)
{
    char line[48];

    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "Signal Info");
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    if (saved_signal.type == FLIPPER_SUBGHZ_TYPE_PARSED)
    {
        snprintf(line, sizeof(line), "Proto: %s", saved_signal.protocol);
        u8g2_DrawStr(&m1_u8g2, 2, 22, line);

        snprintf(line, sizeof(line), "Key: 0x%lX", (uint32_t)saved_signal.key);
        u8g2_DrawStr(&m1_u8g2, 2, 30, line);

        snprintf(line, sizeof(line), "Bits: %lu  TE: %lu us",
                 (unsigned long)saved_signal.bit_count,
                 (unsigned long)saved_signal.te);
        u8g2_DrawStr(&m1_u8g2, 2, 38, line);
    }
    else
    {
        u8g2_DrawStr(&m1_u8g2, 2, 22, "Type: RAW");

        snprintf(line, sizeof(line), "Samples: %u", saved_signal.raw_count);
        u8g2_DrawStr(&m1_u8g2, 2, 30, line);
    }

    /* Use integer arithmetic — embedded printf (nano.specs) has no %f support */
    snprintf(line, sizeof(line), "Freq: %lu.%02lu MHz",
             (unsigned long)(saved_signal.frequency / 1000000UL),
             (unsigned long)((saved_signal.frequency % 1000000UL) / 10000UL));
    u8g2_DrawStr(&m1_u8g2, 2, 46, line);

    if (saved_signal.preset[0])
    {
        snprintf(line, sizeof(line), "Mod: %s", saved_signal.preset);
        u8g2_DrawStr(&m1_u8g2, 2, 54, line);
    }
}

static void draw_decode_screen(void)
{
    char line[48];

    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "Decode Results");
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    if (decode_count == 0)
    {
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 10, 32, "No protocols");
        u8g2_DrawStr(&m1_u8g2, 10, 44, "decoded");
        return;
    }

    if (decode_detail_view)
    {
        /* Detail view of selected decoded signal */
        const SubGHz_Dec_Info_t *d = &decode_results[decode_sel];

        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, 24, protocol_text[d->protocol]);

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

        snprintf(line, sizeof(line), "Key: 0x%lX  %dbit",
                 (uint32_t)d->key, d->bit_len);
        u8g2_DrawStr(&m1_u8g2, 2, 34, line);

        snprintf(line, sizeof(line), "TE: %d us", d->te);
        u8g2_DrawStr(&m1_u8g2, 2, 43, line);

        /* Use integer arithmetic — embedded printf (nano.specs) has no %f support */
        snprintf(line, sizeof(line), "Freq: %lu.%02lu MHz",
                 (unsigned long)(d->frequency / 1000000UL),
                 (unsigned long)((d->frequency % 1000000UL) / 10000UL));
        u8g2_DrawStr(&m1_u8g2, 2, 52, line);

        if (d->serial_number != 0 || d->rolling_code != 0)
        {
            snprintf(line, sizeof(line), "SN: %lX RC: %lX",
                     (unsigned long)d->serial_number,
                     (unsigned long)d->rolling_code);
            u8g2_DrawStr(&m1_u8g2, 2, 61, line);
        }
    }
    else
    {
        /* List view — show decoded count + scrollable list */
        snprintf(line, sizeof(line), "Decoded: %d", decode_count);
        u8g2_DrawStr(&m1_u8g2, 2, 22, line);

        uint8_t vis = DECODE_VISIBLE;
        if (vis > decode_count) vis = decode_count;

        for (uint8_t i = 0; i < vis; i++)
        {
            uint8_t idx = decode_scroll + i;
            if (idx >= decode_count) break;

            const SubGHz_Dec_Info_t *d = &decode_results[idx];
            uint8_t y = 24 + i * DECODE_ROW_H;

            if (idx == decode_sel)
            {
                u8g2_DrawRBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH, DECODE_ROW_H, 2);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            }

            snprintf(line, sizeof(line), "%s 0x%lX",
                     protocol_text[d->protocol], (uint32_t)d->key);
            u8g2_DrawStr(&m1_u8g2, 2, y + 6, line);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
    }
}

static void draw_action_menu(const SubGhzApp *app)
{
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    char dname[22];
    strncpy(dname, app->saved_filename, 21);
    dname[21] = '\0';
    u8g2_DrawStr(&m1_u8g2, 2, 10, dname);

    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    /* Compute row height: 50px available (y=14..63) */
    uint8_t row_h = 50 / action_count;          /* 12 for 4, 10 for 5 */
    uint8_t text_ofs = (row_h >= 12) ? 9 : 8;   /* text baseline offset */

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    for (uint8_t i = 0; i < action_count; i++)
    {
        uint8_t y = 14 + i * row_h;
        if (i == action_sel)
        {
            u8g2_DrawRBox(&m1_u8g2, 1, y, M1_MENU_TEXT_W, row_h, 2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }
        u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, active_labels[i]);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }
}

static void draw(SubGhzApp *app)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    if (in_decode_screen)
        draw_decode_screen();
    else if (in_info_screen)
        draw_info_screen();
    else
        draw_action_menu(app);

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_saved_menu_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
