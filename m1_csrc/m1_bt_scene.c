/* See COPYING.txt for license details. */

/**
 * @file   m1_bt_scene.c
 * @brief  Bluetooth Scene Manager — scene-based menu exposing SiN360 BLE tools.
 *
 * New BLE functions (scan, spam, sniffers, wardrive, detectors) come from
 * sincere360's m1_bt.c which uses the binary SPI command protocol.
 * Bad-BT (HID via BLE) is unchanged and uses the separate m1_badbt.c.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_bt.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_caps.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#include "m1_compile_cfg.h"

#ifdef M1_APP_BADBT_ENABLE
#include "m1_badbt.h"
#endif

/* ---- Scene IDs ---------------------------------------------------------- */
enum {
    BtSceneMenu = 0,

    /* Core */
    BtSceneScan,
    BtSceneAdvertise,
    BtSceneConfig,

    /* Sniffers submenu */
    BtSceneSnifferMenu,
    BtSceneSniffAnalyzer,
    BtSceneSniffGeneric,
    BtSceneSniffFlipper,
    BtSceneSniffAirtag,
    BtSceneMonitorAirtag,
    BtSceneSniffFlock,

    /* Wardrive */
    BtSceneWardrive,
    BtSceneWardriveContinu,
    BtSceneWardriveFlock,

    /* Spam submenu */
    BtSceneSpamMenu,
    BtSceneSpamSourApple,
    BtSceneSpamSwiftpair,
    BtSceneSpamSamsung,
    BtSceneSpamFlipper,
    BtSceneSpamAll,
    BtSceneSpoofAirtag,

    /* Detectors submenu */
    BtSceneDetectMenu,
    BtSceneDetectSkimmers,
    BtSceneDetectFlock,
    BtSceneDetectMeta,

    /* Bad-BT */
#ifdef M1_APP_BADBT_ENABLE
    BtSceneBadBt,
    BtSceneBtName,
#endif

    /* Saved / Info stubs */
#ifdef M1_APP_BT_MANAGE_ENABLE
    BtSceneSaved,
    BtSceneInfo,
#endif

    BtSceneCount
};

/* ---- Blocking delegate macro -------------------------------------------- */
#define DELEGATE(name, fn) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; fn(); m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

/* Capability-gated delegate: shows "not supported" screen and pops immediately
 * when the required ESP32 capability is absent.
 * m1_esp32_ensure_init() is called first so CMD_GET_STATUS can be queried even
 * when the transport was deinitialized by the previous delegate. */
#define DELEGATE_CAPPED(name, fn, cap, label) \
    static void name##_on_enter(M1SceneApp *app) { \
        (void)app; \
        m1_esp32_ensure_init(); \
        if (m1_esp32_require_cap((cap), (label))) { fn(); } \
        m1_esp32_deinit(); app->running = true; m1_scene_pop(app); }

DELEGATE(scan,            bluetooth_scan)
DELEGATE(advertise,       bluetooth_advertise)
DELEGATE(config,          bluetooth_config)

DELEGATE(sniff_analyzer,  ble_sniff_analyzer)
DELEGATE(sniff_generic,   ble_sniff_generic)
DELEGATE(sniff_flipper,   ble_sniff_flipper)
DELEGATE(sniff_airtag,    ble_sniff_airtag)
DELEGATE(monitor_airtag,  ble_monitor_airtag)
DELEGATE(sniff_flock,     ble_sniff_flock)

DELEGATE(wardrive,        ble_wardrive)
DELEGATE(wardrive_continu,ble_wardrive_continuous)
DELEGATE(wardrive_flock,  ble_wardrive_flock)

DELEGATE(spam_sour_apple, ble_spam_sour_apple)
DELEGATE(spam_swiftpair,  ble_spam_swiftpair)
DELEGATE(spam_samsung,    ble_spam_samsung)
DELEGATE(spam_flipper,    ble_spam_flipper)
DELEGATE(spam_all,        ble_spam_all)
DELEGATE(spoof_airtag,    ble_spoof_airtag)

DELEGATE(detect_skimmers, ble_detect_skimmers)
DELEGATE(detect_flock,    ble_detect_flock)
DELEGATE(detect_meta,     ble_detect_meta)

#ifdef M1_APP_BADBT_ENABLE
DELEGATE_CAPPED(badbt,  badbt_run,                M1_ESP32_CMD_AT_BLE_HID, "Bad-BT")
DELEGATE_CAPPED(btname, bluetooth_set_badbt_name, M1_ESP32_CMD_AT_BLE_HID, "BT Name")
#endif

#ifdef M1_APP_BT_MANAGE_ENABLE
DELEGATE_CAPPED(saved, bluetooth_saved_devices, M1_ESP32_CMD_AT_BT_MANAGE, "Saved Devices")
DELEGATE_CAPPED(info,  bluetooth_info,          M1_ESP32_CMD_AT_BT_MANAGE, "BT Info")
#endif

