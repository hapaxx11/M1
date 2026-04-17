/* See COPYING.txt for license details. */

/**
 * @file   m1_esl_scene.c
 * @brief  ESL Tag Tinker — scene-based UI for Pricer ESL tag interaction.
 *
 * Implements features inspired by TagTinker (github.com/i12bp8/TagTinker):
 *   - Broadcast page flip  (flip all listening tags to a chosen display page)
 *   - Broadcast debug      (trigger all tags' diagnostic screen)
 *   - Targeted ping        (wake a specific tag identified by its barcode)
 *
 * Entry point: esl_scene_entry()
 * Called from the Infrared scene menu.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "stm32h5xx_hal.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "m1_scene.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"
#include "m1_system.h"
#include "m1_lib.h"
#include "m1_virtual_kb.h"

#include "m1_esl_proto.h"
#include "m1_esl_ir.h"

/* --------------------------------------------------------------------------- */
/* Constants                                                                   */
/* --------------------------------------------------------------------------- */

/** Default number of times each frame is re-transmitted for reliability. */
#define ESL_DEFAULT_REPEATS      9U
/** Millisecond gap between frame repetitions (allows OS scheduling). */
#define ESL_INTER_REPEAT_MS      0U
/** Milliseconds to show the "Done" confirmation screen. */
#define ESL_DONE_DISPLAY_MS   1200U
/** Number of named broadcast commands (pages 0-7 + debug). */
#define ESL_BROADCAST_ITEM_COUNT  9U

/* --------------------------------------------------------------------------- */
/* Shared application state                                                    */
/* --------------------------------------------------------------------------- */

typedef struct {
    M1SceneApp  scene;              /**< Must be first — cast-compatible.     */
    char        barcode[18];        /**< 17-digit barcode + NUL.              */
    uint8_t     plid[4];            /**< Decoded PLID of current target.      */
    bool        barcode_valid;      /**< True when plid[] is up to date.      */
    uint8_t     broadcast_page;     /**< Selected page for broadcast (0–7).  */
} EslApp;

/* Single static instance — blocking delegates run in the menu task. */
static EslApp esl_app;

/* --------------------------------------------------------------------------- */
/* Scene IDs                                                                   */
/* --------------------------------------------------------------------------- */

enum {
    EslSceneMenu = 0,
    EslSceneBroadcast,
    EslSceneTarget,
    EslSceneCount
};

/* --------------------------------------------------------------------------- */
/* Display helpers                                                             */
/* --------------------------------------------------------------------------- */

/**
 * Draw a two-line centred status screen (title + body) and update the display.
 * Used for "Sending…" and confirmation screens.
 */
static void esl_draw_status(const char *title, const char *body)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 4, 11, title);
    u8g2_DrawHLine(&m1_u8g2, 0, 13, 128);
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 4, 30, body);
    m1_u8g2_nextpage();
}

/**
 * Show a scrollable list of items (title + labels).  sel is the currently
 * highlighted row; scroll is the top-of-viewport index.
 */
static void esl_draw_list(const char *title,
                          const char *const *labels,
                          uint8_t count,
                          uint8_t sel,
                          uint8_t scroll)
{
    uint8_t visible = M1_MENU_VIS(count);
    m1_scene_draw_menu(title, labels, count, sel, scroll, visible);
}

/* --------------------------------------------------------------------------- */
/* IR transmit wrapper: init → send → deinit → show result                    */
/* --------------------------------------------------------------------------- */

static void esl_transmit_and_confirm(const uint8_t *frame, size_t frame_len,
                                     const char *action_label)
{
    esl_draw_status("ESL Tags", "Sending\xE2\x80\xA6");  /* "Sending..." */

    m1_esl_ir_init();
    m1_esl_ir_transmit_pp4(frame, frame_len,
                            ESL_DEFAULT_REPEATS, ESL_INTER_REPEAT_MS);
    m1_esl_ir_deinit();

    esl_draw_status("ESL Tags", action_label);
    vTaskDelay(pdMS_TO_TICKS(ESL_DONE_DISPLAY_MS));
}

/* --------------------------------------------------------------------------- */
/* Broadcast blocking delegate                                                 */
/* --------------------------------------------------------------------------- */

static const char *const broadcast_labels[ESL_BROADCAST_ITEM_COUNT] = {
    "Flip to Page 0",
    "Flip to Page 1",
    "Flip to Page 2",
    "Flip to Page 3",
    "Flip to Page 4",
    "Flip to Page 5",
    "Flip to Page 6",
    "Flip to Page 7",
    "Debug Screen",
};

