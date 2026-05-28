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
 *
 * Phase 7c-3: action-menu rendering and scroll/selection math come from
 * the reusable `subghz_submenu_model` + `m1_submenu_draw` widget so the
 * menu honours `Settings → LCD & Notifications → Text Size` consistently
 * with the Sub-GHz top-level Menu and the Read-Raw MoreRAW menu.
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
#include "m1_submenu.h"
#include "m1_subghz_scene.h"
#include "flipper_subghz.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_protocol_registry.h"
#include "subghz_raw_decoder.h"
#include "m1_virtual_kb.h"
#include "subghz_submenu_model.h"
#include "subghz_signal_fields.h"
#include "subghz_signal_format.h"
#include "m1_subghz_button_bar.h"

extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern void subghz_pulse_handler_reset(void);
extern uint8_t subghz_get_save_fmt_ext(void);
extern bool subghz_protocol_is_static_ext(uint16_t protocol);

/*============================================================================*/
/* Action menu — unified action IDs                                           */
/*============================================================================*/

enum {
    SAVED_ACTION_DECODE = 0,
    SAVED_ACTION_EMULATE,
    SAVED_ACTION_INFO,
    SAVED_ACTION_RENAME,
    SAVED_ACTION_DELETE,
    SAVED_ACTION_SETTINGS,        /**< Phase 9a-2 — per-file SignalSettings */
};

static const char *const raw_action_labels[]    = { "Decode", "Emulate", "Info", "Rename", "Delete" };
static const char *const parsed_action_labels[] = { "Emulate", "Info", "Rename", "Delete" };
/* Parsed-file label set when the protocol supports the SignalSettings
 * scene (Phase 9a-2: KeeLoq family only; broader gating in Phase 9e). */
static const char *const parsed_settings_labels[] = { "Emulate", "Info", "Settings", "Rename", "Delete" };

#define RAW_ACTION_COUNT             5
#define PARSED_ACTION_COUNT          4
#define PARSED_SETTINGS_ACTION_COUNT 5

/* Phase 7c-3: action menu uses the reusable pure-logic submenu model.
 * Selection is reset on every scene_on_enter to match prior behaviour. */
static subghz_submenu_model_t s_action_model;
static uint8_t action_count;
static const char *const *active_labels;

static bool in_info_screen;
static bool is_raw_file;
/* Phase 9a-2 — true when the loaded parsed file's protocol is supported
 * by the SignalSettings scene; gates the "Settings" entry in the menu. */
static bool has_settings_entry;

/* Cached .sub file metadata (loaded eagerly on scene entry) */
static flipper_subghz_signal_t saved_signal;

/*============================================================================*/
/* Decode results storage                                                     */
/*============================================================================*/

#define DECODE_MAX_RESULTS   16
/** Temp .sub file written for Transmitter from the decode-results screen.
 *  Unlinked on return from Transmitter (resume_from_child path) and on
 *  real scene exit. */
#define SAVED_DECODE_TMP_PATH  "/SUBGHZ/_decode_tmp.sub"

static SubGHz_Dec_Info_t decode_results[DECODE_MAX_RESULTS];
static uint8_t  decode_count;
static uint8_t  decode_sel;
static uint8_t  decode_scroll;
static bool     in_decode_screen;
static bool     decode_detail_view;
static bool     s_decode_save_failed;  /**< Brief "Save failed" overlay flag */

static void unlink_decode_tmp(void)
{
    f_unlink(SAVED_DECODE_TMP_PATH);
}

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/**
 * @brief  Map selected action index to unified action ID.
 *         RAW files have Decode at index 0; parsed files skip it.
 *         When the SignalSettings entry is present (Phase 9a-2), it
 *         occupies index 2 between Info and Rename.
 */
