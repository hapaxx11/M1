/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene.c
 * @brief  ESP-NOW Peer Link Scene Manager — registry and entry point.
 *
 * Scene implementations:
 *   m1_espnow_scene_main.c     — top-level menu
 *   m1_espnow_scene_scan.c     — scan list + pairing
 *   m1_espnow_scene_messages.c — short text messages
 *   m1_espnow_scene_send.c     — capture sender picker/progress
 *   m1_espnow_scene_transfer.c — file transfer progress
 *   m1_espnow_scene_trigger.c  — remote trigger request/consent
 *   m1_espnow_scene_tictactoe.c — Tic-Tac-Toe game
 */

#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_espnow_scene.h"
#include "m1_espnow_scene_ctx.h"
#include "m1_scene.h"
#include "m1_esp32_hal.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* ESP32 init/deinit for ESP-NOW module                                     */
/*==========================================================================*/

static void espnow_hw_init(void)
{
    m1_espnow_scene_ctx_reset();
    m1_esp32_ensure_init();
}

static void espnow_hw_deinit(void)
{
    m1_esp32_deinit();
}

/*==========================================================================*/
/* Scene registry                                                           */
/*==========================================================================*/

static const M1SceneHandlers *const scene_registry[EspnowSceneCount] = {
    [EspnowSceneMenu]       = &espnow_scene_menu_handlers,
    [EspnowSceneScan]       = &espnow_scene_scan_handlers,
    [EspnowScenePair]       = &espnow_scene_pair_handlers,
    [EspnowSceneTransfer]   = &espnow_scene_transfer_handlers,
    [EspnowSceneSendCapture]= &espnow_scene_send_capture_handlers,
    [EspnowSceneSendProgress]= &espnow_scene_send_progress_handlers,
    [EspnowSceneMessages]   = &espnow_scene_messages_handlers,
    [EspnowSceneTriggerMenu]= &espnow_scene_trigger_menu_handlers,
    [EspnowSceneTriggerRequest]= &espnow_scene_trigger_request_handlers,
    [EspnowSceneTriggerStatus]= &espnow_scene_trigger_status_handlers,
    [EspnowSceneTriggerListen]= &espnow_scene_trigger_listen_handlers,
    [EspnowSceneTicTacToe]  = &espnow_scene_tictactoe_handlers,
};

/*==========================================================================*/
/* Entry point                                                              */
/*==========================================================================*/

void espnow_scene_entry(void)
{
    m1_scene_run(scene_registry, EspnowSceneCount,
                 espnow_hw_init, espnow_hw_deinit);
    /* Do NOT post Q_EVENT_MENU_EXIT here — this function is called as a
     * nested blocking delegate from the WiFi scene, which handles the
     * exit event when the WiFi module itself exits. */
}