static void esl_run_broadcast(void)
{
    uint8_t sel    = 0U;
    uint8_t scroll = 0U;
    uint8_t visible = M1_MENU_VIS(ESL_BROADCAST_ITEM_COUNT);

    S_M1_Main_Q_t q_item;
    S_M1_Buttons_Status bs;

    /* Draw initial menu. */
    esl_draw_list("Broadcast", broadcast_labels, ESL_BROADCAST_ITEM_COUNT,
                  sel, scroll);

    while(1) {
        if(xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY) != pdTRUE)
            continue;

        if(q_item.q_evt_type != Q_EVENT_KEYPAD) continue;
        if(xQueueReceive(button_events_q_hdl, &bs, 0) != pdTRUE) continue;

        if(bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK) {
            break;
        } else if(bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK) {
            if(sel > 0U) { sel--; if(sel < scroll) scroll = sel; }
            esl_draw_list("Broadcast", broadcast_labels,
                          ESL_BROADCAST_ITEM_COUNT, sel, scroll);
        } else if(bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK) {
            if(sel < ESL_BROADCAST_ITEM_COUNT - 1U) {
                sel++;
                if(sel >= scroll + visible) scroll = (uint8_t)(sel - visible + 1U);
            }
            esl_draw_list("Broadcast", broadcast_labels,
                          ESL_BROADCAST_ITEM_COUNT, sel, scroll);
        } else if(bs.event[BUTTON_OK_KP_ID]    == BUTTON_EVENT_CLICK ||
                  bs.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK) {

            uint8_t frame[M1_ESL_MAX_FRAME_SIZE];
            size_t  frame_len;
            char    done_msg[32];

            if(sel < 8U) {
                /* Page flip 0-7 */
                frame_len = m1_esl_build_broadcast_page_frame(
                    frame, sel, false, 120U);
                snprintf(done_msg, sizeof(done_msg), "Page %u sent", (unsigned)sel);
            } else {
                /* Debug screen */
                frame_len = m1_esl_build_broadcast_debug_frame(frame);
                snprintf(done_msg, sizeof(done_msg), "Debug sent");
            }

            if(frame_len > 0U) {
                esl_transmit_and_confirm(frame, frame_len, done_msg);
            }

            /* Redraw menu so user can send again or navigate. */
            esl_draw_list("Broadcast", broadcast_labels,
                          ESL_BROADCAST_ITEM_COUNT, sel, scroll);
        }
    }

    xQueueReset(main_q_hdl);
}

/* --------------------------------------------------------------------------- */
/* Target ESL blocking delegate                                                */
/* --------------------------------------------------------------------------- */

static const char *const target_action_labels[] = {
    "Ping (wake)",
    "Page 0",
    "Page 1",
    "Page 2",
    "Page 3",
    "Page 4",
    "Page 5",
    "Page 6",
    "Page 7",
    "Debug Screen",
};
#define TARGET_ACTION_COUNT  10U

static void esl_run_target(void)
{
    /* Step 1: ask for barcode via VKB. */
    char barcode[18];
    memset(barcode, 0, sizeof(barcode));

    /* Pre-fill with last valid barcode for convenience. */
    if(esl_app.barcode_valid)
        memcpy(barcode, esl_app.barcode, sizeof(barcode));

    uint8_t entered = m1_vkb_get_text(
        "ESL barcode (17 digits)", esl_app.barcode, barcode, sizeof(barcode));

    if(entered == 0U) {
        /* User cancelled. */
        xQueueReset(main_q_hdl);
        return;
    }

    /* Validate and decode barcode. */
    uint8_t plid[4];
    if(!m1_esl_barcode_to_plid(barcode, plid)) {
        esl_draw_status("ESL Tags", "Invalid barcode");
        vTaskDelay(pdMS_TO_TICKS(1500U));
        xQueueReset(main_q_hdl);
        return;
    }

    /* Store for next time. */
    memcpy(esl_app.barcode, barcode, sizeof(barcode));
    memcpy(esl_app.plid, plid, 4U);
    esl_app.barcode_valid = true;

    /* Step 2: action menu for this target. */
    uint8_t sel    = 0U;
    uint8_t scroll = 0U;
    uint8_t visible = M1_MENU_VIS(TARGET_ACTION_COUNT);

    S_M1_Main_Q_t q_item;
    S_M1_Buttons_Status bs;

    esl_draw_list("Target ESL", target_action_labels, TARGET_ACTION_COUNT,
                  sel, scroll);

    while(1) {
        if(xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY) != pdTRUE)
            continue;

        if(q_item.q_evt_type != Q_EVENT_KEYPAD) continue;
        if(xQueueReceive(button_events_q_hdl, &bs, 0) != pdTRUE) continue;

        if(bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK) {
            break;
        } else if(bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK) {
            if(sel > 0U) { sel--; if(sel < scroll) scroll = sel; }
            esl_draw_list("Target ESL", target_action_labels,
                          TARGET_ACTION_COUNT, sel, scroll);
        } else if(bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK) {
            if(sel < TARGET_ACTION_COUNT - 1U) {
                sel++;
                if(sel >= scroll + visible) scroll = (uint8_t)(sel - visible + 1U);
            }
            esl_draw_list("Target ESL", target_action_labels,
                          TARGET_ACTION_COUNT, sel, scroll);
        } else if(bs.event[BUTTON_OK_KP_ID]    == BUTTON_EVENT_CLICK ||
                  bs.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK) {

            uint8_t frame[M1_ESL_MAX_FRAME_SIZE];
            size_t  frame_len = 0U;
            char    done_msg[32];

            if(sel == 0U) {
                /* Ping */
                frame_len = m1_esl_build_ping_frame(frame, plid);
                snprintf(done_msg, sizeof(done_msg), "Ping sent");
            } else if(sel >= 1U && sel <= 8U) {
                /* Page flip 0-7: ping first, then broadcast page */
                uint8_t page = (uint8_t)(sel - 1U);
                frame_len = m1_esl_build_broadcast_page_frame(
                    frame, page, false, 120U);
                snprintf(done_msg, sizeof(done_msg), "Page %u sent", (unsigned)page);
            } else {
                /* Debug screen (broadcast) */
                frame_len = m1_esl_build_broadcast_debug_frame(frame);
                snprintf(done_msg, sizeof(done_msg), "Debug sent");
            }

            if(frame_len > 0U) {
                esl_transmit_and_confirm(frame, frame_len, done_msg);
            }

            /* Redraw action menu. */
            esl_draw_list("Target ESL", target_action_labels,
                          TARGET_ACTION_COUNT, sel, scroll);
        }
    }

    xQueueReset(main_q_hdl);
}

