/* See COPYING.txt for license details. */

/**
 * @file   m1_wifi_scene_connect.c
 * @brief  WiFi connect-feature delegates (Saved Networks, Status, Disconnect).
 *
 * Compile-gated by M1_APP_WIFI_CONNECT_ENABLE.  All three scenes use
 * DELEGATE_FEATURE to guard on ESP32_FEATURE_WIFI_JOIN (AT-firmware only).
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
#include "m1_esp32_caps.h"
#include "esp32_feature_map.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Capability-gated delegate macro                                          */
/*==========================================================================*/

/* Shows "not supported" screen when the required ESP32 capability is absent.
 * m1_esp32_ensure_init() is called first so CMD_GET_STATUS can be queried even
 * when the transport was deinitialized by the previous delegate.
 * Uses the esp32_feature_map table (Phase C) so cap bits and UI labels are
 * maintained in one place rather than inline at each call site. */
#define DELEGATE_FEATURE(name, fn, fid) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; \
        m1_esp32_ensure_init(); \
        if (m1_esp32_require_cap(esp32_feature_required_caps(fid), \
                                  esp32_feature_label(fid))) { fn(); } \
        m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Connect-feature delegates                                                */
/*==========================================================================*/

DELEGATE_FEATURE(saved,      wifi_saved_networks, ESP32_FEATURE_WIFI_JOIN)
DELEGATE_FEATURE(status,     wifi_show_status,    ESP32_FEATURE_WIFI_JOIN)
DELEGATE_FEATURE(disconnect, wifi_disconnect,     ESP32_FEATURE_WIFI_JOIN)

const M1SceneHandlers wifi_scene_saved_handlers      = { .on_enter = saved_on_enter      };
const M1SceneHandlers wifi_scene_status_handlers     = { .on_enter = status_on_enter     };
const M1SceneHandlers wifi_scene_disconnect_handlers = { .on_enter = disconnect_on_enter };

#endif /* M1_APP_WIFI_CONNECT_ENABLE */
