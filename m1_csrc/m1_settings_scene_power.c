/* See COPYING.txt for license details. */

/**
 * @file   m1_settings_scene_power.c
 * @brief  Settings Power sub-menu + power delegates.
 *
 * Scenes covered:
 *   SettingsScenePowerMenu   — Power sub-menu (3 items)
 *   SettingsScenePowerInfo   — Battery Info delegate
 *   SettingsScenePowerReboot — Reboot delegate
 *   SettingsScenePowerOff    — Power Off delegate
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
#include "m1_power_ctl.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Power delegates                                                          */
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

const M1SceneHandlers settings_scene_power_info_handlers   = { .on_enter = power_info_on_enter   };
const M1SceneHandlers settings_scene_power_reboot_handlers = { .on_enter = power_reboot_on_enter };
const M1SceneHandlers settings_scene_power_off_handlers    = { .on_enter = power_off_on_enter    };

/*==========================================================================*/
/* Power sub-menu                                                           */
/*==========================================================================*/

#define POWER_ITEM_COUNT  3

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

static subghz_submenu_model_t s_power_model;

static void power_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    menu_setting_power_init();
    subghz_submenu_model_init(&s_power_model, POWER_ITEM_COUNT,
                              M1_MENU_VIS(POWER_ITEM_COUNT));
    app->need_redraw = true;
}

static bool power_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_submenu_event(app, event, &s_power_model, power_targets);
}

static void power_menu_on_exit(M1SceneApp *app) { (void)app; }

static void power_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_power_model, "Power", power_labels);
}

const M1SceneHandlers settings_scene_power_menu_handlers = {
    .on_enter = power_menu_on_enter,
    .on_event = power_menu_on_event,
    .on_exit  = power_menu_on_exit,
    .draw     = power_menu_draw,
};
