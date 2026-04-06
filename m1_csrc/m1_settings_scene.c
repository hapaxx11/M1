/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene.c
 * @brief  Settings Scene Manager — scene-based menu with nested sub-menus
 *         and blocking delegates.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#include "m1_settings.h"
#include "m1_storage.h"
#include "m1_power_ctl.h"
#include "m1_fw_update.h"
#include "m1_esp32_fw_update.h"
#include "m1_fw_download.h"

/*==========================================================================*/
/* Scene IDs                                                                */
/*==========================================================================*/

enum {
    SettingsSceneMenu = 0,
    SettingsSceneLCD,
    SettingsSceneStorageMenu,
    SettingsSceneStorageAbout,
    SettingsSceneStorageExplore,
    SettingsSceneStorageMount,
    SettingsSceneStorageUnmount,
    SettingsSceneStorageFormat,
    SettingsScenePowerMenu,
    SettingsScenePowerInfo,
    SettingsScenePowerReboot,
    SettingsScenePowerOff,
    SettingsSceneSystem,
    SettingsSceneFwUpdateMenu,
    SettingsSceneFwUpdateImage,
    SettingsSceneFwUpdateStart,
    SettingsSceneFwUpdateSwap,
    SettingsSceneFwUpdateDownload,
    SettingsSceneEsp32Menu,
    SettingsSceneEsp32Image,
    SettingsSceneEsp32Address,
    SettingsSceneEsp32Update,
    SettingsSceneAbout,
    SettingsSceneCount
};

/*==========================================================================*/
/* Blocking delegates — top-level                                           */
/*==========================================================================*/

static void lcd_on_enter(M1SceneApp *app)
{
    (void)app;
    settings_lcd_and_notifications();
    app->running = true;
    m1_scene_pop(app);
}

static void system_on_enter(M1SceneApp *app)
{
    (void)app;
    settings_system();
    app->running = true;
    m1_scene_pop(app);
}

static void about_on_enter(M1SceneApp *app)
{
    (void)app;
    settings_about();
    app->running = true;
    m1_scene_pop(app);
}

static const M1SceneHandlers lcd_handlers    = { .on_enter = lcd_on_enter    };
static const M1SceneHandlers system_handlers = { .on_enter = system_on_enter };
static const M1SceneHandlers about_handlers  = { .on_enter = about_on_enter  };

/*==========================================================================*/
/* Blocking delegates — Storage                                             */
/*==========================================================================*/

static void storage_about_on_enter(M1SceneApp *app)
{
    (void)app;
    storage_about();
    app->running = true;
    m1_scene_pop(app);
}

static void storage_explore_on_enter(M1SceneApp *app)
{
    (void)app;
    storage_explore();
    app->running = true;
    m1_scene_pop(app);
}

static void storage_mount_on_enter(M1SceneApp *app)
{
    (void)app;
    storage_mount();
    app->running = true;
    m1_scene_pop(app);
}

static void storage_unmount_on_enter(M1SceneApp *app)
{
    (void)app;
    storage_unmount();
    app->running = true;
    m1_scene_pop(app);
}

static void storage_format_on_enter(M1SceneApp *app)
{
    (void)app;
    storage_format();
    app->running = true;
    m1_scene_pop(app);
}

static const M1SceneHandlers storage_about_handlers   = { .on_enter = storage_about_on_enter   };
static const M1SceneHandlers storage_explore_handlers  = { .on_enter = storage_explore_on_enter  };
static const M1SceneHandlers storage_mount_handlers    = { .on_enter = storage_mount_on_enter    };
static const M1SceneHandlers storage_unmount_handlers  = { .on_enter = storage_unmount_on_enter  };
static const M1SceneHandlers storage_format_handlers   = { .on_enter = storage_format_on_enter   };

/*==========================================================================*/
/* Blocking delegates — Power                                               */
/*==========================================================================*/

static void power_info_on_enter(M1SceneApp *app)
{
    (void)app;
    power_battery_info();
    app->running = true;
    m1_scene_pop(app);
}

static void power_reboot_on_enter(M1SceneApp *app)
{
    (void)app;
    power_reboot();
    app->running = true;
    m1_scene_pop(app);
}

static void power_off_on_enter(M1SceneApp *app)
{
    (void)app;
    power_off();
    app->running = true;
    m1_scene_pop(app);
}

static const M1SceneHandlers power_info_handlers   = { .on_enter = power_info_on_enter   };
static const M1SceneHandlers power_reboot_handlers = { .on_enter = power_reboot_on_enter };
static const M1SceneHandlers power_off_handlers    = { .on_enter = power_off_on_enter    };

