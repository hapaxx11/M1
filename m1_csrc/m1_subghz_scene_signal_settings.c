/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_signal_settings.c
 * @brief  Sub-GHz per-file SignalSettings scene — Phase 9a-2 scaffold.
 *
 * Read-only display of the protocol-specific signal fields (Serial,
 * Button, and the encrypted HOP word) extracted from the currently-
 * loaded .sub file pointed to by `app->saved_filepath`.  Pushed by the
 * Saved action menu when the user picks the new "Settings" entry.
 *
 * Phase 9a-2 scope (this file): read-only display for the KeeLoq family
 * (KeeLoq / Star Line / Jarolift), using the host-tested pure-logic
 * extractor in `Sub_Ghz/subghz_signal_fields.c` (Phase 9a-1).  Editing
 * lands in Phases 9b (button) and 9c (counter); extension to
 * Nice FloR-S / CAME Atomo / Alutech AT-4N / Phoenix V2 lands in 9e.
 *
 * Navigation:
 *   - BACK pops back to the SavedMenu action menu.
 *   - OK is a no-op in this scaffold (will push SetButton in 9b).
 *
 * Field extraction is fully reversible (see subghz_signal_fields.h);
 * the display is non-destructive — no file I/O happens beyond the
 * `flipper_subghz_load()` invocation on scene_on_enter.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "flipper_subghz.h"
#include "subghz_signal_fields.h"
#include "subghz_keeloq.h"
#include "subghz_keeloq_mfkeys.h"

/*============================================================================*/
/* Local state                                                                 */
/*============================================================================*/

/** Cached .sub metadata loaded on scene_on_enter from app->saved_filepath. */
static flipper_subghz_signal_t s_signal;
/** Extracted plaintext fields (valid only when s_supported == true). */
static subghz_keeloq_fields_t  s_fields;
/** True when the loaded protocol is a recognised KeeLoq family member. */
static bool                    s_supported;
/** True when the file load itself succeeded — drives the error placeholder. */
static bool                    s_loaded;
/** True when the loaded file's manufacturer key was resolved and the
 *  16-bit rolling counter was decoded successfully.  When false the scene
 *  shows a "key?" placeholder instead of a numeric counter. */
static bool                    s_has_counter;
/** Decoded 16-bit rolling counter (valid iff s_has_counter == true). */
static uint16_t                s_counter;
/** Cached absolute load path ("0:/SUBGHZ/<name>") used by the Phase 9b
 *  save-back helper (`subghz_signal_settings_apply_button`).  Populated
 *  by scene_on_enter from `app->saved_filepath`; cleared on exit so a
 *  stale path can never be written to. */
static char                    s_saved_full_path[80];

/*============================================================================*/
/* Counter resolution (Phase 9c-2)                                             */
/*============================================================================*/

/* Resolve the rolling counter for the currently-extracted KeeLoq-family
 * fields by looking up the manufacturer key in the loaded mfkeys table,
 * deriving the device key via the recorded learning mode, and decrypting
 * the encrypted hop word with the host-tested pure-logic helper.
 *
 * Returns true and writes the 16-bit counter to *counter_out iff:
 *   - signal.manufacture is non-empty,
 *   - the manufacturer entry exists in the mfkeys table,
 *   - the learning mode is Normal or Simple (Secure cannot be resolved
 *     without a seed — matches subghz_keeloq_encoder.c behaviour).
 *
 * Pure-logic glue — depends only on already-tested modules.  All hardware
 * coupling (FatFS keystore I/O) is encapsulated by keeloq_mfkeys_load(),
 * which is expected to have run at boot. */
static bool resolve_counter(const flipper_subghz_signal_t *signal,
                            const subghz_keeloq_fields_t  *fields,
                            uint16_t                       *counter_out)
{
    if (!signal || !fields || !counter_out)
        return false;
    if (signal->manufacture[0] == '\0')
        return false;

    KeeLoqMfrEntry mfr;
    if (!keeloq_mfkeys_find(signal->manufacture, &mfr))
        return false;

    uint64_t device_key;
    switch (mfr.learn_type)
    {
        case KEELOQ_LEARN_NORMAL:
            device_key = keeloq_learn_normal(fields->serial, mfr.key);
            break;
        case KEELOQ_LEARN_SIMPLE:
            device_key = keeloq_learn_simple(fields->serial, mfr.key);
            break;
        case KEELOQ_LEARN_SECURE:
            /* Seed not recoverable from the .sub file — refuse rather than
             * silently fall back to Normal and produce a garbage counter. */
            return false;
        default:
            return false;
    }

    *counter_out = subghz_signal_fields_keeloq_counter_decode(fields->enc_hop,
                                                              device_key);
    return true;
}

