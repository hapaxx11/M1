/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_connect.c
 * @brief  WiFi Connected menu + connect-feature delegates.
 *
 * Compile-gated by M1_APP_WIFI_CONNECT_ENABLE.
 *
 * These scenes are NOT gated on ESP32_FEATURE_WIFI_JOIN because:
 *   - Credentials are stored on the SD card (0:/System/wifi_creds.bin) and
 *     are completely independent of which ESP32 firmware is installed.
 *   - wifi_connect_from_saved() dispatches to AT+CWJAP (AT firmware) or
 *     CMD_WIFI_JOIN binary SPI (SiN360) based on the runtime capability bitmap.
 *   - wifi_show_status() reads local state only — no ESP32 interaction.
 *   - wifi_disconnect() dispatches to AT+CWQAP (AT firmware) or
 *     CMD_WIFI_DISCONNECT binary SPI (SiN360).
 *
 * Scenes covered:
 *   WifiSceneConnectedMenu — Connected sub-menu (3 items: Status, Net Scan, Disconnect)
 *   WifiSceneSaved         — Saved Networks delegate
 *   WifiSceneStatus        — Status delegate
 *   WifiSceneDisconnect    — Disconnect delegate
 */

#include "m1_compile_cfg.h"

#ifdef M1_APP_WIFI_CONNECT_ENABLE

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_wifi_scene.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_wifi.h"
#include "m1_esp32_hal.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Connect-feature delegates (no capability gate)                           */
/*==========================================================================*/

/* Saved Networks: credentials are SD-based; wifi_connect_from_saved()
 * handles both AT and binary SPI firmware automatically. */
static void saved_on_enter(M1SceneApp *app)
{
    (void)app;
    bool was_connected = wifi_is_connected();
    m1_esp32_ensure_init();
    wifi_saved_networks();
    m1_esp32_deinit();
    app->running = true;
    if (!was_connected && wifi_is_connected()) {
        m1_scene_replace(app, WifiSceneConnectedMenu);
        return;
    }
    m1_scene_pop(app);
}

/* Status: shows local s_wifi_stub_connected / s_wifi_stub_ssid state only —
 * no ESP32 interaction required. */
static void status_on_enter(M1SceneApp *app)
{
    (void)app;
    wifi_show_status();
    app->running = true;
    m1_scene_pop(app);
}

/* Disconnect: wifi_disconnect() dispatches to AT+CWQAP or CMD_WIFI_DISCONNECT
 * based on the runtime capability bitmap. */
static void disconnect_on_enter(M1SceneApp *app)
{
    (void)app;
    m1_esp32_ensure_init();
    wifi_disconnect();
    m1_esp32_deinit();
    app->running = true;
    m1_scene_pop(app);
}

const M1SceneHandlers wifi_scene_saved_handlers      = { .on_enter = saved_on_enter      };
const M1SceneHandlers wifi_scene_status_handlers     = { .on_enter = status_on_enter     };
const M1SceneHandlers wifi_scene_disconnect_handlers = { .on_enter = disconnect_on_enter };

/*==========================================================================*/
/* Connected sub-menu (3 items — requires active WiFi connection)           */
/*==========================================================================*/

#define CONNECTED_ITEM_COUNT  3

static const char *const connected_labels[CONNECTED_ITEM_COUNT] = {
    "Status", "Net Scan", "Disconnect",
};

static const uint8_t connected_targets[CONNECTED_ITEM_COUNT] = {
    WifiSceneStatus, WifiSceneNetMenu, WifiSceneDisconnect,
};

static subghz_submenu_model_t s_connected_model;

static void connected_menu_enter(M1SceneApp *app)
{
    if (!wifi_require_connected()) {
        m1_scene_pop(app);
        return;
    }
    if (s_connected_model.item_count == 0)
        subghz_submenu_model_init(&s_connected_model, CONNECTED_ITEM_COUNT,
                                  M1_MENU_VIS(CONNECTED_ITEM_COUNT));
    app->need_redraw = true;
}

static bool connected_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_connected_model, connected_targets);
}

static void connected_menu_draw(M1SceneApp *app)
{
    (void)app;
    const char *ssid = wifi_get_connected_ssid();
    m1_submenu_draw(&s_connected_model,
                    (ssid && ssid[0]) ? ssid : "Connected",
                    connected_labels);
}

const M1SceneHandlers wifi_scene_connected_menu_handlers = {
    .on_enter = connected_menu_enter,
    .on_event = connected_menu_event,
    .on_exit  = NULL,
    .draw     = connected_menu_draw,
};

#endif /* M1_APP_WIFI_CONNECT_ENABLE */
