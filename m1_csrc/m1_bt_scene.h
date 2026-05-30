/* See COPYING.txt for license details. */

/**
 * @file   m1_bt_scene.h
 * @brief  Bluetooth Scene Manager — scene IDs and per-file handler exports.
 *
 * Scene implementation is split across four files following the
 * m1_subghz_scene_*.c convention (Phase D):
 *
 *   m1_bt_scene_menu.c    — top-level 14-item menu + core delegates
 *                           (Scan, Advertise, Config)
 *   m1_bt_scene_sniff.c   — BLE Sniffers sub-menu + 6 sniffer delegates
 *                           + 3 Wardrive delegates
 *   m1_bt_scene_spam.c    — BLE Spam sub-menu + 8 spam/spoof delegates
 *                           + Detectors sub-menu + 3 detect delegates
 *   m1_bt_scene_badbt.c   — Bad-BT (BLE HID), BT Name, GATT Discovery,
 *                           Saved Devices, BT Info delegates
 *
 * m1_bt_scene.c owns only the scene_registry[] table and bt_scene_entry().
 */

#ifndef M1_BT_SCENE_H_
#define M1_BT_SCENE_H_

#include "m1_scene.h"

/*==========================================================================*/
/* Scene IDs                                                                */
/*==========================================================================*/

typedef enum {
    BtSceneMenu = 0,

    /* Core */
    BtSceneScan,
    BtSceneAdvertise,
    BtSceneConfig,

    /* Sniffers sub-menu */
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

    /* Spam sub-menu */
    BtSceneSpamMenu,
    BtSceneSpamSourApple,
    BtSceneSpamSwiftpair,
    BtSceneSpamSamsung,
    BtSceneSpamGoogleFP,
    BtSceneSpamFlipper,
    BtSceneSpamAll,
    BtSceneSpoofAirtag,

    /* Detectors sub-menu */
    BtSceneDetectMenu,
    BtSceneDetectSkimmers,
    BtSceneDetectFlock,
    BtSceneDetectMeta,

    /* Bad-BT */
    BtSceneBadBt,
    BtSceneBtName,

    /* GATT */
    BtSceneGattDiscovery,

    /* Saved / Info */
    BtSceneSaved,
    BtSceneInfo,

    BtSceneCount
} BtSceneId;

/*==========================================================================*/
/* Per-file handler exports                                                 */
/*==========================================================================*/

/* m1_bt_scene_menu.c */
extern const M1SceneHandlers bt_scene_menu_handlers;
extern const M1SceneHandlers bt_scene_scan_handlers;
extern const M1SceneHandlers bt_scene_advertise_handlers;
extern const M1SceneHandlers bt_scene_config_handlers;

/* m1_bt_scene_sniff.c */
extern const M1SceneHandlers bt_scene_sniffer_menu_handlers;
extern const M1SceneHandlers bt_scene_sniff_analyzer_handlers;
extern const M1SceneHandlers bt_scene_sniff_generic_handlers;
extern const M1SceneHandlers bt_scene_sniff_flipper_handlers;
extern const M1SceneHandlers bt_scene_sniff_airtag_handlers;
extern const M1SceneHandlers bt_scene_monitor_airtag_handlers;
extern const M1SceneHandlers bt_scene_sniff_flock_handlers;
extern const M1SceneHandlers bt_scene_wardrive_handlers;
extern const M1SceneHandlers bt_scene_wardrive_continu_handlers;
extern const M1SceneHandlers bt_scene_wardrive_flock_handlers;

/* m1_bt_scene_spam.c */
extern const M1SceneHandlers bt_scene_spam_menu_handlers;
extern const M1SceneHandlers bt_scene_spam_sour_apple_handlers;
extern const M1SceneHandlers bt_scene_spam_swiftpair_handlers;
extern const M1SceneHandlers bt_scene_spam_samsung_handlers;
extern const M1SceneHandlers bt_scene_spam_google_fp_handlers;
extern const M1SceneHandlers bt_scene_spam_flipper_handlers;
extern const M1SceneHandlers bt_scene_spam_all_handlers;
extern const M1SceneHandlers bt_scene_spoof_airtag_handlers;
extern const M1SceneHandlers bt_scene_detect_menu_handlers;
extern const M1SceneHandlers bt_scene_detect_skimmers_handlers;
extern const M1SceneHandlers bt_scene_detect_flock_handlers;
extern const M1SceneHandlers bt_scene_detect_meta_handlers;

/* m1_bt_scene_badbt.c */
extern const M1SceneHandlers bt_scene_badbt_handlers;
extern const M1SceneHandlers bt_scene_btname_handlers;
extern const M1SceneHandlers bt_scene_gatt_discovery_handlers;
extern const M1SceneHandlers bt_scene_saved_handlers;
extern const M1SceneHandlers bt_scene_info_handlers;

/*==========================================================================*/
/* Entry point                                                              */
/*==========================================================================*/

void bt_scene_entry(void);

#endif /* M1_BT_SCENE_H_ */