/*============================================================================*/
/* Scene callbacks                                                             */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    /* Returning from a pushed editor (SetButton, etc.) — clear the
     * cross-scene edit context flag so a subsequent fresh entry from the
     * Saved menu starts with a clean state. */
    app->signal_edit_active = false;

    s_loaded      = false;
    s_supported   = false;
    s_has_counter = false;
    s_counter     = 0U;
    memset(&s_signal, 0, sizeof(s_signal));
    memset(&s_fields, 0, sizeof(s_fields));
    s_saved_full_path[0] = '\0';

    if (app->saved_filepath[0] == '\0')
    {
        app->need_redraw = true;
        return;
    }

    char full_path[80];
    snprintf(full_path, sizeof(full_path), "0:%s", app->saved_filepath);

    if (!flipper_subghz_load(full_path, &s_signal))
    {
        app->need_redraw = true;
        return;
    }
    s_loaded = true;
    /* Stash the absolute path so the Phase 9b save-back path can rewrite
     * the exact file we loaded from without re-deriving from app state. */
    strncpy(s_saved_full_path, full_path, sizeof(s_saved_full_path) - 1);
    s_saved_full_path[sizeof(s_saved_full_path) - 1] = '\0';

    /* Only parsed (PACKET / Key) files have a 64-bit `key` to dissect.
     * RAW captures do not — the SavedMenu must already be gating this
     * scene on parsed files, but defend against being reached otherwise. */
    if (s_signal.type != FLIPPER_SUBGHZ_TYPE_PARSED)
    {
        app->need_redraw = true;
        return;
    }

    if (subghz_signal_fields_is_keeloq_family(s_signal.protocol))
    {
        s_supported = subghz_signal_fields_keeloq_extract(s_signal.protocol,
                                                          s_signal.key,
                                                          &s_fields);
        if (s_supported)
        {
            /* Phase 9c-2 — best-effort counter resolution.  Failure
             * (missing Manufacture line, unknown manufacturer, or Secure
             * learning) leaves s_has_counter == false and the scene draws
             * a "key?" placeholder; the rest of the fields still display
             * normally so the user knows the file loaded successfully. */
            s_has_counter = resolve_counter(&s_signal, &s_fields, &s_counter);
        }
    }

    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventOk:
            /* Phase 9b — push SetButton in edit-signal mode for KeeLoq
             * family files.  Unsupported protocols, RAW files, and load
             * failures swallow the OK so the scene stays put.  Phase 9c
             * will extend this to chain a SetCounter editor as well. */
            if (s_loaded && s_supported)
            {
                app->signal_edit_active = true;
                subghz_scene_push(app, SubGhzSceneSetButton);
            }
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    (void)app;
    /* Defensive: never leave a stale absolute path resident in BSS where
     * a later mis-routed save could overwrite the wrong file. */
    s_saved_full_path[0] = '\0';
    /* Clear the counter cache so a later cross-scene accessor caller can
     * never read a stale value from a previous file. */
    s_has_counter = false;
    s_counter     = 0U;
}

/*============================================================================*/
/* Draw                                                                        */
/*============================================================================*/

static void draw_header(void)
{
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "Signal Settings");
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);
}

static void draw_placeholder(const char *line1, const char *line2)
{
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 4, 32, line1);
    if (line2)
        u8g2_DrawStr(&m1_u8g2, 4, 42, line2);
}

