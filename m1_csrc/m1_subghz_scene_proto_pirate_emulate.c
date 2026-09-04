/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_proto_pirate_emulate.c
 * @brief  Proto Pirate — Emulate scene.
 *
 * Pick a ProtoPirate automotive keyfob protocol from a scrollable list, then
 * enter a 64-bit hex key and transmit using the existing Transmitter scene.
 *
 * The scene is deliberately minimal:
 *   - UP/DOWN cycles the protocol list
 *   - OK on a protocol pushes the hex-key editor scene
 *     (SubGhzSceneSetKey reused via a temporary protocol spec)
 *   - OK in the editor writes a temp .sub and pushes SubGhzSceneTransmitter
 *   - BACK pops to the Proto Pirate menu
 *
 * Because the M1 create-from-scratch catalog only knows protocols with a
 * SubGhzCreateProtoSpec entry, we construct a temporary spec on the stack
 * from the ProtoPirate catalog and feed it directly to subghz_hex_editor.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "m1_submenu.h"
#include "subghz_submenu_model.h"
#include "subghz_proto_pirate.h"
#include "subghz_hex_editor.h"
#include "flipper_subghz.h"

/*============================================================================*/
/* Scene state                                                                */
/*============================================================================*/

static subghz_submenu_model_t s_model;

/** Temp file used to hand the user-built key to the Transmitter scene. */
#define PP_EMULATE_TMP_PATH   "0:/SUBGHZ/_pp_emulate_tmp.sub"

/** Hex editor used for the key-entry overlay. */
static subghz_hex_editor_t s_editor;

/** True when the hex editor is active (protocol selected, editing key). */
static bool s_edit_active;

/** Index of the currently-selected protocol. */
static uint8_t s_proto_idx;

/** Protocol label for the title line. */
static char s_title[32];

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

static const SubGhzProtoPirateDef *get_proto_def(uint8_t idx)
{
    if (idx >= subghz_proto_pirate_catalog_count)
        return NULL;
    return &subghz_proto_pirate_catalog[idx];
}

static void build_labels(const char **labels)
{
    for (uint8_t i = 0; i < subghz_proto_pirate_catalog_count; i++)
        labels[i] = subghz_proto_pirate_catalog[i].name;
}

static uint32_t proto_freq_hz(const SubGhzProtoPirateDef *def)
{
    (void)def;
    /* All Tier-A ProtoPirate automotive protocols operate at 433.92 MHz. */
    return 433920000UL;
}

static const char *proto_preset(const SubGhzProtoPirateDef *def)
{
    (void)def;
    return "FuriHalSubGhzPresetOok650Async";
}