/*==========================================================================*/
/* Blocking delegates — Firmware Update                                     */
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

static const M1SceneHandlers fw_image_handlers    = { .on_enter = fw_image_on_enter    };
static const M1SceneHandlers fw_start_handlers    = { .on_enter = fw_start_on_enter    };
static const M1SceneHandlers fw_swap_handlers     = { .on_enter = fw_swap_on_enter     };
static const M1SceneHandlers fw_download_handlers = { .on_enter = fw_download_on_enter };

/*==========================================================================*/
/* Blocking delegates — ESP32 Update                                        */
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

static const M1SceneHandlers esp32_image_handlers  = { .on_enter = esp32_image_on_enter  };
static const M1SceneHandlers esp32_addr_handlers   = { .on_enter = esp32_addr_on_enter   };
static const M1SceneHandlers esp32_update_handlers = { .on_enter = esp32_update_on_enter };

/*==========================================================================*/
/* Top-level Settings menu scene                                            */
/*==========================================================================*/

#define MENU_ITEM_COUNT  7
#define MENU_VISIBLE     6

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "LCD and Notifications",
    "Storage",
    "Power",
    "System",
    "Firmware update",
    "ESP32 update",
    "About",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    SettingsSceneLCD,
    SettingsSceneStorageMenu,
    SettingsScenePowerMenu,
    SettingsSceneSystem,
    SettingsSceneFwUpdateMenu,
    SettingsSceneEsp32Menu,
    SettingsSceneAbout,
};

static uint8_t menu_sel    = 0;
static uint8_t menu_scroll = 0;

static void menu_on_enter(M1SceneApp *app)
{
    (void)app;
    app->need_redraw = true;
}

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &menu_sel, &menu_scroll,
                               MENU_ITEM_COUNT, MENU_VISIBLE, menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Settings", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, MENU_VISIBLE);
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/*==========================================================================*/
/* Storage sub-menu scene                                                   */
/*==========================================================================*/

#define STORAGE_ITEM_COUNT  5
#define STORAGE_VISIBLE     5

static const char *const storage_labels[STORAGE_ITEM_COUNT] = {
    "About SD Card",
    "Explore SD Card",
    "Mount SD Card",
    "Unmount SD Card",
    "Format SD Card",
};

static const uint8_t storage_targets[STORAGE_ITEM_COUNT] = {
    SettingsSceneStorageAbout,
    SettingsSceneStorageExplore,
    SettingsSceneStorageMount,
    SettingsSceneStorageUnmount,
    SettingsSceneStorageFormat,
};

static uint8_t storage_sel    = 0;
static uint8_t storage_scroll = 0;

static void storage_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    menu_setting_storage_init();
    app->need_redraw = true;
}

static bool storage_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &storage_sel, &storage_scroll,
                               STORAGE_ITEM_COUNT, STORAGE_VISIBLE,
                               storage_targets);
}

static void storage_menu_on_exit(M1SceneApp *app) { (void)app; }

static void storage_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Storage", storage_labels, STORAGE_ITEM_COUNT,
                       storage_sel, storage_scroll, STORAGE_VISIBLE);
}

static const M1SceneHandlers storage_menu_handlers = {
    .on_enter = storage_menu_on_enter,
    .on_event = storage_menu_on_event,
    .on_exit  = storage_menu_on_exit,
    .draw     = storage_menu_draw,
};

/*==========================================================================*/
/* Power sub-menu scene                                                     */
/*==========================================================================*/

#define POWER_ITEM_COUNT  3
#define POWER_VISIBLE     3

static const char *const power_labels[POWER_ITEM_COUNT] = {
    "Battery Info",
    "Reboot",
    "Power Off",
};

static const uint8_t power_targets[POWER_ITEM_COUNT] = {
    SettingsScenePowerInfo,
    SettingsScenePowerReboot,
    SettingsScenePowerOff,
};

static uint8_t power_sel    = 0;
static uint8_t power_scroll = 0;

static void power_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    menu_setting_power_init();
    app->need_redraw = true;
}

static bool power_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &power_sel, &power_scroll,
                               POWER_ITEM_COUNT, POWER_VISIBLE,
                               power_targets);
}

static void power_menu_on_exit(M1SceneApp *app) { (void)app; }

static void power_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Power", power_labels, POWER_ITEM_COUNT,
                       power_sel, power_scroll, POWER_VISIBLE);
}

static const M1SceneHandlers power_menu_handlers = {
    .on_enter = power_menu_on_enter,
    .on_event = power_menu_on_event,
    .on_exit  = power_menu_on_exit,
    .draw     = power_menu_draw,
};