/* --------------------------------------------------------------------------- */
/* Scene handler implementations                                               */
/* --------------------------------------------------------------------------- */

static void broadcast_on_enter(M1SceneApp *app)
{
    esl_run_broadcast();
    app->running = true;
    m1_scene_pop(app);
}

static void target_on_enter(M1SceneApp *app)
{
    esl_run_target();
    app->running = true;
    m1_scene_pop(app);
}

static const M1SceneHandlers broadcast_handlers = { .on_enter = broadcast_on_enter };
static const M1SceneHandlers target_handlers    = { .on_enter = target_on_enter    };

/* --------------------------------------------------------------------------- */
/* Main menu scene                                                             */
/* --------------------------------------------------------------------------- */

#define ESL_MENU_ITEM_COUNT  2U

static const char *const esl_menu_labels[ESL_MENU_ITEM_COUNT] = {
    "Broadcast",
    "Target ESL",
};

static const uint8_t esl_menu_targets[ESL_MENU_ITEM_COUNT] = {
    EslSceneBroadcast,
    EslSceneTarget,
};

static uint8_t esl_menu_sel    = 0U;
static uint8_t esl_menu_scroll = 0U;

static void esl_menu_on_enter(M1SceneApp *app)
{
    (void)app;
    app->need_redraw = true;
}

static bool esl_menu_on_event(M1SceneApp *app, M1SceneEvent event)
{
    return m1_scene_menu_event(app, event,
                               &esl_menu_sel, &esl_menu_scroll,
                               ESL_MENU_ITEM_COUNT,
                               M1_MENU_VIS(ESL_MENU_ITEM_COUNT),
                               esl_menu_targets);
}

static void esl_menu_on_exit(M1SceneApp *app) { (void)app; }

static void esl_menu_draw(M1SceneApp *app)
{
    (void)app;
    m1_scene_draw_menu("ESL Tags", esl_menu_labels, ESL_MENU_ITEM_COUNT,
                       esl_menu_sel, esl_menu_scroll,
                       M1_MENU_VIS(ESL_MENU_ITEM_COUNT));
}

static const M1SceneHandlers esl_menu_handlers = {
    .on_enter = esl_menu_on_enter,
    .on_event = esl_menu_on_event,
    .on_exit  = esl_menu_on_exit,
    .draw     = esl_menu_draw,
};

/* --------------------------------------------------------------------------- */
/* Scene registry                                                              */
/* --------------------------------------------------------------------------- */

static const M1SceneHandlers *const esl_scene_registry[EslSceneCount] = {
    [EslSceneMenu]      = &esl_menu_handlers,
    [EslSceneBroadcast] = &broadcast_handlers,
    [EslSceneTarget]    = &target_handlers,
};

/* --------------------------------------------------------------------------- */
/* Entry point                                                                 */
/* --------------------------------------------------------------------------- */

void esl_scene_entry(void)
{
    memset(&esl_app, 0, sizeof(esl_app));
    m1_scene_run(esl_scene_registry, EslSceneCount, NULL, NULL);
    /* Do NOT post Q_EVENT_MENU_EXIT here — this function is called as a
     * nested blocking delegate from infrared_scene_entry(), which will
     * send the exit event when the infrared module itself exits. */
}
