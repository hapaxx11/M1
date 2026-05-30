/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene_fw.c
 * @brief  Settings Firmware Update sub-menu + fw delegates.
 *
 * Scenes covered:
 *   SettingsSceneFwUpdateMenu     — Firmware Update sub-menu (4 items)
 *   SettingsSceneFwUpdateImage    — Image File delegate
 *   SettingsSceneFwUpdateStart    — Firmware Update Start delegate
 *   SettingsSceneFwUpdateSwap     — Swap Banks delegate
 *   SettingsSceneFwUpdateDownload — Download delegate
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_settings_scene.h"
#include "m1_scene.h"
#include "m1_fw_update.h"
#include "m1_fw_download.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Firmware update delegates                                                */
/*==========================================================================*/

static void fw_image_on_enter(M1SceneApp *app)
{
    (void)app;
    firmware_update_get_image_file();
    app->running = true;
    m1_scene_pop(app);
}

static void fw_start_on_enter(M1SceneApp *app)
{
    (void)app;
    firmware_update_start();
    app->running = true;
    m1_scene_pop(app);
}

static void fw_swap_on_enter(M1SceneApp *app)
{
    (void)app;
    firmware_swap_banks();
    app->running = true;
    m1_scene_pop(app);
}

static void fw_download_on_enter(M1SceneApp *app)
{
    (void)app;
    fw_download_start();
    app->running = true;
    m1_scene_pop(app);
}

const M1SceneHandlers settings_scene_fw_image_handlers    = { .on_enter = fw_image_on_enter    };
const M1SceneHandlers settings_scene_fw_start_handlers    = { .on_enter = fw_start_on_enter    };
const M1SceneHandlers settings_scene_fw_swap_handlers     = { .on_enter = fw_swap_on_enter     };
const M1SceneHandlers settings_scene_fw_download_handlers = { .on_enter = fw_download_on_enter };

/*==========================================================================*/
/* Firmware Update sub-menu                                                 */
/*==========================================================================*/

#define FW_ITEM_COUNT  4

static const char *const fw_labels[FW_ITEM_COUNT] = {
    "Image File",
    "Firmware update",
    "Swap Banks",
    "Download",
};

static const uint8_t fw_targets[FW_ITEM_COUNT] = {
    SettingsSceneFwUpdateImage,
    SettingsSceneFwUpdateStart,
    SettingsSceneFwUpdateSwap,
    SettingsSceneFwUpdateDownload,
};

static uint8_t fw_sel = 0, fw_scroll = 0;

static void fw_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    firmware_update_init();
    app->need_redraw = true;
}

static bool fw_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &fw_sel, &fw_scroll,
                               FW_ITEM_COUNT, M1_MENU_VIS(FW_ITEM_COUNT),
                               fw_targets);
}

static void fw_menu_on_exit(M1SceneApp *app)
{
    (void)app;
    firmware_update_exit();
}

static void fw_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Firmware Update", fw_labels, FW_ITEM_COUNT,
                       fw_sel, fw_scroll, M1_MENU_VIS(FW_ITEM_COUNT));
}

const M1SceneHandlers settings_scene_fw_menu_handlers = {
    .on_enter = fw_menu_on_enter,
    .on_event = fw_menu_on_event,
    .on_exit  = fw_menu_on_exit,
    .draw     = fw_menu_draw,
};
