/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_save_name.c
 * @brief  Sub-GHz Save Name Scene — virtual keyboard for filename entry.
 *
 * Auto-generates a default name from protocol + key, allows user to edit
 * or accept. Saves as Flipper-compatible .sub file.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_virtual_kb.h"
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"
#include "flipper_subghz.h"

extern const char *protocol_text[];
extern const char *subghz_freq_labels[];

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void on_enter(SubGhzApp *app)
{
    /* Build default filename from protocol + key */
    const SubGHz_History_Entry_t *e = subghz_history_get(&app->history, app->history_sel);
    if (e)
    {
        snprintf(app->file_name, sizeof(app->file_name), "%.12s_%lX",
                 protocol_text[e->info.protocol],
                 (unsigned long)(uint32_t)e->info.key);
    }
    else
    {
        snprintf(app->file_name, sizeof(app->file_name), "signal_%lu",
                 (unsigned long)HAL_GetTick());
    }

    /* Run the blocking VKB + save flow immediately on enter */
    if (!e)
    {
        subghz_scene_pop(app);
        return;
    }

    char new_name[32];
    if (!m1_vkb_get_filename("Save signal as:", app->file_name, new_name))
    {
        /* User cancelled */
        subghz_scene_pop(app);
        return;
    }

    /* Build full path and save */
    snprintf(app->file_path, sizeof(app->file_path), "/SUBGHZ/%s.sub", new_name);

    flipper_subghz_signal_t sub_sig;
    memset(&sub_sig, 0, sizeof(sub_sig));
    sub_sig.type      = FLIPPER_SUBGHZ_TYPE_PARSED;
    sub_sig.frequency = e->frequency;
    sub_sig.bit_count = e->info.bit_len;
    sub_sig.key       = e->info.key;
    sub_sig.te        = e->info.te;
    strncpy(sub_sig.preset, "FuriHalSubGhzPresetOok650Async",
            FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1);
    strncpy(sub_sig.protocol, protocol_text[e->info.protocol],
            FLIPPER_SUBGHZ_PROTO_MAX_LEN - 1);

    if (flipper_subghz_save(app->file_path, &sub_sig))
    {
        /* Replace this scene with success screen */
        subghz_scene_replace(app, SubGhzSceneSaveSuccess);
    }
    else
    {
        /* Save failed — pop back */
        subghz_scene_pop(app);
    }
}

static bool on_event(SubGhzApp *app, SubGhzEvent event)
{
    /* VKB is blocking so no events expected here */
    if (event == SubGhzEventBack)
    {
        subghz_scene_pop(app);
        return true;
    }
    return false;
}

static void on_exit(SubGhzApp *app)
{
    (void)app;
}

static void draw(SubGhzApp *app)
{
    /* VKB handles its own drawing in on_enter; this is a no-op placeholder */
    (void)app;
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_save_name_handlers = {
    .on_enter = on_enter,
    .on_event = on_event,
    .on_exit  = on_exit,
    .draw     = draw,
};
