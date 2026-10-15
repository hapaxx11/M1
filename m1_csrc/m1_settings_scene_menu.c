/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene_menu.c
 * @brief  Settings top-level menu scene + LCD and Dashboard delegates.
 *
 * Submenu model: uses `subghz_submenu_model_t` + `m1_submenu_draw/event` for
 * consistent font-aware layout and automatic visible-count sync.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_compile_cfg.h"
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_settings_scene.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_settings.h"
#include "m1_system_dashboard.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#ifdef M1_APP_RPC_ENABLE
#include "m1_rpc.h"
#endif

/*==========================================================================*/
/* Top-level delegates                                                      */
/*==========================================================================*/

static void lcd_on_enter(M1SceneApp *app)
{
    (void)app;
    settings_lcd_and_notifications();
    app->running = true;
    m1_scene_pop(app);
}

static void dashboard_on_enter(M1SceneApp *app)
{
    (void)app;
    system_dashboard_run();
    app->running = true;
    m1_scene_pop(app);
}

static void bluetooth_direct_on_enter(M1SceneApp *app)
{
    settings_set_bluetooth_direct_enabled(
        !settings_bluetooth_direct_enabled());
    settings_save_to_sd();
#ifdef M1_APP_RPC_ENABLE
    m1_rpc_bluetooth_direct_setting_changed();
#endif
    app->running = true;
    m1_scene_pop(app);
}

const M1SceneHandlers settings_scene_lcd_handlers       = { .on_enter = lcd_on_enter       };
const M1SceneHandlers settings_scene_dashboard_handlers = { .on_enter = dashboard_on_enter };
const M1SceneHandlers settings_scene_bluetooth_direct_handlers = {
    .on_enter = bluetooth_direct_on_enter
};

/*==========================================================================*/
/* Top-level Settings menu                                                  */
/*==========================================================================*/

#define MENU_ITEM_COUNT  7

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Dashboard",
    "LCD and Notifications",
    "Storage",
    "Power",
    "Firmware update",
    "ESP32 update",
    "Bluetooth Direct: Off",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    SettingsSceneDashboard,
    SettingsSceneLCD,
    SettingsSceneStorageMenu,
    SettingsScenePowerMenu,
    SettingsSceneFwUpdateMenu,
    SettingsSceneEsp32Menu,
    SettingsSceneBluetoothDirect,
};

static subghz_submenu_model_t s_menu_model;

static void menu_on_enter(M1SceneApp *app)
{
    (void)app;
    if (s_menu_model.item_count == 0)
        subghz_submenu_model_init(&s_menu_model, MENU_ITEM_COUNT,
                                  M1_MENU_VIS(MENU_ITEM_COUNT));
    app->need_redraw = true;
}

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_submenu_event(app, event, &s_menu_model, menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    const char *labels[MENU_ITEM_COUNT];
    memcpy(labels, menu_labels, sizeof(labels));
    labels[6] = settings_bluetooth_direct_enabled()
                    ? "Bluetooth Direct: On"
                    : "Bluetooth Direct: Off";
    m1_submenu_draw(&s_menu_model, "Settings", labels);
}

const M1SceneHandlers settings_scene_menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};
