/* See COPYING.txt for license details. */

/**
 * @file   m1_bt_scene_badbt.c
 * @brief  Bluetooth capability-gated delegates: Bad-BT (BLE HID), BT Name,
 *         GATT Discovery, Saved Devices, and BT Info.
 *
 * All scenes here require an ESP32 capability check (DELEGATE_FEATURE) and
 * use the esp32_feature_map table (Phase C) to resolve cap bits and UI labels.
 *
 * Scenes covered:
 *   BtSceneBadBt          — Bad-BT BLE HID injection (BLE_HID cap)
 *   BtSceneBtName         — Set Bad-BT device name (BLE_HID cap)
 *   BtSceneGattDiscovery  — BLE GATT service discovery (BLE_GATT cap)
 *   BtSceneSaved          — Saved/paired BT devices (BT_MANAGE cap)
 *   BtSceneInfo           — BT adapter info (BT_MANAGE cap)
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_bt_scene.h"
#include "m1_scene.h"
#include "m1_bt.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_caps.h"
#include "esp32_feature_map.h"
#include "m1_badbt.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Capability-gated delegate macro                                          */
/*==========================================================================*/

/* Shows "not supported" screen and pops immediately when the required ESP32
 * capability is absent.  m1_esp32_ensure_init() is called first so
 * CMD_GET_STATUS can be queried even when the transport was deinitialized by
 * the previous delegate.  Uses the esp32_feature_map table (Phase C). */
#define DELEGATE_FEATURE(name, fn, fid) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; \
        m1_esp32_ensure_init(); \
        if (m1_esp32_require_cap(esp32_feature_required_caps(fid), \
                                  esp32_feature_label(fid))) { fn(); } \
        m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Delegates                                                                */
/*==========================================================================*/

DELEGATE_FEATURE(badbt,          badbt_run,                ESP32_FEATURE_BLE_HID)
DELEGATE_FEATURE(btname,         bluetooth_set_badbt_name, ESP32_FEATURE_BLE_HID)
DELEGATE_FEATURE(gatt_discovery, ble_gatt_discovery,       ESP32_FEATURE_BLE_GATT)
DELEGATE_FEATURE(saved,          bluetooth_saved_devices,  ESP32_FEATURE_BT_MANAGE)
DELEGATE_FEATURE(info,           bluetooth_info,           ESP32_FEATURE_BT_MANAGE)

const M1SceneHandlers bt_scene_badbt_handlers         = { .on_enter = badbt_on_enter          };
const M1SceneHandlers bt_scene_btname_handlers        = { .on_enter = btname_on_enter         };
const M1SceneHandlers bt_scene_gatt_discovery_handlers= { .on_enter = gatt_discovery_on_enter };
const M1SceneHandlers bt_scene_saved_handlers         = { .on_enter = saved_on_enter          };
const M1SceneHandlers bt_scene_info_handlers          = { .on_enter = info_on_enter           };
