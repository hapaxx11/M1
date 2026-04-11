/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene.c
 * @brief  WiFi Scene Manager — scene-based menu with blocking delegates.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_wifi.h"
#include "m1_deauth.h"
#include "m1_802154.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/* Scene IDs */
enum {
    WifiSceneMenu = 0,
    WifiSceneScanConnect,
    WifiSceneDeauth,
    WifiSceneZigbee,
    WifiSceneThread,
    WifiSceneSaved,
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    WifiSceneStatus,
    WifiSceneDisconnect,
#endif
    WifiSceneCount
};

/*--- Blocking delegates ---------------------------------------------------*/

static void scan_connect_on_enter(M1SceneApp *app)
{
    (void)app;
    wifi_scan_ap();
    app->running = true;
    m1_scene_pop(app);
}

static void deauth_on_enter(M1SceneApp *app)
{
    (void)app;
    wifi_deauth();
    app->running = true;
    m1_scene_pop(app);
}

static void zigbee_on_enter(M1SceneApp *app)
{
    (void)app;
    zigbee_scan();
    app->running = true;
    m1_scene_pop(app);
}

static void thread_on_enter(M1SceneApp *app)
{
    (void)app;
    thread_scan();
    app->running = true;
    m1_scene_pop(app);
}

static void saved_on_enter(M1SceneApp *app)
{
    (void)app;
    wifi_saved_networks();
    app->running = true;
    m1_scene_pop(app);
}

#ifdef M1_APP_WIFI_CONNECT_ENABLE
static void status_on_enter(M1SceneApp *app)
{
    (void)app;
    wifi_show_status();
    app->running = true;
    m1_scene_pop(app);
}

static void disconnect_on_enter(M1SceneApp *app)
{
    (void)app;
    wifi_disconnect();
    app->running = true;
    m1_scene_pop(app);
}
#endif

/*--- Handler tables -------------------------------------------------------*/

static const M1SceneHandlers scan_connect_handlers = { .on_enter = scan_connect_on_enter };
static const M1SceneHandlers deauth_handlers       = { .on_enter = deauth_on_enter       };
static const M1SceneHandlers zigbee_handlers       = { .on_enter = zigbee_on_enter       };
static const M1SceneHandlers thread_handlers       = { .on_enter = thread_on_enter       };
static const M1SceneHandlers saved_handlers        = { .on_enter = saved_on_enter        };

#ifdef M1_APP_WIFI_CONNECT_ENABLE
static const M1SceneHandlers status_handlers     = { .on_enter = status_on_enter     };
static const M1SceneHandlers disconnect_handlers = { .on_enter = disconnect_on_enter };
#endif

/*--- Menu scene -----------------------------------------------------------*/

#ifdef M1_APP_WIFI_CONNECT_ENABLE
#define MENU_ITEM_COUNT  7
#define MENU_VISIBLE     6
#else
#define MENU_ITEM_COUNT  5
#define MENU_VISIBLE     5
#endif

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "WiFi Scan+Connect",
    "Deauther",
    "Zigbee Scan",
    "Thread Scan",
    "Saved Networks",
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    "Status",
    "Disconnect",
#endif
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    WifiSceneScanConnect,
    WifiSceneDeauth,
    WifiSceneZigbee,
    WifiSceneThread,
    WifiSceneSaved,
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    WifiSceneStatus,
    WifiSceneDisconnect,
#endif
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
    m1_scene_draw_menu("WiFi", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, MENU_VISIBLE);
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/*--- Scene registry -------------------------------------------------------*/

static const M1SceneHandlers *const scene_registry[WifiSceneCount] = {
    [WifiSceneMenu]        = &menu_handlers,
    [WifiSceneScanConnect] = &scan_connect_handlers,
    [WifiSceneDeauth]      = &deauth_handlers,
    [WifiSceneZigbee]      = &zigbee_handlers,
    [WifiSceneThread]      = &thread_handlers,
    [WifiSceneSaved]       = &saved_handlers,
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    [WifiSceneStatus]      = &status_handlers,
    [WifiSceneDisconnect]  = &disconnect_handlers,
#endif
};

/*--- Entry point ----------------------------------------------------------*/

void wifi_scene_entry(void)
{
    m1_scene_run(scene_registry, WifiSceneCount, menu_wifi_init, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
