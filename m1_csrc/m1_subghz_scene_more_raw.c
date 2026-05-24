/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_more_raw.c
 * @brief  Sub-GHz Read-Raw "More" submenu scene (Phase 6).
 *
 * Pushed by the Read Raw scene from the RIGHT button while in the Loaded
 * state.  Provides the Momentum LoadKeyIDLE → MoreRAW submenu actions for
 * a pre-loaded RAW file:
 *
 *   - Decode → push @ref SubGhzSceneDecodeRaw to run offline protocol
 *              decoding against the currently-loaded file.
 *   - Rename → blocking virtual-keyboard rename; updates
 *              @ref SubGhzApp.raw_filepath in place.
 *   - Delete → blocking confirmation dialog; on confirm unlinks the file,
 *              clears @ref SubGhzApp.raw_filepath, and pops back to the
 *              Read Raw scene where the resume-from-child path detects the
 *              empty filepath and resets to the Start state.
 *
 * The Read Raw scene owns the file path buffer (@ref SubGhzApp.raw_filepath);
 * this scene reads it on entry and may mutate it on Rename / Delete.  All
 * three actions are blocking (VKB / message-box / decode-load), so the
 * scene's event handler runs at most once per user interaction.
 *
 * Phase 6 split: this submenu was previously inlined inside
 * `m1_subghz_scene_read_raw.c` behind `rr_in_more_menu` / `rr_more_sel`
 * file-scope statics.  Extracting it removes ~120 lines of overlay state
 * from Read Raw and aligns the architecture with Momentum's dedicated
 * MoreRAW scene.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "ff.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "m1_virtual_kb.h"

/*============================================================================*/
/* Scene state                                                                */
/*============================================================================*/

#define MORE_RAW_ITEM_COUNT 3

enum {
    MORE_RAW_DECODE = 0,
    MORE_RAW_RENAME,
    MORE_RAW_DELETE,
};

static const char *const more_raw_labels[MORE_RAW_ITEM_COUNT] = {
    "Decode", "Rename", "Delete",
};

static uint8_t more_raw_sel;

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/** Extract bare filename (with extension) from a full path.  Handles both
 *  '/' and '\\' separators. */
static const char *extract_filename(const char *fullpath)
{
    const char *fwd = strrchr(fullpath, '/');
    const char *bck = strrchr(fullpath, '\\');
    const char *p = (fwd && bck) ? (fwd > bck ? fwd : bck)
                  : (fwd ? fwd : bck);
    return p ? p + 1 : fullpath;
}

/*============================================================================*/
/* Action handlers                                                            */
/*============================================================================*/

static void do_rename(SubGhzApp *app)
{
    if (app->raw_filepath[0] == '\0')
        return;

    const char *fname = extract_filename(app->raw_filepath);

    /* Strip extension for the VKB prompt */
    char base[32];
    strncpy(base, fname, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    char new_name[32];
    if (!m1_vkb_get_filename("Rename to:", base, new_name))
        return;

    const char *ext = strrchr(fname, '.');
    if (!ext) ext = ".sub";

    char old_path[80];
    char new_path[80];
    snprintf(old_path, sizeof(old_path), "0:%s", app->raw_filepath);
    snprintf(new_path, sizeof(new_path), "0:/SUBGHZ/%s%s", new_name, ext);

    FRESULT res = f_rename(old_path, new_path);
    if (res == FR_OK)
    {
        snprintf(app->raw_filepath, sizeof(app->raw_filepath),
                 "/SUBGHZ/%s%s", new_name, ext);
    }
    else
    {
        m1_message_box(&m1_u8g2, "Rename failed",
                       "Could not rename file", "", "BACK");
    }
}

static void do_delete(SubGhzApp *app)
{
    if (app->raw_filepath[0] == '\0')
        return;

    char short_name[32];
    strncpy(short_name, extract_filename(app->raw_filepath),
            sizeof(short_name) - 1);
    short_name[sizeof(short_name) - 1] = '\0';

    uint8_t confirm = m1_message_box_choice(
        &m1_u8g2, "Delete file?", short_name, "", "OK  /  Cancel");
    if (confirm != 1)
        return;

    char del_path[80];
    snprintf(del_path, sizeof(del_path), "0:%s", app->raw_filepath);
    FRESULT res = f_unlink(del_path);
    if ((res == FR_OK) || (res == FR_NO_FILE))
    {
        /* Clear the active filepath so the Read Raw scene's
         * resume-from-child path resets to the Start state. */
        app->raw_filepath[0] = '\0';
        subghz_scene_pop(app);
    }
    else
    {
        m1_message_box(&m1_u8g2, "Delete failed",
                       "Could not delete file", "", "BACK");
    }
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    /* If there is no file to act on, immediately pop back. */
    if (app->raw_filepath[0] == '\0')
    {
        subghz_scene_pop(app);
        return;
    }
    more_raw_sel = 0;
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            subghz_scene_pop(app);
            return true;

        case SubGhzEventUp:
            more_raw_sel = (more_raw_sel > 0)
                           ? (uint8_t)(more_raw_sel - 1)
                           : (uint8_t)(MORE_RAW_ITEM_COUNT - 1);
            app->need_redraw = true;
            return true;

        case SubGhzEventDown:
            more_raw_sel = (uint8_t)((more_raw_sel + 1) % MORE_RAW_ITEM_COUNT);
            app->need_redraw = true;
            return true;

        case SubGhzEventOk:
            switch (more_raw_sel)
            {
                case MORE_RAW_DECODE:
                    /* Decode pushes the DecodeRaw scene which loads the file
                     * and renders results.  On pop-back we stay on this menu. */
                    subghz_scene_push(app, SubGhzSceneDecodeRaw);
                    break;
                case MORE_RAW_RENAME:
                    do_rename(app);
                    app->need_redraw = true;
                    break;
                case MORE_RAW_DELETE:
                    do_delete(app);
                    /* do_delete() pops this scene on success; on failure /
                     * cancel we stay here.  Redraw is harmless in both cases. */
                    app->need_redraw = true;
                    break;
                default:
                    break;
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
}

/*============================================================================*/
/* Draw                                                                       */
/*============================================================================*/

static void draw(SubGhzApp *app)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title: truncated filename (or "RAW" fallback) */
    char title[22];
    const char *fname = (app->raw_filepath[0] != '\0')
                        ? extract_filename(app->raw_filepath) : "RAW";
    strncpy(title, fname, sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';

    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 10, title);
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    /* Items: same row-height computation pattern used by the SavedMenu scene. */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    const uint8_t row_h    = 50 / MORE_RAW_ITEM_COUNT;
    const uint8_t text_ofs = (row_h >= 12) ? 9 : 8;

    for (uint8_t i = 0; i < MORE_RAW_ITEM_COUNT; i++)
    {
        uint8_t y = 14 + i * row_h;
        if (i == more_raw_sel)
        {
            u8g2_DrawRBox(&m1_u8g2, 1, y, M1_MENU_TEXT_W, row_h, 2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }
        u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, more_raw_labels[i]);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_more_raw_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
