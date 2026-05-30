/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene_storage.c
 * @brief  Settings Storage sub-menu + storage delegates.
 *
 * Scenes covered:
 *   SettingsSceneStorageMenu    — Storage sub-menu (5 items)
 *   SettingsSceneStorageAbout   — About SD Card delegate
 *   SettingsSceneStorageExplore — Explore SD Card delegate
 *   SettingsSceneStorageMount   — Mount SD Card delegate
 *   SettingsSceneStorageUnmount — Unmount SD Card delegate
 *   SettingsSceneStorageFormat  — Format SD Card delegate
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_settings_scene.h"
#include "m1_scene.h"
#include "m1_storage.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Storage delegates                                                        */
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

const M1SceneHandlers settings_scene_storage_about_handlers   = { .on_enter = storage_about_on_enter   };
const M1SceneHandlers settings_scene_storage_explore_handlers = { .on_enter = storage_explore_on_enter };
const M1SceneHandlers settings_scene_storage_mount_handlers   = { .on_enter = storage_mount_on_enter   };
const M1SceneHandlers settings_scene_storage_unmount_handlers = { .on_enter = storage_unmount_on_enter };
const M1SceneHandlers settings_scene_storage_format_handlers  = { .on_enter = storage_format_on_enter  };

/*==========================================================================*/
/* Storage sub-menu                                                         */
/*==========================================================================*/

#define STORAGE_ITEM_COUNT  5

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

static uint8_t storage_sel = 0, storage_scroll = 0;

static void storage_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    menu_setting_storage_init();
    app->need_redraw = true;
}

static bool storage_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &storage_sel, &storage_scroll,
                               STORAGE_ITEM_COUNT, M1_MENU_VIS(STORAGE_ITEM_COUNT),
                               storage_targets);
}

static void storage_menu_on_exit(M1SceneApp *app) { (void)app; }

static void storage_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Storage", storage_labels, STORAGE_ITEM_COUNT,
                       storage_sel, storage_scroll, M1_MENU_VIS(STORAGE_ITEM_COUNT));
}

const M1SceneHandlers settings_scene_storage_menu_handlers = {
    .on_enter = storage_menu_on_enter,
    .on_event = storage_menu_on_event,
    .on_exit  = storage_menu_on_exit,
    .draw     = storage_menu_draw,
};
