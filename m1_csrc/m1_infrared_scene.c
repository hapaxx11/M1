/* See COPYING.txt for license details. */

/**
 * @file   m1_infrared_scene.c
 * @brief  Infrared Scene Manager — scene-based menu with blocking delegates.
 */

#include <stdint.h>
#include <stdbool.h>
#include "m1_scene.h"
#include "m1_infrared.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/* Scene IDs */
enum {
    InfraredSceneMenu = 0,
    InfraredSceneUniversal,
    InfraredSceneLearn,
    InfraredSceneReplay,
    InfraredSceneCount
};

/*--- Blocking delegates ---------------------------------------------------*/

static void universal_on_enter(M1SceneApp *app)
{
    (void)app;
    infrared_universal_remotes();
    app->running = true;
    m1_scene_pop(app);
}

static void learn_on_enter(M1SceneApp *app)
{
    (void)app;
    infrared_learn_new_remote();
    app->running = true;
    m1_scene_pop(app);
}

static void replay_on_enter(M1SceneApp *app)
{
    (void)app;
    infrared_saved_remotes();
    app->running = true;
    m1_scene_pop(app);
}

/*--- Handler tables -------------------------------------------------------*/

static const M1SceneHandlers universal_handlers = { .on_enter = universal_on_enter };
static const M1SceneHandlers learn_handlers     = { .on_enter = learn_on_enter     };
static const M1SceneHandlers replay_handlers    = { .on_enter = replay_on_enter    };

/*--- Menu scene -----------------------------------------------------------*/

#define MENU_ITEM_COUNT  3
#define MENU_VISIBLE     3

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Universal Remotes",
    "Learn",
    "Replay",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    InfraredSceneUniversal,
    InfraredSceneLearn,
    InfraredSceneReplay,
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
                               MENU_ITEM_COUNT, MENU_VISIBLE, menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("Infrared", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, MENU_VISIBLE);
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/*--- Scene registry -------------------------------------------------------*/

static const M1SceneHandlers *const scene_registry[InfraredSceneCount] = {
    [InfraredSceneMenu]      = &menu_handlers,
    [InfraredSceneUniversal] = &universal_handlers,
    [InfraredSceneLearn]     = &learn_handlers,
    [InfraredSceneReplay]    = &replay_handlers,
};

/*--- Entry point ----------------------------------------------------------*/

void infrared_scene_entry(void)
{
    m1_scene_run(scene_registry, InfraredSceneCount, menu_infrared_init, NULL);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
