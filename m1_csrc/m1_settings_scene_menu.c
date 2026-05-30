/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene_menu.c
 * @brief  Settings top-level menu scene + LCD, About, and Dashboard delegates.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_settings_scene.h"
#include "m1_scene.h"
#include "m1_settings.h"
#include "m1_system_dashboard.h"
#include "m1_lib.h"
#include "m1_tasks.h"

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

static void about_on_enter(M1SceneApp *app)
{
    (void)app;
    settings_about();
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

const M1SceneHandlers settings_scene_lcd_handlers       = { .on_enter = lcd_on_enter       };
const M1SceneHandlers settings_scene_about_handlers     = { .on_enter = about_on_enter     };
const M1SceneHandlers settings_scene_dashboard_handlers = { .on_enter = dashboard_on_enter };

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
    "About",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    SettingsSceneDashboard,
    SettingsSceneLCD,
    SettingsSceneStorageMenu,
    SettingsScenePowerMenu,
    SettingsSceneFwUpdateMenu,
    SettingsSceneEsp32Menu,
    SettingsSceneAbout,
};

static uint8_t menu_sel    = 0;
static uint8_t menu_scroll = 0;

static void menu_on_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &menu_sel, &menu_scroll,
                               MENU_ITEM_COUNT, M1_MENU_VIS(MENU_ITEM_COUNT),
                               menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Settings", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, M1_MENU_VIS(MENU_ITEM_COUNT));
}

const M1SceneHandlers settings_scene_menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};
