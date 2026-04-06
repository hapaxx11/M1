/* See COPYING.txt for license details. */

/**
 * @file   m1_nfc_scene.c
 * @brief  NFC Scene Manager — scene-based menu with blocking delegates.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_nfc.h"
#include "m1_field_detect.h"
#include "m1_lib.h"
#include "m1_tasks.h"

/* Scene IDs */
enum {
    NfcSceneMenu = 0,
    NfcSceneRead,
    NfcSceneDetectReader,
    NfcSceneSaved,
    NfcSceneExtraActions,
    NfcSceneAddManually,
    NfcSceneTools,
    NfcSceneFieldDetect,
    NfcSceneCount
};

/*--- Blocking delegates ---------------------------------------------------*/

static void read_on_enter(M1SceneApp *app)
{
    (void)app;
    nfc_read();
    app->running = true;
    m1_scene_pop(app);
}

static void detect_reader_on_enter(M1SceneApp *app)
{
    (void)app;
    nfc_detect_reader();
    app->running = true;
    m1_scene_pop(app);
}

static void saved_on_enter(M1SceneApp *app)
{
    (void)app;
    nfc_saved();
    app->running = true;
    m1_scene_pop(app);
}

static void extra_actions_on_enter(M1SceneApp *app)
{
    (void)app;
    nfc_extra_actions();
    app->running = true;
    m1_scene_pop(app);
}

static void add_manually_on_enter(M1SceneApp *app)
{
    (void)app;
    nfc_add_manually();
    app->running = true;
    m1_scene_pop(app);
}

static void tools_on_enter(M1SceneApp *app)
{
    (void)app;
    nfc_tools();
    app->running = true;
    m1_scene_pop(app);
}

static void field_detect_on_enter(M1SceneApp *app)
{
    (void)app;
    nfc_rfid_detect_run();
    app->running = true;
    m1_scene_pop(app);
}

/*--- Handler tables -------------------------------------------------------*/

static const M1SceneHandlers read_handlers          = { .on_enter = read_on_enter          };
static const M1SceneHandlers detect_reader_handlers = { .on_enter = detect_reader_on_enter };
static const M1SceneHandlers saved_handlers         = { .on_enter = saved_on_enter         };
static const M1SceneHandlers extra_actions_handlers = { .on_enter = extra_actions_on_enter };
static const M1SceneHandlers add_manually_handlers  = { .on_enter = add_manually_on_enter  };
static const M1SceneHandlers tools_handlers         = { .on_enter = tools_on_enter         };
static const M1SceneHandlers field_detect_handlers  = { .on_enter = field_detect_on_enter  };

/*--- Menu scene -----------------------------------------------------------*/

#define MENU_ITEM_COUNT  7
#define MENU_VISIBLE     6

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Read",
    "Detect Reader",
    "Saved",
    "Extra Actions",
    "Add Manually",
    "Tools",
    "Field Detect",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    NfcSceneRead,
    NfcSceneDetectReader,
    NfcSceneSaved,
    NfcSceneExtraActions,
    NfcSceneAddManually,
    NfcSceneTools,
    NfcSceneFieldDetect,
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
    m1_scene_draw_menu("NFC", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, MENU_VISIBLE);
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/*--- Scene registry -------------------------------------------------------*/

static const M1SceneHandlers *const scene_registry[NfcSceneCount] = {
    [NfcSceneMenu]         = &menu_handlers,
    [NfcSceneRead]         = &read_handlers,
    [NfcSceneDetectReader] = &detect_reader_handlers,
    [NfcSceneSaved]        = &saved_handlers,
    [NfcSceneExtraActions] = &extra_actions_handlers,
    [NfcSceneAddManually]  = &add_manually_handlers,
    [NfcSceneTools]        = &tools_handlers,
    [NfcSceneFieldDetect]  = &field_detect_handlers,
};

/*--- Entry point ----------------------------------------------------------*/

void nfc_scene_entry(void)
{
    m1_scene_run(scene_registry, NfcSceneCount, menu_nfc_init, menu_nfc_deinit);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