/* ---- Handler tables ----------------------------------------------------- */
#define HANDLERS(name) static const M1SceneHandlers name##_handlers = { .on_enter = name##_on_enter }

HANDLERS(scan);
HANDLERS(advertise);
HANDLERS(config);
HANDLERS(sniff_analyzer);
HANDLERS(sniff_generic);
HANDLERS(sniff_flipper);
HANDLERS(sniff_airtag);
HANDLERS(monitor_airtag);
HANDLERS(sniff_flock);
HANDLERS(wardrive);
HANDLERS(wardrive_continu);
HANDLERS(wardrive_flock);
HANDLERS(spam_sour_apple);
HANDLERS(spam_swiftpair);
HANDLERS(spam_samsung);
HANDLERS(spam_flipper);
HANDLERS(spam_all);
HANDLERS(spoof_airtag);
HANDLERS(detect_skimmers);
HANDLERS(detect_flock);
HANDLERS(detect_meta);
#ifdef M1_APP_BADBT_ENABLE
HANDLERS(badbt);
HANDLERS(btname);
#endif
#ifdef M1_APP_BT_MANAGE_ENABLE
HANDLERS(saved);
HANDLERS(info);
#endif

/* ---- Sniffers submenu --------------------------------------------------- */
#define SNIFFER_ITEM_COUNT  6
static const char *const sniffer_labels[SNIFFER_ITEM_COUNT] = {
    "Analyzer", "Generic", "Flipper",
    "AirTag Sniff", "AirTag Monitor", "Flock",
};
static const uint8_t sniffer_targets[SNIFFER_ITEM_COUNT] = {
    BtSceneSniffAnalyzer, BtSceneSniffGeneric, BtSceneSniffFlipper,
    BtSceneSniffAirtag, BtSceneMonitorAirtag, BtSceneSniffFlock,
};
static uint8_t sniffer_sel = 0, sniffer_scroll = 0;
static void sniffer_menu_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }
static bool sniffer_menu_event(M1SceneApp *app, M1SceneEvent ev) {
    return m1_scene_menu_event(app, ev, &sniffer_sel, &sniffer_scroll,
                               SNIFFER_ITEM_COUNT, M1_MENU_VIS(SNIFFER_ITEM_COUNT), sniffer_targets);
}
static void sniffer_menu_draw(M1SceneApp *app) {
    (void)app;
    m1_scene_draw_menu("BLE Sniffers", sniffer_labels, SNIFFER_ITEM_COUNT,
                       sniffer_sel, sniffer_scroll, M1_MENU_VIS(SNIFFER_ITEM_COUNT));
}
static const M1SceneHandlers sniffer_menu_handlers = {
    .on_enter = sniffer_menu_enter, .on_event = sniffer_menu_event,
    .on_exit = NULL, .draw = sniffer_menu_draw,
};

/* ---- BLE Spam submenu --------------------------------------------------- */
#define SPAM_ITEM_COUNT  6
static const char *const spam_labels[SPAM_ITEM_COUNT] = {
    "Sour Apple", "SwiftPair", "Samsung",
    "Flipper", "All", "AirTag Spoof",
};
static const uint8_t spam_targets[SPAM_ITEM_COUNT] = {
    BtSceneSpamSourApple, BtSceneSpamSwiftpair, BtSceneSpamSamsung,
    BtSceneSpamFlipper, BtSceneSpamAll, BtSceneSpoofAirtag,
};
static uint8_t spam_sel = 0, spam_scroll = 0;
static void spam_menu_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }
static bool spam_menu_event(M1SceneApp *app, M1SceneEvent ev) {
    return m1_scene_menu_event(app, ev, &spam_sel, &spam_scroll,
                               SPAM_ITEM_COUNT, M1_MENU_VIS(SPAM_ITEM_COUNT), spam_targets);
}
static void spam_menu_draw(M1SceneApp *app) {
    (void)app;
    m1_scene_draw_menu("BLE Spam", spam_labels, SPAM_ITEM_COUNT,
                       spam_sel, spam_scroll, M1_MENU_VIS(SPAM_ITEM_COUNT));
}
static const M1SceneHandlers spam_menu_handlers = {
    .on_enter = spam_menu_enter, .on_event = spam_menu_event,
    .on_exit = NULL, .draw = spam_menu_draw,
};

