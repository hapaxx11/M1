/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_main.c
 * @brief  ESP-NOW Peer Link top-level menu scene.
 *
 * Provides the 3-item menu: Scan Peers, Send File, Tic-Tac-Toe.
 * Gates entry on M1_ESP32_CAP_ESPNOW via DELEGATE_FEATURE pattern.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_espnow_scene.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_caps.h"
#include "esp32_feature_map.h"
#include "m1_tasks.h"

/*==========================================================================*/
/* Top-level menu                                                           */
/*==========================================================================*/

#define MENU_ITEM_COUNT  3

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Scan Peers",
    "Send File",
    "Tic-Tac-Toe",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    EspnowSceneScan,
    EspnowSceneTransfer,
    EspnowSceneTicTacToe,
};

static subghz_submenu_model_t s_menu_model;

static void menu_on_enter(M1SceneApp *app)
{
    (void)app;
    /* Gate the entire module on ESP-NOW capability */
    if (!m1_esp32_require_cap(esp32_feature_required_caps(ESP32_FEATURE_ESPNOW),
                              esp32_feature_label(ESP32_FEATURE_ESPNOW))) {
        app->running = false;
        return;
    }
    if (s_menu_model.item_count == 0)
        subghz_submenu_model_init(&s_menu_model, MENU_ITEM_COUNT,
                                  M1_MENU_VIS(MENU_ITEM_COUNT));
    app->need_redraw = true;
}

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_submenu_event(app, event, &s_menu_model, menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_menu_model, "ESP-NOW Peer", menu_labels);
}

const M1SceneHandlers espnow_scene_menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};