/** Write the temp .sub file and push the Transmitter scene. */
static bool push_transmitter_with_key(SubGhzApp *app,
                                      const SubGhzProtoPirateDef *def)
{
    uint64_t raw = subghz_hex_editor_value(&s_editor);
    uint8_t bits = def->bit_count;
    if (bits == 0 || bits > 64)
        bits = 64;

    uint64_t mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
    uint64_t key = raw & mask;

    (void)f_unlink(PP_EMULATE_TMP_PATH);

    if (!flipper_subghz_save_key(PP_EMULATE_TMP_PATH,
                                 proto_freq_hz(def),
                                 proto_preset(def),
                                 def->name,
                                 bits,
                                 key,
                                 def->te_short))
    {
        app->need_redraw = true;
        return false;
    }

    strncpy(app->tx_path, PP_EMULATE_TMP_PATH, sizeof(app->tx_path) - 1);
    app->tx_path[sizeof(app->tx_path) - 1] = '\0';
    app->tx_repeat_count = 1U;
    app->tx_mode = 0U;  /* SUBGHZ_TX_MODE_SINGLE */
    app->tx_autostart = true;
    strncpy(app->tx_protocol_name, def->name,
            sizeof(app->tx_protocol_name) - 1);
    app->tx_protocol_name[sizeof(app->tx_protocol_name) - 1] = '\0';

    app->resume_from_child = true;
    subghz_scene_push(app, SubGhzSceneTransmitter);
    return true;
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    (void)app;
    s_edit_active = false;

    subghz_submenu_model_init(&s_model,
                              subghz_proto_pirate_catalog_count,
                              M1_MENU_VIS(subghz_proto_pirate_catalog_count));

    uint32_t saved = subghz_scene_get_state(app, SubGhzSceneProtoPirateEmulate);
    s_proto_idx = (uint8_t)(saved & 0xFFu);
    if (s_proto_idx >= subghz_proto_pirate_catalog_count)
        s_proto_idx = 0;
    subghz_submenu_model_set_selected(&s_model, s_proto_idx);

    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    if (!s_edit_active)
    {
        /* Protocol picker mode. */
        subghz_submenu_model_set_visible_count(
            &s_model, M1_MENU_VIS(subghz_proto_pirate_catalog_count));

        switch (event)
        {
            case SubGhzEventBack:
                subghz_scene_pop(app);
                return true;

            case SubGhzEventUp:
                subghz_submenu_model_up(&s_model);
                s_proto_idx = s_model.selected;
                subghz_scene_set_state(app, SubGhzSceneProtoPirateEmulate,
                                       s_proto_idx);
                app->need_redraw = true;
                return true;

            case SubGhzEventDown:
                subghz_submenu_model_down(&s_model);
                s_proto_idx = s_model.selected;
                subghz_scene_set_state(app, SubGhzSceneProtoPirateEmulate,
                                       s_proto_idx);
                app->need_redraw = true;
                return true;

            case SubGhzEventOk:
            {
                const SubGhzProtoPirateDef *def = get_proto_def(s_proto_idx);
                if (def)
                {
                    uint8_t bits = def->bit_count;
                    if (bits == 0 || bits > 64)
                        bits = 64;
                    subghz_hex_editor_init(&s_editor, bits);
                    s_edit_active = true;
                    strncpy(s_title, def->name, sizeof(s_title) - 1);
                    s_title[sizeof(s_title) - 1] = '\0';
                    app->need_redraw = true;
                }
                return true;
            }

            default:
                break;
        }
        return false;
    }

    /* Hex-key editor mode. */
    switch (event)
    {
        case SubGhzEventBack:
            s_edit_active = false;
            (void)f_unlink(PP_EMULATE_TMP_PATH);
            app->need_redraw = true;
            return true;

        case SubGhzEventUp:
            subghz_hex_editor_up(&s_editor);
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            subghz_hex_editor_down(&s_editor);
            app->need_redraw = true;
            return true;

        case SubGhzEventLeft:
            subghz_hex_editor_left(&s_editor);
            app->need_redraw = true;
            return true;

        case SubGhzEventRight:
            subghz_hex_editor_right(&s_editor);
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
        {
            const SubGhzProtoPirateDef *def = get_proto_def(s_proto_idx);
            if (def && push_transmitter_with_key(app, def))
                return true;
            app->need_redraw = true;
            return true;
        }

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    if (app && app->resume_from_child)
        return;  /* handing off to Transmitter; do not unlink temp file */
    (void)f_unlink(PP_EMULATE_TMP_PATH);
}

/*============================================================================*/
/* Draw                                                                       */
/*============================================================================*/

static void draw_editor(SubGhzApp *app)
{
    (void)app;

    char hex_str[2 + SUBGHZ_HEX_EDITOR_MAX_DIGITS + 1];
    int p = 0;
    hex_str[p++] = '0';
    hex_str[p++] = 'x';
    for (uint8_t d = 0U; d < s_editor.digit_count; ++d)
        hex_str[p++] = "0123456789ABCDEF"[s_editor.digits[d] & 0x0FU];
    hex_str[p] = '\0';

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, s_title);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 20, "433.92 MHz OOK");

    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 4, 38, hex_str);

    uint8_t cx = (uint8_t)(4U + ((uint16_t)s_editor.cursor + 2U) * 8U);
    u8g2_DrawHLine(&m1_u8g2, cx, 40, 7);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 0, 56, "\x18\x19:Hex L/R:Move OK:Send");

    m1_u8g2_nextpage();
}

static void draw(SubGhzApp *app)
{
    if (s_edit_active)
    {
        draw_editor(app);
        return;
    }

    const char *labels[SubGhzProtoPirate_Count];
    build_labels(labels);

    subghz_submenu_model_set_visible_count(
        &s_model, M1_MENU_VIS(subghz_proto_pirate_catalog_count));
    m1_submenu_draw(&s_model, "PP Emulate", labels);
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_proto_pirate_emulate_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
