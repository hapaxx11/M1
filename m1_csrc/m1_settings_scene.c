/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene.c
 * @brief  Settings Scene Manager — scene registry and entry point.
 *
 * Scene implementations live in per-group files (Phase D split):
 *   m1_settings_scene_menu.c    — top-level menu + LCD/About/Dashboard delegates
 *   m1_settings_scene_storage.c — Storage sub-menu + 5 storage delegates
 *   m1_settings_scene_power.c   — Power sub-menu + 3 power delegates
 *   m1_settings_scene_fw.c      — Firmware Update sub-menu + 4 fw delegates
 *   m1_settings_scene_esp32.c   — ESP32 Update sub-menu + 6 ESP32 delegates
 */

#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_settings_scene.h"
#include "m1_scene.h"
#include "m1_settings.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Scene registry                                                           */
/*==========================================================================*/

static const M1SceneHandlers *const scene_registry[SettingsSceneCount] = {
    /* Top-level menu */
    [SettingsSceneMenu]             = &settings_scene_menu_handlers,
    [SettingsSceneLCD]              = &settings_scene_lcd_handlers,
    [SettingsSceneAbout]            = &settings_scene_about_handlers,
    [SettingsSceneDashboard]        = &settings_scene_dashboard_handlers,

    /* Storage sub-menu + delegates */
    [SettingsSceneStorageMenu]      = &settings_scene_storage_menu_handlers,
    [SettingsSceneStorageAbout]     = &settings_scene_storage_about_handlers,
    [SettingsSceneStorageExplore]   = &settings_scene_storage_explore_handlers,
    [SettingsSceneStorageMount]     = &settings_scene_storage_mount_handlers,
    [SettingsSceneStorageUnmount]   = &settings_scene_storage_unmount_handlers,
    [SettingsSceneStorageFormat]    = &settings_scene_storage_format_handlers,

    /* Power sub-menu + delegates */
    [SettingsScenePowerMenu]        = &settings_scene_power_menu_handlers,
    [SettingsScenePowerInfo]        = &settings_scene_power_info_handlers,
    [SettingsScenePowerReboot]      = &settings_scene_power_reboot_handlers,
    [SettingsScenePowerOff]         = &settings_scene_power_off_handlers,

    /* Firmware Update sub-menu + delegates */
    [SettingsSceneFwUpdateMenu]     = &settings_scene_fw_menu_handlers,
    [SettingsSceneFwUpdateImage]    = &settings_scene_fw_image_handlers,
    [SettingsSceneFwUpdateStart]    = &settings_scene_fw_start_handlers,
    [SettingsSceneFwUpdateSwap]     = &settings_scene_fw_swap_handlers,
    [SettingsSceneFwUpdateDownload] = &settings_scene_fw_download_handlers,

    /* ESP32 Update sub-menu + delegates */
    [SettingsSceneEsp32Menu]        = &settings_scene_esp32_menu_handlers,
    [SettingsSceneEsp32Image]       = &settings_scene_esp32_image_handlers,
    [SettingsSceneEsp32Address]     = &settings_scene_esp32_addr_handlers,
    [SettingsSceneEsp32Update]      = &settings_scene_esp32_update_handlers,
    [SettingsSceneEsp32Download]    = &settings_scene_esp32_download_handlers,
    [SettingsSceneEsp32Backup]      = &settings_scene_esp32_backup_handlers,
    [SettingsSceneEsp32CheckInfo]   = &settings_scene_esp32_check_info_handlers,
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