/* ---- Detectors submenu -------------------------------------------------- */
#define DETECT_ITEM_COUNT  3
static const char *const detect_labels[DETECT_ITEM_COUNT] = {
    "Skimmers", "Flock Track", "Meta Devices",
};
static const uint8_t detect_targets[DETECT_ITEM_COUNT] = {
    BtSceneDetectSkimmers, BtSceneDetectFlock, BtSceneDetectMeta,
};
static uint8_t detect_sel = 0, detect_scroll = 0;
static void detect_menu_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }
static bool detect_menu_event(M1SceneApp *app, M1SceneEvent ev) {
    return m1_scene_menu_event(app, ev, &detect_sel, &detect_scroll,
                               DETECT_ITEM_COUNT, M1_MENU_VIS(DETECT_ITEM_COUNT), detect_targets);
}
static void detect_menu_draw(M1SceneApp *app) {
    (void)app;
    m1_scene_draw_menu("BLE Detect", detect_labels, DETECT_ITEM_COUNT,
                       detect_sel, detect_scroll, M1_MENU_VIS(DETECT_ITEM_COUNT));
}
static const M1SceneHandlers detect_menu_handlers = {
    .on_enter = detect_menu_enter, .on_event = detect_menu_event,
    .on_exit = NULL, .draw = detect_menu_draw,
};

/* ---- Top-level menu ----------------------------------------------------- */
#if defined(M1_APP_BADBT_ENABLE) && defined(M1_APP_BT_MANAGE_ENABLE)
#define MENU_ITEM_COUNT  13
#elif defined(M1_APP_BADBT_ENABLE) || defined(M1_APP_BT_MANAGE_ENABLE)
#define MENU_ITEM_COUNT  11
#else
#define MENU_ITEM_COUNT  9
#endif

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
#ifdef M1_APP_BADBT_ENABLE
    "Bad-BT",
    "BT Name",
#endif
#ifdef M1_APP_BT_MANAGE_ENABLE
    "Saved Devices",
    "BT Info",
#endif
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
#ifdef M1_APP_BADBT_ENABLE
    BtSceneBadBt,
    BtSceneBtName,
#endif
#ifdef M1_APP_BT_MANAGE_ENABLE
    BtSceneSaved,
    BtSceneInfo,
#endif
};

static uint8_t menu_sel    = 0;
static uint8_t menu_scroll = 0;

static void menu_on_enter(M1SceneApp *app) { (void)app; app->need_redraw = true; }

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &menu_sel, &menu_scroll,
                               MENU_ITEM_COUNT, M1_MENU_VIS(MENU_ITEM_COUNT), menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Bluetooth", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, M1_MENU_VIS(MENU_ITEM_COUNT));
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/* ---- Scene registry ----------------------------------------------------- */
static const M1SceneHandlers *const scene_registry[BtSceneCount] = {
    [BtSceneMenu]           = &menu_handlers,
    [BtSceneScan]           = &scan_handlers,
    [BtSceneAdvertise]      = &advertise_handlers,
    [BtSceneConfig]         = &config_handlers,

    [BtSceneSnifferMenu]    = &sniffer_menu_handlers,
    [BtSceneSniffAnalyzer]  = &sniff_analyzer_handlers,
    [BtSceneSniffGeneric]   = &sniff_generic_handlers,
    [BtSceneSniffFlipper]   = &sniff_flipper_handlers,
    [BtSceneSniffAirtag]    = &sniff_airtag_handlers,
    [BtSceneMonitorAirtag]  = &monitor_airtag_handlers,
    [BtSceneSniffFlock]     = &sniff_flock_handlers,

    [BtSceneWardrive]       = &wardrive_handlers,
    [BtSceneWardriveContinu]= &wardrive_continu_handlers,
    [BtSceneWardriveFlock]  = &wardrive_flock_handlers,

    [BtSceneSpamMenu]       = &spam_menu_handlers,
    [BtSceneSpamSourApple]  = &spam_sour_apple_handlers,
    [BtSceneSpamSwiftpair]  = &spam_swiftpair_handlers,
    [BtSceneSpamSamsung]    = &spam_samsung_handlers,
    [BtSceneSpamFlipper]    = &spam_flipper_handlers,
    [BtSceneSpamAll]        = &spam_all_handlers,
    [BtSceneSpoofAirtag]    = &spoof_airtag_handlers,

    [BtSceneDetectMenu]     = &detect_menu_handlers,
    [BtSceneDetectSkimmers] = &detect_skimmers_handlers,
    [BtSceneDetectFlock]    = &detect_flock_handlers,
    [BtSceneDetectMeta]     = &detect_meta_handlers,

#ifdef M1_APP_BADBT_ENABLE
    [BtSceneBadBt]          = &badbt_handlers,
    [BtSceneBtName]         = &btname_handlers,
#endif
#ifdef M1_APP_BT_MANAGE_ENABLE
    [BtSceneSaved]          = &saved_handlers,
    [BtSceneInfo]           = &info_handlers,
#endif
};

/* ---- Entry point -------------------------------------------------------- */
void bt_scene_entry(void)
{
    m1_scene_run(scene_registry, BtSceneCount, menu_bluetooth_init, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
