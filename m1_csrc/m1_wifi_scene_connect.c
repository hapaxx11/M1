/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_connect.c
 * @brief  WiFi connect-feature delegates (Saved Networks, Status, Disconnect).
 *
 * Compile-gated by M1_APP_WIFI_CONNECT_ENABLE.
 *
 * These three scenes are NOT gated on ESP32_FEATURE_WIFI_JOIN because:
 *   - Credentials are stored on the SD card (0:/System/wifi_creds.bin) and
 *     are completely independent of which ESP32 firmware is installed.
 *   - wifi_connect_from_saved() dispatches to AT+CWJAP (AT firmware) or
 *     CMD_WIFI_JOIN binary SPI (SiN360) based on the runtime capability bitmap.
 *   - wifi_show_status() reads local state only — no ESP32 interaction.
 *   - wifi_disconnect() dispatches to AT+CWQAP (AT firmware) or
 *     CMD_WIFI_DISCONNECT binary SPI (SiN360).
 *
 * Scenes covered:
 *   WifiSceneSaved      — Saved Networks delegate
 *   WifiSceneStatus     — Status delegate
 *   WifiSceneDisconnect — Disconnect delegate
 */

#include "m1_compile_cfg.h"

#ifdef M1_APP_WIFI_CONNECT_ENABLE

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_wifi_scene.h"
#include "m1_scene.h"
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
    m1_esp32_ensure_init();
    wifi_saved_networks();
    m1_esp32_deinit();
    app->running = true;
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

#endif /* M1_APP_WIFI_CONNECT_ENABLE */
