/* See COPYING.txt for license details. */

/**
 * @file   m1_bt_scene.c
 * @brief  Bluetooth Scene Manager — scene registry and entry point.
 *
 * Scene implementations live in per-group files (Phase D split):
 *   m1_bt_scene_menu.c   — top menu + Scan/Advertise/Config delegates
 *   m1_bt_scene_sniff.c  — BLE Sniffers sub-menu + sniffer + wardrive delegates
 *   m1_bt_scene_spam.c   — BLE Spam sub-menu + spam/spoof + Detectors delegates
 *   m1_bt_scene_badbt.c  — Bad-BT / BT Name / GATT / Saved / Info delegates
 */

#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_bt_scene.h"
#include "m1_scene.h"
#include "m1_bt.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Scene registry                                                           */
/*==========================================================================*/

static const M1SceneHandlers *const scene_registry[BtSceneCount] = {
    [BtSceneMenu]           = &bt_scene_menu_handlers,
    [BtSceneScan]           = &bt_scene_scan_handlers,
    [BtSceneAdvertise]      = &bt_scene_advertise_handlers,
    [BtSceneConfig]         = &bt_scene_config_handlers,

    [BtSceneSnifferMenu]    = &bt_scene_sniffer_menu_handlers,
    [BtSceneSniffAnalyzer]  = &bt_scene_sniff_analyzer_handlers,
    [BtSceneSniffGeneric]   = &bt_scene_sniff_generic_handlers,
    [BtSceneSniffFlipper]   = &bt_scene_sniff_flipper_handlers,
    [BtSceneSniffAirtag]    = &bt_scene_sniff_airtag_handlers,
    [BtSceneMonitorAirtag]  = &bt_scene_monitor_airtag_handlers,
    [BtSceneSniffFlock]     = &bt_scene_sniff_flock_handlers,

    [BtSceneWardrive]       = &bt_scene_wardrive_handlers,
    [BtSceneWardriveContinu]= &bt_scene_wardrive_continu_handlers,
    [BtSceneWardriveFlock]  = &bt_scene_wardrive_flock_handlers,

    [BtSceneSpamMenu]       = &bt_scene_spam_menu_handlers,
    [BtSceneSpamSourApple]  = &bt_scene_spam_sour_apple_handlers,
    [BtSceneSpamSwiftpair]  = &bt_scene_spam_swiftpair_handlers,
    [BtSceneSpamSamsung]    = &bt_scene_spam_samsung_handlers,
    [BtSceneSpamGoogleFP]   = &bt_scene_spam_google_fp_handlers,
    [BtSceneSpamFlipper]    = &bt_scene_spam_flipper_handlers,
    [BtSceneSpamAll]        = &bt_scene_spam_all_handlers,
    [BtSceneSpoofAirtag]    = &bt_scene_spoof_airtag_handlers,

    [BtSceneDetectMenu]     = &bt_scene_detect_menu_handlers,
    [BtSceneDetectSkimmers] = &bt_scene_detect_skimmers_handlers,
    [BtSceneDetectFlock]    = &bt_scene_detect_flock_handlers,
    [BtSceneDetectMeta]     = &bt_scene_detect_meta_handlers,

    [BtSceneBadBt]          = &bt_scene_badbt_handlers,
    [BtSceneBtName]         = &bt_scene_btname_handlers,
    [BtSceneGattDiscovery]  = &bt_scene_gatt_discovery_handlers,
    [BtSceneSaved]          = &bt_scene_saved_handlers,
    [BtSceneInfo]           = &bt_scene_info_handlers,
};

/*==========================================================================*/
/* Entry point                                                              */
/*==========================================================================*/

void bt_scene_entry(void)
{
    m1_scene_run(scene_registry, BtSceneCount, menu_bluetooth_init, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
