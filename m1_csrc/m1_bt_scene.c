/* See COPYING.txt for license details. */

/**
 * @file   m1_bt_scene.c
 * @brief  Bluetooth Scene Manager — scene-based menu with blocking delegates.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_bt.h"
#include "m1_lib.h"
#include "m1_tasks.h"

#ifdef M1_APP_BADBT_ENABLE
#include "m1_badbt.h"
#endif

#ifdef M1_APP_BLE_SPAM_ENABLE
#include "m1_ble_spam.h"
#endif

/*==========================================================================*/
/* Simple mode (no BT_MANAGE)                                               */
/*==========================================================================*/

#ifndef M1_APP_BT_MANAGE_ENABLE

/* Scene IDs */
enum {
    BtSceneMenu = 0,
    BtSceneConfig,
    BtSceneScan,
    BtSceneAdvertise,
    BtSceneCount
};

/*--- Blocking delegates ---------------------------------------------------*/

static void config_on_enter(M1SceneApp *app)
{
    (void)app;
    bluetooth_config();
    app->running = true;
    m1_scene_pop(app);
}

static void scan_on_enter(M1SceneApp *app)
{
    (void)app;
    bluetooth_scan();
    app->running = true;
    m1_scene_pop(app);
}

static void advertise_on_enter(M1SceneApp *app)
{
    (void)app;
    bluetooth_advertise();
    app->running = true;
    m1_scene_pop(app);
}

/*--- Handler tables -------------------------------------------------------*/

static const M1SceneHandlers config_handlers    = { .on_enter = config_on_enter    };
static const M1SceneHandlers scan_handlers      = { .on_enter = scan_on_enter      };
static const M1SceneHandlers advertise_handlers = { .on_enter = advertise_on_enter };

/*--- Menu scene -----------------------------------------------------------*/

#define MENU_ITEM_COUNT  3
#define MENU_VISIBLE     3

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Config",
    "Scan",
    "Advertise",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    BtSceneConfig,
    BtSceneScan,
    BtSceneAdvertise,
};

#else /* M1_APP_BT_MANAGE_ENABLE defined */

/*==========================================================================*/
/* Full mode (BT_MANAGE enabled)                                            */
/*==========================================================================*/

/* Scene IDs */
enum {
    BtSceneMenu = 0,
    BtSceneScan,
    BtSceneSaved,
    BtSceneAdvertise,
#ifdef M1_APP_BADBT_ENABLE
    BtSceneBadBt,
    BtSceneBtName,
#endif
#ifdef M1_APP_BLE_SPAM_ENABLE
    BtSceneBleSpam,
#endif
    BtSceneInfo,
    BtSceneCount
};

/*--- Blocking delegates ---------------------------------------------------*/

static void scan_on_enter(M1SceneApp *app)
{
    (void)app;
    bluetooth_scan();
    app->running = true;
    m1_scene_pop(app);
}

static void saved_on_enter(M1SceneApp *app)
{
    (void)app;
    bluetooth_saved_devices();
    app->running = true;
    m1_scene_pop(app);
}

static void advertise_on_enter(M1SceneApp *app)
{
    (void)app;
    bluetooth_advertise();
    app->running = true;
    m1_scene_pop(app);
}

#ifdef M1_APP_BADBT_ENABLE
static void badbt_on_enter(M1SceneApp *app)
{
    (void)app;
    badbt_run();
    app->running = true;
    m1_scene_pop(app);
}

static void btname_on_enter(M1SceneApp *app)
{
    (void)app;
    bluetooth_set_badbt_name();
    app->running = true;
    m1_scene_pop(app);
}
#endif

#ifdef M1_APP_BLE_SPAM_ENABLE
static void blespam_on_enter(M1SceneApp *app)
{
    (void)app;
    ble_spam_run();
    app->running = true;
    m1_scene_pop(app);
}
#endif

