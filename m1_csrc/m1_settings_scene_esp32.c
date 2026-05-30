/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene_esp32.c
 * @brief  Settings ESP32 Update sub-menu + ESP32 delegates.
 *
 * Scenes covered:
 *   SettingsSceneEsp32Menu      — ESP32 Update sub-menu (6 items)
 *   SettingsSceneEsp32Image     — Image File delegate
 *   SettingsSceneEsp32Address   — Start Address delegate
 *   SettingsSceneEsp32Update    — Firmware Update delegate
 *   SettingsSceneEsp32Download  — Download delegate
 *   SettingsSceneEsp32Backup    — Backup Flash delegate
 *   SettingsSceneEsp32CheckInfo — Check Info delegate
 *
 * Phase E: uses `subghz_submenu_model_t` + `m1_submenu_draw/event` for
 * consistent font-aware layout and automatic visible-count sync.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_settings_scene.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_esp32_fw_update.h"
#include "m1_esp32_fw_download.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* ESP32 delegates                                                          */
/*==========================================================================*/

static void esp32_image_on_enter(M1SceneApp *app)
{
    (void)app;
    setting_esp32_image_file();
    app->running = true;
    m1_scene_pop(app);
}

static void esp32_addr_on_enter(M1SceneApp *app)
{
    (void)app;
    setting_esp32_start_address();
    app->running = true;
    m1_scene_pop(app);
}

static void esp32_update_on_enter(M1SceneApp *app)
{
    (void)app;
    setting_esp32_firmware_update();
    app->running = true;
    m1_scene_pop(app);
}

static void esp32_backup_on_enter(M1SceneApp *app)
{
    (void)app;
    setting_esp32_backup_flash();
    app->running = true;
    m1_scene_pop(app);
}

static void esp32_check_info_on_enter(M1SceneApp *app)
{
    (void)app;
    setting_esp32_check_info();
    app->running = true;
    m1_scene_pop(app);
}

static void esp32_download_on_enter(M1SceneApp *app)
{
    (void)app;
    esp32_fw_download_start();
    app->running = true;
    m1_scene_pop(app);
}

const M1SceneHandlers settings_scene_esp32_image_handlers     = { .on_enter = esp32_image_on_enter      };
const M1SceneHandlers settings_scene_esp32_addr_handlers      = { .on_enter = esp32_addr_on_enter       };
const M1SceneHandlers settings_scene_esp32_update_handlers    = { .on_enter = esp32_update_on_enter     };
const M1SceneHandlers settings_scene_esp32_backup_handlers    = { .on_enter = esp32_backup_on_enter     };
const M1SceneHandlers settings_scene_esp32_check_info_handlers= { .on_enter = esp32_check_info_on_enter };
const M1SceneHandlers settings_scene_esp32_download_handlers  = { .on_enter = esp32_download_on_enter   };

/*==========================================================================*/
/* ESP32 Update sub-menu                                                    */
/*==========================================================================*/

#define ESP32_ITEM_COUNT  6

static const char *const esp32_labels[ESP32_ITEM_COUNT] = {
    "Image File",
    "Start Address",
    "Firmware Update",
    "Download",
    "Backup Flash",
    "Check Info",
};

static const uint8_t esp32_targets[ESP32_ITEM_COUNT] = {
    SettingsSceneEsp32Image,
    SettingsSceneEsp32Address,
    SettingsSceneEsp32Update,
    SettingsSceneEsp32Download,
    SettingsSceneEsp32Backup,
    SettingsSceneEsp32CheckInfo,
};

static subghz_submenu_model_t s_esp32_model;

static void esp32_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    setting_esp32_init();
    subghz_submenu_model_init(&s_esp32_model, ESP32_ITEM_COUNT,
                              M1_MENU_VIS(ESP32_ITEM_COUNT));
    app->need_redraw = true;
}

static bool esp32_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_submenu_event(app, event, &s_esp32_model, esp32_targets);
}

static void esp32_menu_on_exit(M1SceneApp *app)
{
    (void)app;
    setting_esp32_exit();
}

static void esp32_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_esp32_model, "ESP32 Update", esp32_labels);
}

const M1SceneHandlers settings_scene_esp32_menu_handlers = {
    .on_enter = esp32_menu_on_enter,
    .on_event = esp32_menu_on_event,
    .on_exit  = esp32_menu_on_exit,
    .draw     = esp32_menu_draw,
};
