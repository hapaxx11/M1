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

/*============================================================================*/
/* Scene callbacks                                                             */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    s_loaded    = false;
    s_supported = false;
    memset(&s_signal, 0, sizeof(s_signal));
    memset(&s_fields, 0, sizeof(s_fields));

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
            /* No-op in the 9a-2 scaffold.  Phase 9b will push SetButton;
             * Phase 9c will push SetCounter.  Consume the event so the
             * scene-manager doesn't fall through to a parent handler. */
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

    /* Counter requires manufacturer-key decryption (Phase 9c).  Placeholder
     * text keeps the layout consistent so the 9c diff is minimal. */
    u8g2_DrawStr(&m1_u8g2, 2, 62, "Counter: (9c)");
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
