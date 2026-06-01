/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene.h
 * @brief  Settings Scene Manager — scene IDs and per-file handler exports.
 *
 * Scene implementation is split across five files following the
 * m1_subghz_scene_*.c convention (Phase D):
 *
 *   m1_settings_scene_menu.c    — top-level 7-item menu + LCD/About/Dashboard
 *   m1_settings_scene_storage.c — Storage sub-menu + 5 storage delegates
 *   m1_settings_scene_power.c   — Power sub-menu + 3 power delegates
 *   m1_settings_scene_fw.c      — Firmware Update sub-menu + 4 fw delegates
 *   m1_settings_scene_esp32.c   — ESP32 Update sub-menu + 6 ESP32 delegates
 *
 * m1_settings_scene.c owns only the scene_registry[] table and
 * settings_scene_entry().
 */

#ifndef M1_SETTINGS_SCENE_H_
#define M1_SETTINGS_SCENE_H_

#include "m1_scene.h"

/*==========================================================================*/
/* Scene IDs                                                                */
/*==========================================================================*/

typedef enum {
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
    SettingsSceneFwUpdateMenu,
    SettingsSceneFwUpdateImage,
    SettingsSceneFwUpdateStart,
    SettingsSceneFwUpdateSwap,
    SettingsSceneFwUpdateDownload,
    SettingsSceneEsp32Menu,
    SettingsSceneEsp32Image,
    SettingsSceneEsp32Address,
    SettingsSceneEsp32Update,
    SettingsSceneEsp32Download,
    SettingsSceneEsp32Backup,
    SettingsSceneEsp32CheckInfo,
    SettingsSceneAbout,
    SettingsSceneDashboard,
    SettingsSceneCount
} SettingsSceneId;

/*==========================================================================*/
/* Per-file handler exports                                                 */
/*==========================================================================*/

/* m1_settings_scene_menu.c */
extern const M1SceneHandlers settings_scene_menu_handlers;
extern const M1SceneHandlers settings_scene_lcd_handlers;
extern const M1SceneHandlers settings_scene_about_handlers;
extern const M1SceneHandlers settings_scene_dashboard_handlers;

/* m1_settings_scene_storage.c */
extern const M1SceneHandlers settings_scene_storage_menu_handlers;
extern const M1SceneHandlers settings_scene_storage_about_handlers;
extern const M1SceneHandlers settings_scene_storage_explore_handlers;
extern const M1SceneHandlers settings_scene_storage_mount_handlers;
extern const M1SceneHandlers settings_scene_storage_unmount_handlers;
extern const M1SceneHandlers settings_scene_storage_format_handlers;

/* m1_settings_scene_power.c */
extern const M1SceneHandlers settings_scene_power_menu_handlers;
extern const M1SceneHandlers settings_scene_power_info_handlers;
extern const M1SceneHandlers settings_scene_power_reboot_handlers;
extern const M1SceneHandlers settings_scene_power_off_handlers;

/* m1_settings_scene_fw.c */
extern const M1SceneHandlers settings_scene_fw_menu_handlers;
extern const M1SceneHandlers settings_scene_fw_image_handlers;
extern const M1SceneHandlers settings_scene_fw_start_handlers;
extern const M1SceneHandlers settings_scene_fw_swap_handlers;
extern const M1SceneHandlers settings_scene_fw_download_handlers;

/* m1_settings_scene_esp32.c */
extern const M1SceneHandlers settings_scene_esp32_menu_handlers;
extern const M1SceneHandlers settings_scene_esp32_image_handlers;
extern const M1SceneHandlers settings_scene_esp32_addr_handlers;
extern const M1SceneHandlers settings_scene_esp32_update_handlers;
extern const M1SceneHandlers settings_scene_esp32_download_handlers;
extern const M1SceneHandlers settings_scene_esp32_backup_handlers;
extern const M1SceneHandlers settings_scene_esp32_check_info_handlers;

/*==========================================================================*/
/* Entry point                                                              */
/*==========================================================================*/

void settings_scene_entry(void);

#endif /* M1_SETTINGS_SCENE_H_ */