static uint8_t map_action(uint8_t sel)
{
    if (is_raw_file)
        return sel;                /* Direct mapping: 0=Decode,1=Emulate,… */
    if (has_settings_entry)
    {
        /* Parsed-with-Settings layout: 0→Emulate, 1→Info, 2→Settings,
         * 3→Rename, 4→Delete. */
        if (sel == 2) return SAVED_ACTION_SETTINGS;
        if (sel <= 1) return sel + 1;   /* 0→Emulate(1), 1→Info(2) */
        return sel;                     /* 3→Rename(3), 4→Delete(4) */
    }
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

    /* Phase 9a-2/9e-1: gate the Settings entry on parsed files whose
     * protocol either has a host-tested field extractor (KeeLoq family —
     * full read-only display + Button/Counter/CounterMode editing) or
     * has a documented deferred-implementation status (Nice FloR-S /
     * CAME Atomo / Alutech AT-4N / Phoenix V2 — the scene shows a
     * clear "edit not yet supported" placeholder with the specific
     * blocker for that protocol).  All other protocols hide the entry. */
    {
        const subghz_counter_edit_status_t edit_status =
            subghz_signal_fields_counter_edit_status(saved_signal.protocol, NULL);
        has_settings_entry = (!is_raw_file)
                           && (saved_signal.type == FLIPPER_SUBGHZ_TYPE_PARSED)
                           && (edit_status == SUBGHZ_COUNTER_EDIT_SUPPORTED
                               || edit_status == SUBGHZ_COUNTER_EDIT_DEFERRED);
    }

    if (is_raw_file)
    {
        action_count  = RAW_ACTION_COUNT;
        active_labels = raw_action_labels;
    }
    else if (has_settings_entry)
    {
        action_count  = PARSED_SETTINGS_ACTION_COUNT;
        active_labels = parsed_settings_labels;
    }
    else
    {
        action_count  = PARSED_ACTION_COUNT;
        active_labels = parsed_action_labels;
    }
    subghz_submenu_model_init(&s_action_model,
                              action_count,
                              M1_MENU_VIS(action_count));
    return true;
}

