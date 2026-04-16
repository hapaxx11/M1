/* See COPYING.txt for license details. */

/**
 * @file   m1_games_scene.c
 * @brief  Games Scene Manager — scene-based menu with blocking delegates.
 */

#include "m1_compile_cfg.h"

#ifdef M1_APP_GAMES_ENABLE

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_games.h"
#include "music_player.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/* Scene IDs */
enum {
    GamesSceneMenu = 0,
    GamesSceneSnake,
    GamesSceneTetris,
    GamesSceneTRex,
    GamesScenePong,
    GamesSceneDice,
    GamesSceneMusic,
    GamesSceneClock,
    GamesSceneCount
};

/*--- Blocking delegates ---------------------------------------------------*/

static void snake_on_enter(M1SceneApp *app)
{
    (void)app;
    game_snake_run();
    app->running = true;
    m1_scene_pop(app);
}

static void tetris_on_enter(M1SceneApp *app)
{
    (void)app;
    game_tetris_run();
    app->running = true;
    m1_scene_pop(app);
}

static void trex_on_enter(M1SceneApp *app)
{
    (void)app;
    game_trex_run();
    app->running = true;
    m1_scene_pop(app);
}

static void pong_on_enter(M1SceneApp *app)
{
    (void)app;
    game_pong_run();
    app->running = true;
    m1_scene_pop(app);
}

static void dice_on_enter(M1SceneApp *app)
{
    (void)app;
    game_dice_run();
    app->running = true;
    m1_scene_pop(app);
}

static void music_on_enter(M1SceneApp *app)
{
    (void)app;
    music_player_run();
    app->running = true;
    m1_scene_pop(app);
}

static void clock_on_enter(M1SceneApp *app)
{
    (void)app;
    app_clock_run();
    app->running = true;
    m1_scene_pop(app);
}

/*--- Handler tables -------------------------------------------------------*/

static const M1SceneHandlers snake_handlers  = { .on_enter = snake_on_enter  };
static const M1SceneHandlers tetris_handlers = { .on_enter = tetris_on_enter };
static const M1SceneHandlers trex_handlers   = { .on_enter = trex_on_enter   };
static const M1SceneHandlers pong_handlers   = { .on_enter = pong_on_enter   };
static const M1SceneHandlers dice_handlers   = { .on_enter = dice_on_enter   };
static const M1SceneHandlers music_handlers  = { .on_enter = music_on_enter  };
static const M1SceneHandlers clock_handlers  = { .on_enter = clock_on_enter  };

/*--- Menu scene -----------------------------------------------------------*/

#define MENU_ITEM_COUNT  7

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Snake",
    "Tetris",
    "T-Rex Runner",
    "Pong",
    "Dice Roll",
    "Music Player",
    "Clock",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    GamesSceneSnake,
    GamesSceneTetris,
    GamesSceneTRex,
    GamesScenePong,
    GamesSceneDice,
    GamesSceneMusic,
    GamesSceneClock,
};

static uint8_t menu_sel    = 0;
static uint8_t menu_scroll = 0;

static void menu_on_enter(M1SceneApp *app)
{
    (void)app;
    app->need_redraw = true;
}

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event, &menu_sel, &menu_scroll,
                               MENU_ITEM_COUNT, M1_MENU_VIS(MENU_ITEM_COUNT), menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Games", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, M1_MENU_VIS(MENU_ITEM_COUNT));
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/*--- Scene registry -------------------------------------------------------*/

static const M1SceneHandlers *const scene_registry[GamesSceneCount] = {
    [GamesSceneMenu]   = &menu_handlers,
    [GamesSceneSnake]  = &snake_handlers,
    [GamesSceneTetris] = &tetris_handlers,
    [GamesSceneTRex]   = &trex_handlers,
    [GamesScenePong]   = &pong_handlers,
    [GamesSceneDice]   = &dice_handlers,
    [GamesSceneMusic]  = &music_handlers,
    [GamesSceneClock]  = &clock_handlers,
};

/*--- Entry point ----------------------------------------------------------*/

void games_scene_entry(void)
{
    m1_scene_run(scene_registry, GamesSceneCount, NULL, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}

#endif /* M1_APP_GAMES_ENABLE */
