/* See COPYING.txt for license details. */

/**
 * @file   m1_bt_scene_spam.c
 * @brief  Bluetooth BLE Spam sub-menu + spam/spoof delegates
 *         + Detectors sub-menu + detect delegates.
 *
 * Scenes covered:
 *   BtSceneSpamMenu       — BLE Spam sub-menu (8 items)
 *   BtSceneSpamSourApple  — Sour Apple spam delegate
 *   BtSceneSpamSwiftpair  — SwiftPair spam delegate
 *   BtSceneSpamSamsung    — Samsung spam delegate
 *   BtSceneSpamGoogleFP   — Google Fast-Pair spam delegate
 *   BtSceneSpamFlipper    — Flipper spam delegate
 *   BtSceneSpamAll        — All spam delegate
 *   BtSceneSpoofAirtag    — AirTag spoof delegate
 *   BtSceneDetectMenu     — Detectors sub-menu (3 items)
 *   BtSceneDetectSkimmers — Skimmers detect delegate
 *   BtSceneDetectFlock    — Flock detect delegate
 *   BtSceneDetectMeta     — Meta Devices detect delegate
 *
 * Phase E: uses `subghz_submenu_model_t` + `m1_submenu_draw/event` for
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
#include "m1_lib.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Blocking delegate macro                                                  */
/*==========================================================================*/

#define DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/*==========================================================================*/
/* Spam delegates                                                           */
/*==========================================================================*/

DELEGATE(spam_sour_apple, ble_spam_sour_apple)
DELEGATE(spam_swiftpair,  ble_spam_swiftpair)
DELEGATE(spam_samsung,    ble_spam_samsung)
DELEGATE(spam_google_fp,  ble_spam_google_fastpair)
DELEGATE(spam_flipper,    ble_spam_flipper)
DELEGATE(spam_all,        ble_spam_all)
DELEGATE(spoof_airtag,    ble_spoof_airtag)

const M1SceneHandlers bt_scene_spam_sour_apple_handlers = { .on_enter = spam_sour_apple_on_enter };
const M1SceneHandlers bt_scene_spam_swiftpair_handlers  = { .on_enter = spam_swiftpair_on_enter  };
const M1SceneHandlers bt_scene_spam_samsung_handlers    = { .on_enter = spam_samsung_on_enter    };
const M1SceneHandlers bt_scene_spam_google_fp_handlers  = { .on_enter = spam_google_fp_on_enter  };
const M1SceneHandlers bt_scene_spam_flipper_handlers    = { .on_enter = spam_flipper_on_enter    };
const M1SceneHandlers bt_scene_spam_all_handlers        = { .on_enter = spam_all_on_enter        };
const M1SceneHandlers bt_scene_spoof_airtag_handlers    = { .on_enter = spoof_airtag_on_enter    };

/*==========================================================================*/
/* Detect delegates                                                         */
/*==========================================================================*/

DELEGATE(detect_skimmers, ble_detect_skimmers)
DELEGATE(detect_flock,    ble_detect_flock)
DELEGATE(detect_meta,     ble_detect_meta)

const M1SceneHandlers bt_scene_detect_skimmers_handlers = { .on_enter = detect_skimmers_on_enter };
const M1SceneHandlers bt_scene_detect_flock_handlers    = { .on_enter = detect_flock_on_enter    };
const M1SceneHandlers bt_scene_detect_meta_handlers     = { .on_enter = detect_meta_on_enter     };

/*==========================================================================*/
/* BLE Spam sub-menu                                                        */
/*==========================================================================*/

#define SPAM_ITEM_COUNT  7

static const char *const spam_labels[SPAM_ITEM_COUNT] = {
    "Sour Apple", "SwiftPair", "Samsung",
    "Google FP", "Flipper", "All", "AirTag Spoof",
};

static const uint8_t spam_targets[SPAM_ITEM_COUNT] = {
    BtSceneSpamSourApple, BtSceneSpamSwiftpair, BtSceneSpamSamsung,
    BtSceneSpamGoogleFP, BtSceneSpamFlipper, BtSceneSpamAll, BtSceneSpoofAirtag,
};

static subghz_submenu_model_t s_spam_model;

static void spam_menu_enter(M1SceneApp *app)
{
    (void)app;
    subghz_submenu_model_init(&s_spam_model, SPAM_ITEM_COUNT,
                              M1_MENU_VIS(SPAM_ITEM_COUNT));
    app->need_redraw = true;
}

static bool spam_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_spam_model, spam_targets);
}

static void spam_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_spam_model, "BLE Spam", spam_labels);
}

const M1SceneHandlers bt_scene_spam_menu_handlers = {
    .on_enter = spam_menu_enter,
    .on_event = spam_menu_event,
    .on_exit  = NULL,
    .draw     = spam_menu_draw,
};

/*==========================================================================*/
/* Detectors sub-menu                                                       */
/*==========================================================================*/

#define DETECT_ITEM_COUNT  3

static const char *const detect_labels[DETECT_ITEM_COUNT] = {
    "Skimmers", "Flock Track", "Meta Devices",
};

static const uint8_t detect_targets[DETECT_ITEM_COUNT] = {
    BtSceneDetectSkimmers, BtSceneDetectFlock, BtSceneDetectMeta,
};

static subghz_submenu_model_t s_detect_model;

static void detect_menu_enter(M1SceneApp *app)
{
    (void)app;
    subghz_submenu_model_init(&s_detect_model, DETECT_ITEM_COUNT,
                              M1_MENU_VIS(DETECT_ITEM_COUNT));
    app->need_redraw = true;
}

static bool detect_menu_event(M1SceneApp *app, M1SceneEvent ev)
{
    return m1_submenu_event(app, ev, &s_detect_model, detect_targets);
}

static void detect_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_detect_model, "BLE Detect", detect_labels);
}

const M1SceneHandlers bt_scene_detect_menu_handlers = {
    .on_enter = detect_menu_enter,
    .on_event = detect_menu_event,
    .on_exit  = NULL,
    .draw     = detect_menu_draw,
};
