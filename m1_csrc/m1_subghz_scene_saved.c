/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_saved.c
 * @brief  Sub-GHz Saved Scene — browse 0:/SUBGHZ/ files with action menu.
 *
 * Uses the existing storage_browse() file browser.  After selection,
 * shows an action menu: Emulate, Info, Rename, Delete.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_storage.h"
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"
#include "flipper_subghz.h"
#include "m1_sub_ghz.h"
#include "m1_virtual_kb.h"

extern uint8_t sub_ghz_replay_flipper_file(const char *sub_path);

/*============================================================================*/
/* Action menu                                                                */
/*============================================================================*/

#define ACTION_COUNT   4
#define ACTION_EMULATE 0
#define ACTION_INFO    1
#define ACTION_RENAME  2
#define ACTION_DELETE  3

static const char *action_labels[ACTION_COUNT] = {
    "Emulate", "Info", "Rename", "Delete"
};

static uint8_t action_sel = 0;
static char saved_filepath[64];
static char saved_filename[32];
static bool in_action_menu = false;
static bool in_info_screen = false;

/* Cached .sub file metadata (loaded on Info) */
static flipper_subghz_signal_t saved_signal;

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    (void)app;
    in_action_menu = false;
    in_info_screen = false;
    action_sel = 0;
    app->need_redraw = true;
}

static bool handle_action(SubGhzApp *app, uint8_t action)
{
    switch (action)
    {
        case ACTION_EMULATE:
        {
            uint8_t ret = sub_ghz_replay_flipper_file(saved_filepath);
            /* Restore radio to known state after replay (the replay
             * function calls menu_sub_ghz_exit which powers off the
             * SI4463). */
            menu_sub_ghz_init();
            if (ret != 0)
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
                    case 6: err = "Dynamic/RAW-only protocol";  break;
                    case 7: err = "Unsupported protocol";       break;
                }
                m1_message_box(&m1_u8g2, "Emulate failed", err, "",
                               "BACK to return");
            }
            app->need_redraw = true;
            return true;
        }
        case ACTION_INFO:
        {
            /* Load .sub file metadata and switch to info display */
            char full_path[72];
            snprintf(full_path, sizeof(full_path), "0:%s", saved_filepath);
            memset(&saved_signal, 0, sizeof(saved_signal));
            flipper_subghz_load(full_path, &saved_signal);
            in_info_screen = true;
            app->need_redraw = true;
            return true;
        }
        case ACTION_RENAME:
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
            app->need_redraw = true;
            return true;
        }
        case ACTION_DELETE:
        {
            /* Confirm deletion */
            uint8_t confirm = m1_message_box_choice(&m1_u8g2,
                "Delete file?", saved_filename, "", "OK  /  Cancel");
            if (confirm == 1)
            {
                char del_path[72];
                snprintf(del_path, sizeof(del_path), "0:%s", saved_filepath);
                f_unlink(del_path);
            }
            app->need_redraw = true;
            return true;
        }
    }
    return false;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    if (in_info_screen)
    {
        /* Info screen — BACK returns to action menu */
        if (event == SubGhzEventBack)
        {
            in_info_screen = false;
            app->need_redraw = true;
            return true;
        }
        return false;
    }

    if (!in_action_menu)
    {
        /* File browser mode */
        switch (event)
        {
            case SubGhzEventBack:
                subghz_scene_pop(app);
                return true;

            case SubGhzEventOk:
            {
                /* Open file browser (blocking call) */
                S_M1_file_info *f_info = storage_browse("0:/SUBGHZ");
                if (f_info && f_info->file_is_selected)
                {
                    snprintf(saved_filepath, sizeof(saved_filepath), "/SUBGHZ/%s",
                             f_info->file_name);
                    strncpy(saved_filename, f_info->file_name, sizeof(saved_filename) - 1);
                    saved_filename[sizeof(saved_filename) - 1] = '\0';
                    in_action_menu = true;
                    action_sel = 0;
                }
                app->need_redraw = true;
                return true;
            }

            default:
                break;
        }
    }
    else
    {
        /* Action menu mode */
        switch (event)
        {
            case SubGhzEventBack:
                in_action_menu = false;
                app->need_redraw = true;
                return true;

            case SubGhzEventUp:
                action_sel = (action_sel > 0) ? action_sel - 1 : ACTION_COUNT - 1;
                app->need_redraw = true;
                return true;

            case SubGhzEventDown:
                action_sel = (action_sel + 1) % ACTION_COUNT;
                app->need_redraw = true;
                return true;

            case SubGhzEventOk:
                handle_action(app, action_sel);
                return true;

            default:
                break;
        }
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    (void)app;
}

static void draw(SubGhzApp *app)
{
    (void)app;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    if (in_info_screen)
    {
        /* Saved signal info display */
        char line[48];

        u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
        u8g2_DrawStr(&m1_u8g2, 2, 10, "Signal Info");
        u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

        /* Protocol / type */
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

        /* Frequency */
        snprintf(line, sizeof(line), "Freq: %.2f MHz",
                 (float)saved_signal.frequency / 1000000.0f);
        u8g2_DrawStr(&m1_u8g2, 2, 46, line);

        /* Modulation preset */
        if (saved_signal.preset[0])
        {
            snprintf(line, sizeof(line), "Mod: %s", saved_signal.preset);
            u8g2_DrawStr(&m1_u8g2, 2, 54, line);
        }
    }
    else if (!in_action_menu)
    {
        /* Prompt to open file browser */
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        m1_draw_text(&m1_u8g2, 2, 15, 124, "Saved Signals", TEXT_ALIGN_CENTER);

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        m1_draw_text(&m1_u8g2, 2, 30, 124, "Press OK to browse", TEXT_ALIGN_CENTER);
        m1_draw_text(&m1_u8g2, 2, 40, 124, "0:/SUBGHZ/", TEXT_ALIGN_CENTER);

        subghz_button_bar_draw(
            NULL, NULL,
            NULL, NULL,
            NULL, "OK:Browse");
    }
    else
    {
        /* Action menu — no button bar, items fill available space */
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
        /* Truncated filename */
        char dname[22];
        strncpy(dname, saved_filename, 21);
        dname[21] = '\0';
        u8g2_DrawStr(&m1_u8g2, 2, 10, dname);

        u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

        /* 4 items × 12px height starting at y=14, last box ends at y=62
         * — all highlight boxes stay within the 64px screen. */
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        for (uint8_t i = 0; i < ACTION_COUNT; i++)
        {
            uint8_t y = 14 + i * 12;
            if (i == action_sel)
            {
                u8g2_DrawBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH, 12);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            }
            u8g2_DrawStr(&m1_u8g2, 8, y + 9, action_labels[i]);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
    }

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