/*==========================================================================*/
/* Firmware Update sub-menu scene                                           */
/*==========================================================================*/

#define FW_ITEM_COUNT  4
#define FW_VISIBLE     4

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

static uint8_t fw_sel    = 0;
static uint8_t fw_scroll = 0;

static void fw_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    firmware_update_init();
    app->need_redraw = true;
}

static bool fw_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &fw_sel, &fw_scroll,
                               FW_ITEM_COUNT, FW_VISIBLE, fw_targets);
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
                       fw_sel, fw_scroll, FW_VISIBLE);
}

static const M1SceneHandlers fw_menu_handlers = {
    .on_enter = fw_menu_on_enter,
    .on_event = fw_menu_on_event,
    .on_exit  = fw_menu_on_exit,
    .draw     = fw_menu_draw,
};

/*==========================================================================*/
/* ESP32 Update sub-menu scene                                              */
/*==========================================================================*/

#define ESP32_ITEM_COUNT  3
#define ESP32_VISIBLE     3

static const char *const esp32_labels[ESP32_ITEM_COUNT] = {
    "Image File",
    "Start Address",
    "Firmware Update",
};

static const uint8_t esp32_targets[ESP32_ITEM_COUNT] = {
    SettingsSceneEsp32Image,
    SettingsSceneEsp32Address,
    SettingsSceneEsp32Update,
};

static uint8_t esp32_sel    = 0;
static uint8_t esp32_scroll = 0;

static void esp32_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    setting_esp32_init();
    app->need_redraw = true;
}

static bool esp32_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &esp32_sel, &esp32_scroll,
                               ESP32_ITEM_COUNT, ESP32_VISIBLE,
                               esp32_targets);
}

static void esp32_menu_on_exit(M1SceneApp *app)
{
    (void)app;
    setting_esp32_exit();
}

static void esp32_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("ESP32 Update", esp32_labels, ESP32_ITEM_COUNT,
                       esp32_sel, esp32_scroll, ESP32_VISIBLE);
}

static const M1SceneHandlers esp32_menu_handlers = {
    .on_enter = esp32_menu_on_enter,
    .on_event = esp32_menu_on_event,
    .on_exit  = esp32_menu_on_exit,
    .draw     = esp32_menu_draw,
};

/*==========================================================================*/
/* Scene registry                                                           */
/*==========================================================================*/

static const M1SceneHandlers *const scene_registry[SettingsSceneCount] = {
    /* Top-level menu */
    [SettingsSceneMenu]             = &menu_handlers,
    [SettingsSceneLCD]              = &lcd_handlers,
    [SettingsSceneSystem]           = &system_handlers,
    [SettingsSceneAbout]            = &about_handlers,

    /* Storage sub-menu + delegates */
    [SettingsSceneStorageMenu]      = &storage_menu_handlers,
    [SettingsSceneStorageAbout]     = &storage_about_handlers,
    [SettingsSceneStorageExplore]   = &storage_explore_handlers,
    [SettingsSceneStorageMount]     = &storage_mount_handlers,
    [SettingsSceneStorageUnmount]   = &storage_unmount_handlers,
    [SettingsSceneStorageFormat]    = &storage_format_handlers,

    /* Power sub-menu + delegates */
    [SettingsScenePowerMenu]        = &power_menu_handlers,
    [SettingsScenePowerInfo]        = &power_info_handlers,
    [SettingsScenePowerReboot]      = &power_reboot_handlers,
    [SettingsScenePowerOff]         = &power_off_handlers,

    /* Firmware Update sub-menu + delegates */
    [SettingsSceneFwUpdateMenu]     = &fw_menu_handlers,
    [SettingsSceneFwUpdateImage]    = &fw_image_handlers,
    [SettingsSceneFwUpdateStart]    = &fw_start_handlers,
    [SettingsSceneFwUpdateSwap]     = &fw_swap_handlers,
    [SettingsSceneFwUpdateDownload] = &fw_download_handlers,

    /* ESP32 Update sub-menu + delegates */
    [SettingsSceneEsp32Menu]        = &esp32_menu_handlers,
    [SettingsSceneEsp32Image]       = &esp32_image_handlers,
    [SettingsSceneEsp32Address]     = &esp32_addr_handlers,
    [SettingsSceneEsp32Update]      = &esp32_update_handlers,
};

/*==========================================================================*/
/* Entry point                                                              */
/*==========================================================================*/

void settings_scene_entry(void)
{
    m1_scene_run(scene_registry, SettingsSceneCount,
                 menu_settings_init, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
