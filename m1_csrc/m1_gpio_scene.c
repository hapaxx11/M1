/* See COPYING.txt for license details. */

/**
 * @file   m1_gpio_scene.c
 * @brief  GPIO Scene Manager — scene-based menu with blocking delegates.
 *
 * Submenu model: uses `subghz_submenu_model_t` + `m1_submenu_draw/event` for
 * consistent font-aware layout and automatic visible-count sync.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_scene.h"
#include "m1_submenu.h"
#include "m1_gpio.h"
#include "signal_gen.h"
#include "m1_lib.h"
#include "m1_tasks.h"

#ifdef M1_APP_CAN_ENABLE
#include "m1_can.h"
#endif

/* Scene IDs */
enum {
    GpioSceneMenu = 0,
    GpioSceneControl,
    GpioScene3v3,
    GpioScene5v,
    GpioSceneUart,
    GpioSceneSignalGen,
#ifdef M1_APP_CAN_ENABLE
    GpioSceneCanMenu,
    GpioSceneCanSniffer,
    GpioSceneCanSend,
    GpioSceneCanSaved,
#endif
    GpioSceneCount
};

/*--- Blocking delegates ---------------------------------------------------*/

static void control_on_enter(M1SceneApp *app)
{
    (void)app;
    gpio_manual_control();
    app->running = true;
    m1_scene_pop(app);
}

static void v3_3_on_enter(M1SceneApp *app)
{
    (void)app;
    gpio_3_3v_on_gpio();
    app->running = true;
    m1_scene_pop(app);
}

static void v5_on_enter(M1SceneApp *app)
{
    (void)app;
    gpio_5v_on_gpio();
    app->running = true;
    m1_scene_pop(app);
}

static void uart_on_enter(M1SceneApp *app)
{
    (void)app;
    gpio_usb_uart_bridge();
    app->running = true;
    m1_scene_pop(app);
}

static void siggen_on_enter(M1SceneApp *app)
{
    (void)app;
    signal_gen_run();
    app->running = true;
    m1_scene_pop(app);
}

#ifdef M1_APP_CAN_ENABLE
static void can_sniffer_on_enter(M1SceneApp *app)
{
    (void)app;
    can_sniffer();
    app->running = true;
    m1_scene_pop(app);
}

static void can_send_on_enter(M1SceneApp *app)
{
    (void)app;
    can_send();
    app->running = true;
    m1_scene_pop(app);
}

static void can_saved_on_enter(M1SceneApp *app)
{
    (void)app;
    can_saved();
    app->running = true;
    m1_scene_pop(app);
}
#endif

/*--- Handler tables -------------------------------------------------------*/

static const M1SceneHandlers control_handlers = { .on_enter = control_on_enter };
static const M1SceneHandlers v3_3_handlers    = { .on_enter = v3_3_on_enter    };
static const M1SceneHandlers v5_handlers      = { .on_enter = v5_on_enter      };
static const M1SceneHandlers uart_handlers    = { .on_enter = uart_on_enter    };
static const M1SceneHandlers siggen_handlers  = { .on_enter = siggen_on_enter  };

#ifdef M1_APP_CAN_ENABLE
static const M1SceneHandlers can_sniffer_handlers = { .on_enter = can_sniffer_on_enter };
static const M1SceneHandlers can_send_handlers    = { .on_enter = can_send_on_enter    };
static const M1SceneHandlers can_saved_handlers   = { .on_enter = can_saved_on_enter   };
#endif

/*--- Main menu scene ------------------------------------------------------*/

#ifdef M1_APP_CAN_ENABLE
#define MENU_ITEM_COUNT  6
#else
#define MENU_ITEM_COUNT  5
#endif

static const char *const menu_labels[MENU_ITEM_COUNT] = {
    "GPIO Control",
    "3.3V power",
    "5V power",
    "USB-UART bridge",
    "Signal Gen",
#ifdef M1_APP_CAN_ENABLE
    "CAN Bus",
#endif
};

static const uint8_t menu_targets[MENU_ITEM_COUNT] = {
    GpioSceneControl,
    GpioScene3v3,
    GpioScene5v,
    GpioSceneUart,
    GpioSceneSignalGen,
#ifdef M1_APP_CAN_ENABLE
    GpioSceneCanMenu,
#endif
};

static subghz_submenu_model_t s_gpio_menu_model;

static void menu_on_enter(M1SceneApp *app)
{
    (void)app;
    if (s_gpio_menu_model.item_count == 0)
        subghz_submenu_model_init(&s_gpio_menu_model, MENU_ITEM_COUNT,
                                  M1_MENU_VIS(MENU_ITEM_COUNT));
    app->need_redraw = true;
}

static bool menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_submenu_event(app, event, &s_gpio_menu_model, menu_targets);
}

static void menu_on_exit(M1SceneApp *app) { (void)app; }

static void menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_gpio_menu_model, "GPIO", menu_labels);
}

static const M1SceneHandlers menu_handlers = {
    .on_enter = menu_on_enter,
    .on_event = menu_on_event,
    .on_exit  = menu_on_exit,
    .draw     = menu_draw,
};

/*--- CAN Bus sub-menu scene -----------------------------------------------*/

#ifdef M1_APP_CAN_ENABLE

#define CAN_MENU_ITEM_COUNT  3

static const char *const can_menu_labels[CAN_MENU_ITEM_COUNT] = {
    "Sniffer",
    "Send Frame",
    "Saved",
};

static const uint8_t can_menu_targets[CAN_MENU_ITEM_COUNT] = {
    GpioSceneCanSniffer,
    GpioSceneCanSend,
    GpioSceneCanSaved,
};

static subghz_submenu_model_t s_can_menu_model;

static void can_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    menu_can_init();
    if (s_can_menu_model.item_count == 0)
        subghz_submenu_model_init(&s_can_menu_model, CAN_MENU_ITEM_COUNT,
                                  M1_MENU_VIS(CAN_MENU_ITEM_COUNT));
    app->need_redraw = true;
}

static bool can_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_submenu_event(app, event, &s_can_menu_model, can_menu_targets);
}

static void can_menu_on_exit(M1SceneApp *app)
{
    (void)app;
    menu_can_deinit();
}

static void can_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_submenu_draw(&s_can_menu_model, "CAN Bus", can_menu_labels);
}

static const M1SceneHandlers can_menu_handlers = {
    .on_enter = can_menu_on_enter,
    .on_event = can_menu_on_event,
    .on_exit  = can_menu_on_exit,
    .draw     = can_menu_draw,
};

#endif /* M1_APP_CAN_ENABLE */

/*--- Scene registry -------------------------------------------------------*/

static const M1SceneHandlers *const scene_registry[GpioSceneCount] = {
    [GpioSceneMenu]      = &menu_handlers,
    [GpioSceneControl]   = &control_handlers,
    [GpioScene3v3]       = &v3_3_handlers,
    [GpioScene5v]        = &v5_handlers,
    [GpioSceneUart]      = &uart_handlers,
    [GpioSceneSignalGen] = &siggen_handlers,
#ifdef M1_APP_CAN_ENABLE
    [GpioSceneCanMenu]    = &can_menu_handlers,
    [GpioSceneCanSniffer] = &can_sniffer_handlers,
    [GpioSceneCanSend]    = &can_send_handlers,
    [GpioSceneCanSaved]   = &can_saved_handlers,
#endif
};

/*--- Entry point ----------------------------------------------------------*/

void gpio_scene_entry(void)
{
    m1_scene_run(scene_registry, GpioSceneCount, menu_gpio_init, menu_gpio_exit);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
