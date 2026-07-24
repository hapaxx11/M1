/* See COPYING.txt for license details. */

/**
 * @file   m1_bt_scene_sniff.c
 * @brief  Bluetooth BLE Sniffers sub-menu + sniffer and wardrive delegates.
 *
 * Scenes covered:
 *   BtSceneSnifferMenu    — BLE Sniffers sub-menu (6 items)
 *   BtSceneSniffAnalyzer  — Analyzer sniffer delegate
 *   BtSceneSniffGeneric   — Generic sniffer delegate
 *   BtSceneSniffFlipper   — Flipper sniffer delegate
 *   BtSceneSniffAirtag    — AirTag sniff delegate
 *   BtSceneMonitorAirtag  — AirTag monitor delegate
 *   BtSceneSniffFlock     — Flock sniffer delegate
 *   BtSceneWardrive       — Wardrive delegate
 *   BtSceneWardriveContinu— Wardrive continuous delegate
 *   BtSceneWardriveFlock  — Wardrive Flock delegate
 *
 * Submenu model: uses `subghz_submenu_model_t` + `m1_submenu_draw/event` for
 * consistent font-aware layout and automatic visible-count sync.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_bt_scene.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_bt.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_caps.h"
#include "esp32_feature_map.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Blocking delegate macro                                                  */
/*==========================================================================*/

#define DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/* Capability-gated blocking delegate — see m1_bt_scene_badbt.c for the
 * canonical documentation of this pattern.  Only sniffers that actually
 * touch the ESP32 (via CMD_BLE_SCAN_START/NEXT_RAW, ESP32_FEATURE_BLE_SCAN)
 * are gated here — monitor_airtag / sniff_flock below are placeholder
 * ("not yet implemented") stubs with no ESP32 dependency. */
#define DELEGATE_FEATURE(name, fn, fid) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; \
        m1_esp32_ensure_init(); \
        if (m1_esp32_require_cap(esp32_feature_required_caps(fid), \
                                  esp32_feature_label(fid))) { fn(); } \
        m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Sniffer delegates                                                        */
/*==========================================================================*/

DELEGATE_FEATURE(sniff_analyzer, ble_sniff_analyzer, ESP32_FEATURE_BLE_SCAN)
DELEGATE_FEATURE(sniff_generic,  ble_sniff_generic,  ESP32_FEATURE_BLE_SCAN)
DELEGATE_FEATURE(sniff_flipper,  ble_sniff_flipper,  ESP32_FEATURE_BLE_SCAN)
DELEGATE_FEATURE(sniff_airtag,   ble_sniff_airtag,   ESP32_FEATURE_BLE_SCAN)
DELEGATE(monitor_airtag,  ble_monitor_airtag)
DELEGATE(sniff_flock,     ble_sniff_flock)

const M1SceneHandlers bt_scene_sniff_analyzer_handlers  = { .on_enter = sniff_analyzer_on_enter  };
const M1SceneHandlers bt_scene_sniff_generic_handlers   = { .on_enter = sniff_generic_on_enter   };
const M1SceneHandlers bt_scene_sniff_flipper_handlers   = { .on_enter = sniff_flipper_on_enter   };
const M1SceneHandlers bt_scene_sniff_airtag_handlers    = { .on_enter = sniff_airtag_on_enter    };
const M1SceneHandlers bt_scene_monitor_airtag_handlers  = { .on_enter = monitor_airtag_on_enter  };
const M1SceneHandlers bt_scene_sniff_flock_handlers     = { .on_enter = sniff_flock_on_enter     };

/*==========================================================================*/
/* Wardrive delegates                                                       */
/*==========================================================================*/

DELEGATE(wardrive,         ble_wardrive)
DELEGATE(wardrive_continu, ble_wardrive_continuous)
DELEGATE(wardrive_flock,   ble_wardrive_flock)

const M1SceneHandlers bt_scene_wardrive_handlers        = { .on_enter = wardrive_on_enter         };
const M1SceneHandlers bt_scene_wardrive_continu_handlers= { .on_enter = wardrive_continu_on_enter };
const M1SceneHandlers bt_scene_wardrive_flock_handlers  = { .on_enter = wardrive_flock_on_enter   };

/*==========================================================================*/
/* BLE Sniffers sub-menu                                                    */
/*==========================================================================*/

#define SNIFFER_ITEM_COUNT  6

static const char *const sniffer_labels[SNIFFER_ITEM_COUNT] = {
    "Analyzer", "Generic", "Flipper",
    "AirTag Sniff", "AirTag Monitor", "Flock",
};

static const uint8_t sniffer_targets[SNIFFER_ITEM_COUNT] = {
    BtSceneSniffAnalyzer, BtSceneSniffGeneric, BtSceneSniffFlipper,
    BtSceneSniffAirtag, BtSceneMonitorAirtag, BtSceneSniffFlock,
};

static subghz_submenu_model_t s_sniffer_model;

static void sniffer_menu_enter(M1SceneApp *app)
{
    (void)app;
    if (s_sniffer_model.item_count == 0)
        subghz_submenu_model_init(&s_sniffer_model, SNIFFER_ITEM_COUNT,
                                  M1_MENU_VIS(SNIFFER_ITEM_COUNT));
    app->need_redraw = true;
}

static bool sniffer_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_sniffer_model, sniffer_targets);
}

static void sniffer_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_sniffer_model, "BLE Sniffers", sniffer_labels);
}

const M1SceneHandlers bt_scene_sniffer_menu_handlers = {
    .on_enter = sniffer_menu_enter,
    .on_event = sniffer_menu_event,
    .on_exit  = NULL,
    .draw     = sniffer_menu_draw,
};
