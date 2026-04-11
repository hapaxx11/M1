/* See COPYING.txt for license details. */

/**
 * @file   m1_rfid_scene.c
 * @brief  125 kHz RFID Scene Manager — scene-based menu with blocking delegates.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_rfid.h"
#include "m1_lib.h"
#include "m1_tasks.h"

#ifdef M1_APP_FILE_IMPORT_ENABLE
#include "m1_flipper_integration.h"
#endif

/* Scene IDs */
enum {
    RfidSceneMenu = 0,
    RfidSceneRead,
    RfidSceneSaved,
    RfidSceneAdd,
#ifdef M1_APP_FILE_IMPORT_ENABLE
    RfidSceneImport,
#endif
    RfidSceneUtilities,
    RfidSceneCount
};

/*--- Blocking delegates ---------------------------------------------------*/

static void read_on_enter(M1SceneApp *app)
{
    (void)app;
    rfid_125khz_read();
    app->running = true;
    m1_scene_pop(app);
}

static void saved_on_enter(M1SceneApp *app)
{
    (void)app;
    rfid_125khz_saved();
    app->running = true;
    m1_scene_pop(app);
}

static void add_on_enter(M1SceneApp *app)
{
    (void)app;
    rfid_125khz_add_manually();
    app->running = true;
    m1_scene_pop(app);
}

#ifdef M1_APP_FILE_IMPORT_ENABLE
static void import_on_enter(M1SceneApp *app)
{
    (void)app;
    rfid_import_flipper();
    app->running = true;
    m1_scene_pop(app);
}
#endif

static void utilities_on_enter(M1SceneApp *app)
{
    (void)app;
    rfid_125khz_utilities();
    app->running = true;
    m1_scene_pop(app);
}

/*--- Handler tables -------------------------------------------------------*/

static const M1SceneHandlers read_handlers      = { .on_enter = read_on_enter      };
static const M1SceneHandlers saved_handlers     = { .on_enter = saved_on_enter     };
static const M1SceneHandlers add_handlers       = { .on_enter = add_on_enter       };
#ifdef M1_APP_FILE_IMPORT_ENABLE
static const M1SceneHandlers import_handlers    = { .on_enter = import_on_enter    };
#endif
static const M1SceneHandlers utilities_handlers = { .on_enter = utilities_on_enter };

/*--- Menu scene -----------------------------------------------------------*/

#ifdef M1_APP_FILE_IMPORT_ENABLE
#define MENU_ITEM_COUNT  5
#else
#define MENU_ITEM_COUNT  4
#endif

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "Read",
    "Saved",
    "Add",
#ifdef M1_APP_FILE_IMPORT_ENABLE
    "Import .rfid",
#endif
    "Utilities",
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    RfidSceneRead,
    RfidSceneSaved,
    RfidSceneAdd,
#ifdef M1_APP_FILE_IMPORT_ENABLE
    RfidSceneImport,
#endif
    RfidSceneUtilities,
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
    m1_scene_draw_menu("125 kHz RFID", menu_labels, MENU_ITEM_COUNT,
                       menu_sel, menu_scroll, M1_MENU_VIS(MENU_ITEM_COUNT));
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/*--- Scene registry -------------------------------------------------------*/

static const M1SceneHandlers *const scene_registry[RfidSceneCount] = {
    [RfidSceneMenu]      = &menu_handlers,
    [RfidSceneRead]      = &read_handlers,
    [RfidSceneSaved]     = &saved_handlers,
    [RfidSceneAdd]       = &add_handlers,
#ifdef M1_APP_FILE_IMPORT_ENABLE
    [RfidSceneImport]    = &import_handlers,
#endif
    [RfidSceneUtilities] = &utilities_handlers,
};

/*--- Entry point ----------------------------------------------------------*/

void rfid_scene_entry(void)
{
    m1_scene_run(scene_registry, RfidSceneCount,
                 menu_125khz_rfid_init, menu_125khz_rfid_deinit);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