static void info_on_enter(M1SceneApp *app)
{
    (void)app;
    bluetooth_info();
    app->running = true;
    m1_scene_pop(app);
}

/*--- Handler tables -------------------------------------------------------*/

static const M1SceneHandlers scan_handlers      = { .on_enter = scan_on_enter      };
static const M1SceneHandlers saved_handlers     = { .on_enter = saved_on_enter     };
static const M1SceneHandlers advertise_handlers = { .on_enter = advertise_on_enter };
#ifdef M1_APP_BADBT_ENABLE
static const M1SceneHandlers badbt_handlers     = { .on_enter = badbt_on_enter     };
static const M1SceneHandlers btname_handlers    = { .on_enter = btname_on_enter    };
#endif
#ifdef M1_APP_BLE_SPAM_ENABLE
static const M1SceneHandlers blespam_handlers   = { .on_enter = blespam_on_enter   };
#endif
static const M1SceneHandlers info_handlers      = { .on_enter = info_on_enter      };

/*--- Menu scene — item count depends on enabled features ------------------*/

#if defined(M1_APP_BADBT_ENABLE) && defined(M1_APP_BLE_SPAM_ENABLE)
#define MENU_ITEM_COUNT  7
#elif defined(M1_APP_BADBT_ENABLE)
#define MENU_ITEM_COUNT  6
#elif defined(M1_APP_BLE_SPAM_ENABLE)
#define MENU_ITEM_COUNT  5
#else
#define MENU_ITEM_COUNT  4
#endif

#define MENU_VISIBLE  (MENU_ITEM_COUNT < 6 ? MENU_ITEM_COUNT : 6)

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Scan",
    "Saved",
    "Advertise",
#ifdef M1_APP_BADBT_ENABLE
    "Bad-BT",
    "BT Name",
#endif
#ifdef M1_APP_BLE_SPAM_ENABLE
    "BLE Spam",
#endif
    "BT Info",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    BtSceneScan,
    BtSceneSaved,
    BtSceneAdvertise,
#ifdef M1_APP_BADBT_ENABLE
    BtSceneBadBt,
    BtSceneBtName,
#endif
#ifdef M1_APP_BLE_SPAM_ENABLE
    BtSceneBleSpam,
#endif
    BtSceneInfo,
};

#endif /* M1_APP_BT_MANAGE_ENABLE */

/*==========================================================================*/
/* Menu handlers (shared by both modes)                                     */
/*==========================================================================*/

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
    m1_scene_draw_menu("Bluetooth", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, MENU_VISIBLE);
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/*--- Scene registry -------------------------------------------------------*/

#ifndef M1_APP_BT_MANAGE_ENABLE

static const M1SceneHandlers *const scene_registry[BtSceneCount] = {
    [BtSceneMenu]      = &menu_handlers,
    [BtSceneConfig]    = &config_handlers,
    [BtSceneScan]      = &scan_handlers,
    [BtSceneAdvertise] = &advertise_handlers,
};

#else /* M1_APP_BT_MANAGE_ENABLE */

static const M1SceneHandlers *const scene_registry[BtSceneCount] = {
    [BtSceneMenu]      = &menu_handlers,
    [BtSceneScan]      = &scan_handlers,
    [BtSceneSaved]     = &saved_handlers,
    [BtSceneAdvertise] = &advertise_handlers,
#ifdef M1_APP_BADBT_ENABLE
    [BtSceneBadBt]     = &badbt_handlers,
    [BtSceneBtName]    = &btname_handlers,
#endif
#ifdef M1_APP_BLE_SPAM_ENABLE
    [BtSceneBleSpam]   = &blespam_handlers,
#endif
    [BtSceneInfo]      = &info_handlers,
};

#endif /* M1_APP_BT_MANAGE_ENABLE */

/*--- Entry point ----------------------------------------------------------*/

void bt_scene_entry(void)
{
    m1_scene_run(scene_registry, BtSceneCount, menu_bluetooth_init, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