static void scene_on_enter(SubGhzApp *app)
{
    /* Returning from a child scene pushed by the decode-results screen
     * (Transmitter or SaveSuccess): unlink the temp .sub if it was written
     * for Transmitter, restore the decode view, and skip the full reload. */
    if (app->resume_from_child)
    {
        app->resume_from_child = false;
        unlink_decode_tmp();
        app->need_redraw = true;
        return;
    }

    in_info_screen     = false;
    in_decode_screen   = false;
    decode_detail_view = false;
    decode_sel    = 0;
    decode_scroll = 0;

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
            s_decode_save_failed = false;
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
            app->tx_repeat_count = 5U;             /* static keys: 5 bursts for reliable TX */
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
        case SAVED_ACTION_SETTINGS:
        {
            /* Phase 9a-2 — push the read-only SignalSettings scene.  The
             * scene reloads from app->saved_filepath and dissects the
             * 64-bit key into protocol-specific fields. */
            subghz_scene_push(app, SubGhzSceneSignalSettings);
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
        /* Any keypress dismisses the "Save failed" overlay. */
        if (s_decode_save_failed)
        {
            s_decode_save_failed = false;
            app->need_redraw = true;
            return true;
        }

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
                if (decode_detail_view && decode_count > 0)
                {
                    /* Send: write temp .sub key file and push Transmitter */
                    const SubGHz_Dec_Info_t *d = &decode_results[decode_sel];
                    if (!subghz_protocol_is_static_ext(d->protocol))
                        return true;

                    uint16_t te = d->te;
                    if (te == 0 && d->protocol < subghz_protocol_registry_count)
                        te = subghz_protocols_list[d->protocol].te_short;

                    /* Use the preset from the loaded file; fall back to OOK
                     * when the file omitted the field. */
                    const char *preset = (saved_signal.preset[0] != '\0')
                                        ? saved_signal.preset
                                        : "FuriHalSubGhzPresetOok650Async";

                    if (!flipper_subghz_save_key(SAVED_DECODE_TMP_PATH,
                            d->frequency, preset,
                            protocol_text[d->protocol],
                            d->bit_len, d->key, te))
                        return true;

                    strncpy(app->tx_path, SAVED_DECODE_TMP_PATH, sizeof(app->tx_path) - 1);
                    app->tx_path[sizeof(app->tx_path) - 1] = '\0';
                    app->tx_repeat_count = 5U;
                    app->tx_mode         = 0U;
                    strncpy(app->tx_protocol_name, protocol_text[d->protocol],
                            sizeof(app->tx_protocol_name) - 1);
                    app->tx_protocol_name[sizeof(app->tx_protocol_name) - 1] = '\0';
                    app->resume_from_child = true;
                    subghz_scene_push(app, SubGhzSceneTransmitter);
                }
                else if (!decode_detail_view && decode_count > 0)
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
                if (decode_detail_view && decode_count > 0)
                {
                    /* Save decoded signal */
                    const SubGHz_Dec_Info_t *d = &decode_results[decode_sel];
                    char default_name[32];
                    snprintf(default_name, sizeof(default_name), "%.12s_%lX",
                             protocol_text[d->protocol],
                             (unsigned long)(uint32_t)d->key);

                    char new_name[32];
                    if (!m1_vkb_get_filename("Save signal as:", default_name, new_name))
                    {
                        app->need_redraw = true;
                        return true;
                    }

                    /* Reject empty names — an empty path segment causes
                     * f_open to fail silently. */
                    if (new_name[0] == '\0')
                    {
                        app->need_redraw = true;
                        return true;
                    }

                    uint16_t te = d->te;
                    if (te == 0 && d->protocol < subghz_protocol_registry_count)
                        te = subghz_protocols_list[d->protocol].te_short;

                    /* Use the preset from the loaded file; fall back to OOK
                     * when the file omitted the field. */
                    const char *preset = (saved_signal.preset[0] != '\0')
                                        ? saved_signal.preset
                                        : "FuriHalSubGhzPresetOok650Async";

                    uint8_t fmt = subghz_get_save_fmt_ext();
                    const char *ext = (fmt == 1) ? ".sgh" : ".sub";
                    char save_path[80];
                    snprintf(save_path, sizeof(save_path), "/SUBGHZ/%s%s", new_name, ext);

                    f_mkdir("/SUBGHZ");
                    bool saved;
                    if (fmt == 1)
                        saved = flipper_subghz_save_m1native_key(save_path,
                                    d->frequency, preset,
                                    protocol_text[d->protocol],
                                    d->bit_len, d->key, te);
                    else
                        saved = flipper_subghz_save_key(save_path,
                                    d->frequency, preset,
                                    protocol_text[d->protocol],
                                    d->bit_len, d->key, te);

                    if (saved)
                    {
                        strncpy(app->file_path, save_path, sizeof(app->file_path) - 1);
                        app->file_path[sizeof(app->file_path) - 1] = '\0';
                        app->resume_from_child = true;
                        subghz_scene_push(app, SubGhzSceneSaveSuccess);
                    }
                    else
                    {
                        s_decode_save_failed = true;
                        app->need_redraw = true;
                    }
                }
                else if (!decode_detail_view && decode_count > 0)
                {
                    if (decode_sel + 1 < decode_count)
                        decode_sel++;
                    uint8_t vis = M1_MENU_VIS(decode_count);
                    if (decode_sel >= decode_scroll + vis)
                        decode_scroll = decode_sel - vis + 1;
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

    /* Re-sync visible_count in case the user changed the text-size
     * preference while a child scene was on top. */
    subghz_submenu_model_set_visible_count(&s_action_model,
                                           M1_MENU_VIS(action_count));

    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            subghz_submenu_model_up(&s_action_model);
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            subghz_submenu_model_down(&s_action_model);
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
            handle_action(app, map_action(s_action_model.selected));
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    /* When handing off to a child scene from the decode-results screen,
     * the temp .sub must survive until scene_on_enter reclaims it on
     * return.  resume_from_child is set immediately before the push so it
     * acts as the "do not unlink" gate here, matching the SetKey pattern. */
    if (app && app->resume_from_child)
        return;
    unlink_decode_tmp();
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
        /* Phase 11-1: consult the protocol's polymorphic get_string vtable
         * slot when present.  Currently installed only on the KeeLoq family
         * (KeeLoq / Star Line / Jarolift); all other parsed protocols fall
         * through to the generic Proto / Key / Bits / TE layout below. */
        int16_t proto_idx = subghz_protocol_find_by_name(saved_signal.protocol);
        const SubGhzProtocolDef *proto =
            (proto_idx >= 0) ? subghz_protocol_get((uint16_t)proto_idx) : NULL;

        if (proto && proto->get_string)
        {
            char info[96];
            SubGhzSignalView view = {
                .protocol  = saved_signal.protocol,
                .key       = saved_signal.key,
                .bit_count = saved_signal.bit_count,
                .te        = saved_signal.te,
            };
            proto->get_string(&view, info, sizeof(info));

            /* Render up to 3 newline-separated rows from the formatter
             * output at y=22/30/38, leaving y=46 for Freq and y=54 for Mod. */
            int y = 22;
            const char *p = info;
            for (int row = 0; row < 3 && *p; row++)
            {
                size_t n = 0;
                while (p[n] && p[n] != '\n' && n < sizeof(line) - 1)
                {
                    line[n] = p[n];
                    n++;
                }
                line[n] = '\0';
                if (n > 0)
                    u8g2_DrawStr(&m1_u8g2, 2, y, line);
                y += 8;
                if (p[n] == '\n') p += n + 1;
                else              p += n;
            }
        }
        else
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
    const uint8_t item_h   = m1_menu_item_h();
    const uint8_t text_ofs = item_h - 1;

    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Decode Results", TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

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

        if (s_decode_save_failed)
        {
            /* Brief "Save failed" overlay — any key will dismiss it */
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
            m1_draw_text(&m1_u8g2, 0, 30, M1_LCD_DISPLAY_WIDTH,
                         "Save failed", TEXT_ALIGN_CENTER);
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            m1_draw_text(&m1_u8g2, 0, 42, M1_LCD_DISPLAY_WIDTH,
                         "Press any key", TEXT_ALIGN_CENTER);
            return;
        }

        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, 22, protocol_text[d->protocol]);

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

        snprintf(line, sizeof(line), "Key: 0x%lX  %dbit",
                 (uint32_t)d->key, d->bit_len);
        u8g2_DrawStr(&m1_u8g2, 2, 30, line);

        /* Display TE — use protocol-specified value when available */
        {
            uint16_t te_display = d->te;
            if (te_display == 0 && d->protocol < subghz_protocol_registry_count)
                te_display = subghz_protocols_list[d->protocol].te_short;
            snprintf(line, sizeof(line), "TE: %d us", te_display);
            u8g2_DrawStr(&m1_u8g2, 2, 38, line);
        }

        /* Use integer arithmetic — embedded printf (nano.specs) has no %f support */
        snprintf(line, sizeof(line), "Freq: %lu.%02lu MHz",
                 (unsigned long)(d->frequency / 1000000UL),
                 (unsigned long)((d->frequency % 1000000UL) / 10000UL));
        u8g2_DrawStr(&m1_u8g2, 2, 46, line);

        /* Bottom bar — Send (OK) / Save (DOWN) */
        bool can_send = subghz_protocol_is_static_ext(d->protocol);
        subghz_button_bar_draw(
            arrowdown_8x8, "Save",
            NULL, can_send ? "Send" : NULL,
            NULL, NULL);
    }
    else
    {
        /* List view */
        u8g2_SetFont(&m1_u8g2, m1_menu_font());
        uint8_t vis = M1_MENU_VIS(decode_count);

        for (uint8_t i = 0; i < vis && (decode_scroll + i) < decode_count; i++)
        {
            uint8_t idx = decode_scroll + i;
            const SubGHz_Dec_Info_t *d = &decode_results[idx];
            uint8_t y = (uint8_t)(M1_MENU_AREA_TOP + i * item_h);

            if (idx == decode_sel)
            {
                u8g2_DrawRBox(&m1_u8g2, M1_MENU_HIGHLIGHT_X, y,
                              M1_MENU_TEXT_W, item_h, 2);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            }

            snprintf(line, sizeof(line), "%s 0x%lX",
                     protocol_text[d->protocol], (uint32_t)d->key);
            u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, line);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }

        /* Scrollbar */
        if (decode_count > 0)
        {
            uint8_t sb_area_h   = vis * item_h;
            uint8_t sb_handle_h = sb_area_h / decode_count;
            if (sb_handle_h < 6)
                sb_handle_h = 6;
            uint8_t sb_travel_h = (sb_area_h > sb_handle_h)
                                  ? (sb_area_h - sb_handle_h) : 0;
            uint8_t sb_handle_y = M1_MENU_AREA_TOP;
            if (decode_count > 1)
                sb_handle_y += (uint8_t)((uint16_t)sb_travel_h * decode_sel
                               / (decode_count - 1));

            u8g2_DrawVLine(&m1_u8g2,
                           M1_MENU_SCROLLBAR_X + M1_MENU_SCROLLBAR_W / 2,
                           M1_MENU_AREA_TOP, sb_area_h);
            u8g2_DrawRBox(&m1_u8g2,
                          M1_MENU_SCROLLBAR_X, sb_handle_y,
                          M1_MENU_SCROLLBAR_W, sb_handle_h, 1);
        }
    }
}

static void draw_action_menu(const SubGhzApp *app)
{
    /* Phase 7c-3: render via the reusable submenu widget.  The filename
     * (truncated to a manageable length) is used as the title; the model
     * derives row height, scroll math, and scrollbar geometry from the
     * user's text-size preference. */
    char title[22];
    strncpy(title, app->saved_filename, sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';

    /* Re-sync visible_count for the current text-size setting before
     * drawing — matches the Phase 7c-1 / 7c-2 pattern. */
    subghz_submenu_model_set_visible_count(&s_action_model,
                                           M1_MENU_VIS(action_count));
    m1_submenu_draw(&s_action_model, title, active_labels);
}

static void draw(SubGhzApp *app)
{
    if (in_decode_screen)
    {
        m1_u8g2_firstpage();
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        draw_decode_screen();
        m1_u8g2_nextpage();
    }
    else if (in_info_screen)
    {
        m1_u8g2_firstpage();
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        draw_info_screen();
        m1_u8g2_nextpage();
    }
    else
    {
        /* m1_submenu_draw owns its own firstpage/nextpage. */
        draw_action_menu(app);
    }
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
