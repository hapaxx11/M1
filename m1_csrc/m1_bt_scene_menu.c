/* See COPYING.txt for license details. */

/**
 * @file   m1_bt_scene_menu.c
 * @brief  Bluetooth top-level menu scene + core delegates
 *         (Scan, Advertise, Config).
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_bt_scene.h"
#include "m1_scene.h"
#include "m1_bt.h"
#include "m1_esp32_hal.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#include "m1_compile_cfg.h"

/*==========================================================================*/
/* Blocking delegate macro                                                  */
/*==========================================================================*/

#define DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Core delegates                                                           */
/*==========================================================================*/

DELEGATE(scan,      bluetooth_scan)
DELEGATE(advertise, bluetooth_advertise)
DELEGATE(config,    bluetooth_config)

const M1SceneHandlers bt_scene_scan_handlers      = { .on_enter = scan_on_enter      };
const M1SceneHandlers bt_scene_advertise_handlers = { .on_enter = advertise_on_enter };
const M1SceneHandlers bt_scene_config_handlers    = { .on_enter = config_on_enter    };

/*==========================================================================*/
/* Top-level menu                                                           */
/*==========================================================================*/

#define MENU_ITEM_COUNT  14

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "BLE Scan",
    "BLE Advertise",
    "BLE Config",
    "Sniffers",
    "BLE Spam",
    "Wardrive",
    "Wardrive Cont.",
    "Wardrive Flock",
    "Detectors",
    "GATT Discover",
    "Bad-BT",
    "BT Name",
    "Saved Devices",
    "BT Info",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    BtSceneScan,
    BtSceneAdvertise,
    BtSceneConfig,
    BtSceneSnifferMenu,
    BtSceneSpamMenu,
    BtSceneWardrive,
    BtSceneWardriveContinu,
    BtSceneWardriveFlock,
    BtSceneDetectMenu,
    BtSceneGattDiscovery,
    BtSceneBadBt,
    BtSceneBtName,
    BtSceneSaved,
    BtSceneInfo,
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
    m1_scene_draw_menu("Bluetooth", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, M1_MENU_VIS(MENU_ITEM_COUNT));
}

const M1SceneHandlers bt_scene_menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};