static void draw_keeloq_fields(void)
{
    char line[40];

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    snprintf(line, sizeof(line), "Proto: %s", s_signal.protocol);
    u8g2_DrawStr(&m1_u8g2, 2, 22, line);

    snprintf(line, sizeof(line), "Serial : 0x%07lX",
             (unsigned long)s_fields.serial);
    u8g2_DrawStr(&m1_u8g2, 2, 32, line);

    snprintf(line, sizeof(line), "Button : 0x%X",
             (unsigned)(s_fields.button & 0x0F));
    u8g2_DrawStr(&m1_u8g2, 2, 42, line);

    snprintf(line, sizeof(line), "EncHop : 0x%08lX",
             (unsigned long)s_fields.enc_hop);
    u8g2_DrawStr(&m1_u8g2, 2, 52, line);

    /* Counter — Phase 9c-2.  Resolved on scene_on_enter via the
     * manufacturer-key store + the learning mode recorded in the keystore
     * entry.  When resolution fails (no Manufacture line, manufacturer
     * absent from the keystore, or Secure learning without a seed) we
     * show a short "key?" hint so the user can tell the field is gated
     * on mfkey availability rather than the file being broken. */
    if (s_has_counter)
        snprintf(line, sizeof(line), "Counter: %u", (unsigned)s_counter);
    else
        snprintf(line, sizeof(line), "Counter: key?");
    u8g2_DrawStr(&m1_u8g2, 2, 62, line);
}

static void draw(SubGhzApp *app)
{
    (void)app;
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    draw_header();

    if (!s_loaded)
    {
        draw_placeholder("File not loaded", NULL);
    }
    else if (s_signal.type != FLIPPER_SUBGHZ_TYPE_PARSED)
    {
        draw_placeholder("RAW file —", "no parsed fields");
    }
    else if (!s_supported)
    {
        char line[40];
        snprintf(line, sizeof(line), "Proto: %s", s_signal.protocol);
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, 22, line);
        draw_placeholder("Field display not", "supported (9e)");
    }
    else
    {
        draw_keeloq_fields();
    }
    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                               */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_signal_settings_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};

/*============================================================================*/
/* Phase 9b — Cross-scene API (consumed by SubGhzSceneSetButton)              */
/*============================================================================*/

uint8_t subghz_signal_settings_get_button(void)
{
    if (!s_loaded || !s_supported)
        return 0U;
    return (uint8_t)(s_fields.button & 0x0FU);
}

bool subghz_signal_settings_apply_button(uint8_t new_button)
{
    if (!s_loaded || !s_supported)
        return false;
    if (s_signal.type != FLIPPER_SUBGHZ_TYPE_PARSED)
        return false;

    /* Substitute the button field and re-assemble the 64-bit Flipper key
     * using the host-tested pure-logic assembler (Phase 9a-1). */
    subghz_keeloq_fields_t updated = s_fields;
    updated.button = (uint8_t)(new_button & 0x0FU);

    uint64_t new_key = 0;
    if (!subghz_signal_fields_keeloq_assemble(s_signal.protocol,
                                              &updated, &new_key))
        return false;

    /* The cached absolute load path is populated by scene_on_enter; if
     * empty (no file loaded) the save is rejected.  The Manufacture line
     * is propagated verbatim — if the source file had none, the field is
     * "" and the save helper falls back to plain key-save behaviour. */
    if (s_saved_full_path[0] == '\0')
        return false;

    bool ok = flipper_subghz_save_key_with_manufacture(s_saved_full_path,
                                                       s_signal.frequency,
                                                       s_signal.preset,
                                                       s_signal.protocol,
                                                       s_signal.bit_count,
                                                       new_key,
                                                       s_signal.te,
                                                       s_signal.manufacture);
    if (!ok)
        return false;

    /* Update the cache so a redraw without re-entering the scene shows
     * the new value immediately.  The scene's on_enter will overwrite
     * this anyway when control returns. */
    s_fields    = updated;
    s_signal.key = new_key;
    return true;
}

/*============================================================================*/
/* Phase 9c-2 — Cross-scene API (counter accessors for SubGhzSceneSetCounter)  */
/*============================================================================*/

bool subghz_signal_settings_has_counter(void)
{
    return s_loaded && s_supported && s_has_counter;
}

uint16_t subghz_signal_settings_get_counter(void)
{
    if (!s_loaded || !s_supported || !s_has_counter)
        return 0U;
    return s_counter;
}
