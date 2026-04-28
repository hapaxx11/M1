/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_saved.c
 * @brief  Sub-GHz Saved Scene — browse 0:/SUBGHZ/ files with action menu.
 *
 * Uses the existing storage_browse() file browser.  After selection,
 * shows an action menu whose contents depend on the file type:
 *
 *   RAW files:    Decode, Emulate, Info, Rename, Delete
 *   Parsed files: Emulate, Info, Rename, Delete
 *
 * "Emulate" for RAW files pushes into the Read Raw scene in Loaded state
 * (Momentum's LoadKeyIDLE), giving the user a waveform viewer with Send/New
 * buttons rather than a blind one-shot inline replay.
 *
 * "Emulate" for parsed files performs an inline blocking replay from this scene
 * (unchanged from the original behaviour).
 *
 * The "Decode" action (RAW only) feeds raw pulse data through the protocol
 * decoder registry offline, similar to Momentum firmware's decode feature.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_storage.h"
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"
#include "flipper_subghz.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_protocol_registry.h"
#include "subghz_raw_decoder.h"
#include "m1_virtual_kb.h"

extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern void subghz_pulse_handler_reset(void);
extern bool subghz_decenc_read(SubGHz_Dec_Info_t *out, bool raw_mode);

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

static uint8_t action_sel  = 0;
static uint8_t action_count = PARSED_ACTION_COUNT;
static const char **active_labels = parsed_action_labels;

static char saved_filepath[64];
static char saved_filename[32];
static bool in_action_menu  = false;
static bool in_info_screen  = false;
static bool is_raw_file     = false;

/* Cached .sub file metadata (loaded eagerly on file selection) */
static flipper_subghz_signal_t saved_signal;

/*============================================================================*/
/* Decode results storage                                                     */
/*============================================================================*/

#define DECODE_MAX_RESULTS   16
#define DECODE_VISIBLE        3   /* visible rows in decode list */
#define DECODE_ROW_H          8

static SubGHz_Dec_Info_t decode_results[DECODE_MAX_RESULTS];
static uint8_t  decode_count       = 0;
static uint8_t  decode_sel         = 0;
static uint8_t  decode_scroll      = 0;
static bool     in_decode_screen   = false;
static bool     decode_detail_view = false;

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

/**
 * @brief  Open the 0:/SUBGHZ/ file browser and populate action menu state.
 *         On selection, eagerly loads the file metadata to determine if it
 *         is a RAW or parsed .sub file and adjusts the action menu accordingly.
 *
 * @retval true   A file was selected (action menu ready).
 * @retval false  User cancelled the browser (caller should pop the scene).
 */
static bool open_saved_browser(void)
{
    S_M1_file_info *f_info = storage_browse("0:/SUBGHZ");
    if (f_info && f_info->file_is_selected)
    {
        snprintf(saved_filepath, sizeof(saved_filepath), "/SUBGHZ/%s",
                 f_info->file_name);
        strncpy(saved_filename, f_info->file_name, sizeof(saved_filename) - 1);
        saved_filename[sizeof(saved_filename) - 1] = '\0';

        /* Load file metadata to determine type (RAW vs parsed) */
        char full_path[72];
        snprintf(full_path, sizeof(full_path), "0:%s", saved_filepath);
        memset(&saved_signal, 0, sizeof(saved_signal));
        bool loaded = flipper_subghz_load(full_path, &saved_signal);

        if (!loaded)
        {
            /* Load failed — don't enter action menu with invalid state */
            is_raw_file = false;
            in_action_menu = false;
            action_count = 0;
            action_sel = 0;
            return false;
        }

        is_raw_file = (saved_signal.type == FLIPPER_SUBGHZ_TYPE_RAW
                       && saved_signal.raw_count > 0);

        if (is_raw_file)
        {
            action_count = RAW_ACTION_COUNT;
            active_labels = raw_action_labels;
        }
        else
        {
            action_count = PARSED_ACTION_COUNT;
            active_labels = parsed_action_labels;
        }

        in_action_menu = true;
        action_sel = 0;
        return true;
    }
    return false;
}

/*============================================================================*/
/* Offline decode — feed raw pulse data through protocol decoders             */
/*============================================================================*/

/**
 * @brief  ARM-side decode callback for subghz_decode_raw_offline().
 *
 * Copies pulse data into the global subghz_decenc_ctl buffer, then
 * iterates through the protocol registry.  On success, reads the
 * decoded result from the global state.
 */
static bool decode_try_fn(const uint16_t *pulse_buf,
                           uint16_t pulse_count,
                           SubGhzRawDecodeResult *out_result,
                           void *user_ctx)
{
    (void)user_ctx;

    /* Copy pulse data into the global buffer that decoders read from */
    memcpy(subghz_decenc_ctl.pulse_times, pulse_buf,
           pulse_count * sizeof(uint16_t));
    subghz_decenc_ctl.npulsecount = pulse_count;

    /* Try every registered protocol decoder */
    for (uint16_t p = 0; p < subghz_protocol_registry_count; p++)
    {
        const SubGhzProtocolDef *proto = &subghz_protocol_registry[p];
        if (proto->decode && proto->decode(p, pulse_count) == 0)
        {
            SubGHz_Dec_Info_t info;
            if (subghz_decenc_read(&info, false))
            {
                out_result->protocol      = info.protocol;
                out_result->key           = info.key;
                out_result->bit_len       = info.bit_len;
                out_result->te            = info.te;
                out_result->serial_number = info.serial_number;
                out_result->rolling_code  = info.rolling_code;
                out_result->button_id     = info.button_id;
                return true;
            }
            break;
        }
    }
    return false;
}

/**
 * @brief  Attempt to decode protocols from a loaded RAW .sub file.
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
        decode_try_fn,
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

static void scene_on_enter(SubGhzApp *app)
{
    in_action_menu     = false;
    in_info_screen     = false;
    in_decode_screen   = false;
    decode_detail_view = false;
    action_sel = 0;

    /* Open file browser immediately — no intermediate prompt screen */
    if (!open_saved_browser())
    {
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
            /* Offline protocol decode of RAW signal data.
             * saved_signal was already loaded in open_saved_browser(). */
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
                strncpy(app->raw_load_path, saved_filepath, sizeof(app->raw_load_path) - 1);
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

            /* Parsed (static-key) files: inline blocking replay in this scene.
             * NOISE (RAW) files always take the if (is_raw_file) branch above
             * and open the Read Raw scene — they never reach this path.
             * Here we only handle PACKET/key files which need the key encoder
             * to reconstruct the OOK waveform from protocol + key fields. */
            {
                uint8_t ret;

                ret = sub_ghz_replay_flipper_file(saved_filepath);

                if (ret == 0)
                {
                    /* Restore radio to known state after replay (the replay
                     * function calls menu_sub_ghz_exit which powers off the
                     * SI4463 on success). */
                    menu_sub_ghz_init();
                }
                else
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
                        case 6: err = "Cannot replay: dynamic key";   break;
                        case 7: err = "Unsupported protocol";       break;
                    }
                    m1_message_box(&m1_u8g2, "Emulate failed", err, "",
                                   "BACK to return");
                }
                app->need_redraw = true;
                return true;
            }
        }
        case SAVED_ACTION_INFO:
        {
            /* saved_signal was already loaded in open_saved_browser() */
            in_info_screen = true;
            app->need_redraw = true;
            return true;
        }
        case SAVED_ACTION_RENAME:
        {
            char new_name[32];
            /* Strip extension for rename prompt */
            char base[32];
            strncpy(base, saved_filename, sizeof(base) - 1);
            base[sizeof(base) - 1] = '\0';
            char *dot = strrchr(base, '.');
            if (dot) *dot = '\0';

            if (m1_vkb_get_filename("Rename to:", base, new_name))
            {
                char old_path[72], new_path[72];
                snprintf(old_path, sizeof(old_path), "0:%s", saved_filepath);
                /* Determine extension */
                const char *ext = strrchr(saved_filename, '.');
                if (!ext) ext = ".sub";
                snprintf(new_path, sizeof(new_path), "0:/SUBGHZ/%s%s", new_name, ext);
                f_rename(old_path, new_path);
            }
            /* Return to file browser after rename */
            if (!open_saved_browser())
                subghz_scene_pop(app);
            app->need_redraw = true;
            return true;
        }
        case SAVED_ACTION_DELETE:
        {
            /* Confirm deletion */
            uint8_t confirm = m1_message_box_choice(&m1_u8g2,
                "Delete file?", saved_filename, "", "OK  /  Cancel");
            if (confirm == 1)
            {
                char del_path[72];
                snprintf(del_path, sizeof(del_path), "0:%s", saved_filepath);
                f_unlink(del_path);
                /* Return to file browser after delete */
                if (!open_saved_browser())
                    subghz_scene_pop(app);
            }
            app->need_redraw = true;
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
            /* Re-open file browser; pop if user cancels */
            if (!open_saved_browser())
                subghz_scene_pop(app);
            app->need_redraw = true;
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
                u8g2_DrawBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH, DECODE_ROW_H);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            }

            snprintf(line, sizeof(line), "%s 0x%lX",
                     protocol_text[d->protocol], (uint32_t)d->key);
            u8g2_DrawStr(&m1_u8g2, 2, y + 6, line);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
    }
}

static void draw_action_menu(void)
{
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
    char dname[22];
    strncpy(dname, saved_filename, 21);
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
            u8g2_DrawBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH, row_h);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }
        u8g2_DrawStr(&m1_u8g2, 8, y + text_ofs, active_labels[i]);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }
}

static void draw(SubGhzApp *app)
{
    (void)app;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    if (in_decode_screen)
        draw_decode_screen();
    else if (in_info_screen)
        draw_info_screen();
    else
        draw_action_menu();

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_saved_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
