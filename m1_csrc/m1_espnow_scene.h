/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene.h
 * @brief  ESP-NOW Peer Link Scene Manager — scene IDs and handler exports.
 *
 * Scene implementation:
 *   m1_espnow_scene_main.c     — top-level menu
 *   m1_espnow_scene_scan.c     — peer discovery / scan list + pairing
 *   m1_espnow_scene_messages.c — short text messages
 *   m1_espnow_scene_send.c     — capture sender picker/progress
 *   m1_espnow_scene_transfer.c — file receive progress
 *   m1_espnow_scene_tictactoe.c — Tic-Tac-Toe game
 *
 * m1_espnow_scene.c owns the scene_registry[] table and espnow_scene_entry().
 */

#ifndef M1_ESPNOW_SCENE_H_
#define M1_ESPNOW_SCENE_H_

#include "m1_scene.h"

/*==========================================================================*/
/* Scene IDs                                                                */
/*==========================================================================*/

typedef enum {
    EspnowSceneMenu = 0,

    /* Peer discovery / pairing */
    EspnowSceneScan,
    EspnowScenePair,

    /* File transfer */
    EspnowSceneTransfer,
    EspnowSceneSendCapture,
    EspnowSceneSendProgress,

    /* Messaging */
    EspnowSceneMessages,

    /* Games */
    EspnowSceneTicTacToe,

    EspnowSceneCount
} EspnowSceneId;

/*==========================================================================*/
/* Per-file handler exports                                                 */
/*==========================================================================*/

/* m1_espnow_scene_main.c */
extern const M1SceneHandlers espnow_scene_menu_handlers;

/* m1_espnow_scene_scan.c */
extern const M1SceneHandlers espnow_scene_scan_handlers;
extern const M1SceneHandlers espnow_scene_pair_handlers;

/* m1_espnow_scene_transfer.c */
extern const M1SceneHandlers espnow_scene_transfer_handlers;

/* m1_espnow_scene_send.c */
extern const M1SceneHandlers espnow_scene_send_capture_handlers;
extern const M1SceneHandlers espnow_scene_send_progress_handlers;

/* m1_espnow_scene_messages.c */
extern const M1SceneHandlers espnow_scene_messages_handlers;

/* m1_espnow_scene_tictactoe.c */
extern const M1SceneHandlers espnow_scene_tictactoe_handlers;

/*==========================================================================*/
/* Entry point                                                              */
/*==========================================================================*/

void espnow_scene_entry(void);

#endif /* M1_ESPNOW_SCENE_H_ */
