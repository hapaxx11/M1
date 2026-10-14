/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_proto_pirate_bruteforce.c
 * @brief  Proto Pirate — PSA TEA brute-force scene.
 *
 * Flow:
 *   1. on_enter opens the file browser (0:/SUBGHZ); the user picks a PSA
 *      capture in Flipper Key .sub or M1 native .sgh format carrying `Key:`
 *      and `Key_2:` (written by the Read scene for TEA-mode/0x36 captures, or
 *      exported by ProtoPirate).
 *   2. The two-phase TEA key search (Sub_Ghz/subghz_psa_bruteforce.c) runs on
 *      a low-priority worker task; the scene polls completion on its 100 ms
 *      tick and draws a live progress bar.  The full space is 2 × 2^24
 *      candidates — worst case a few minutes at 250 MHz; BACK cancels.
 *   3. On FOUND the recovered serial/button/counter are shown and OK saves a
 *      `<name>_recovered.sub` copy patched with Serial/Btn/Cnt/Type/CRC/Seed
 *      (flipper_subghz_save_psa_recovered).
 *
 * Async pattern: bounded one-shot worker + binary semaphore + cancel flag
 * (same pattern as the WiFi-scan RPC worker), polled from the scene tick —
 * no new main-queue event type is needed.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm32h5xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "m1_subghz_button_bar.h"
#include "m1_storage.h"
#include "subghz_saved_path.h"
#include "flipper_subghz.h"
#include "subghz_psa_bruteforce.h"

/*============================================================================*/
/* Scene state                                                                 */
/*============================================================================*/

typedef enum {
    PP_BF_STATE_RUNNING,
    PP_BF_STATE_FOUND,
    PP_BF_STATE_SAVED,
    PP_BF_STATE_NOT_FOUND,
    PP_BF_STATE_CANCELLED,
    PP_BF_STATE_INVALID,   /* picked file is not a PSA capture with Key_2 */
    PP_BF_STATE_ERROR,     /* worker/semaphore allocation failed */
} pp_bf_state_t;

/* Worker + scene shared context — heap-allocated for the scene's lifetime.
 * Budget: ~300 B context + 3 KB task stack/TCB from the 256 KB FreeRTOS pool
 * (see configTOTAL_HEAP_SIZE). */
typedef struct {
    subghz_bruteforce_job_t job;      /* generic lifecycle: progress/cancel */
    subghz_psa_bf_result_t  result;   /* recovered fields when FOUND */
    SemaphoreHandle_t       done;     /* given once by the worker at exit */
    uint64_t key1;
    uint32_t frequency;
    uint32_t te;
    uint32_t bit_count;
    uint16_t key2;
    char     preset[FLIPPER_SUBGHZ_PRESET_MAX_LEN];
    char     src_path[SUBGHZ_SAVED_FULL_PATH_MAX];
    pp_bf_state_t state;
} pp_bf_ctx_t;

static pp_bf_ctx_t *s_ctx;

/** Case-insensitive equality for ASCII field strings (the project avoids
 *  relying on libc strcasecmp). */
static bool pp_str_ieq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb)
            return false;
    }
    return *a == *b;
}

/*============================================================================*/
/* Worker task                                                                 */
/*============================================================================*/

#define PP_BF_TASK_STACK_WORDS  768u   /* 3 KB — covers the 256 B TEA schedule + call depth */
#define PP_BF_TASK_PRIORITY     (tskIDLE_PRIORITY + 1u)
#define PP_BF_TICK_MS           100u

static void psa_bf_worker(void *arg)
{
    pp_bf_ctx_t *ctx = (pp_bf_ctx_t *)arg;
    (void)subghz_psa_bf_run(&ctx->job, ctx->key1, ctx->key2, &ctx->result);
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}

/*============================================================================*/
/* Helpers                                                                     */
/*============================================================================*/

static void pp_bf_ctx_free(void)
{
    if (s_ctx == NULL)
        return;
    if (s_ctx->done != NULL)
        vSemaphoreDelete(s_ctx->done);
    free(s_ctx);
    s_ctx = NULL;
}

/** Cancel a running search and wait for the worker to finish before freeing
 *  its shared context or completion semaphore. */
static void pp_bf_stop_worker(void)
{
    if (s_ctx == NULL || s_ctx->done == NULL)
        return;
    if (s_ctx->state == PP_BF_STATE_RUNNING)
    {
        subghz_bruteforce_job_cancel(&s_ctx->job);
        while (xSemaphoreTake(s_ctx->done, portMAX_DELAY) != pdTRUE)
        {
        }
    }
}

/** Load and validate the picked file.  Returns true when the capture is a
 *  PSA key file carrying Key_2 and the context fields are populated. */
static bool pp_bf_load_capture(pp_bf_ctx_t *ctx)
{
    bool ok = false;
    flipper_subghz_signal_t *sig = malloc(sizeof(*sig));
    if (sig == NULL)
        return false;

    memset(sig, 0, sizeof(*sig));
    if (flipper_subghz_load(ctx->src_path, sig) &&
        sig->type == FLIPPER_SUBGHZ_TYPE_PARSED &&
        pp_str_ieq(sig->protocol, "PSA") &&
        flipper_subghz_psa_is_tea_candidate(
            sig->has_key_2, sig->has_protocol_type, sig->protocol_type))
    {
        ctx->key1      = sig->key;
        ctx->key2      = sig->key_2;
        ctx->frequency = sig->frequency;
        ctx->te        = sig->te;
        ctx->bit_count = sig->bit_count;
        strncpy(ctx->preset, sig->preset, sizeof(ctx->preset) - 1);
        ctx->preset[sizeof(ctx->preset) - 1] = '\0';
        ok = true;
    }

    free(sig);
    return ok;
}

static bool pp_bf_start_worker(pp_bf_ctx_t *ctx)
{
    subghz_bruteforce_job_init(&ctx->job);
    if (!subghz_bruteforce_job_begin(&ctx->job, PSA_BF_TOTAL))
        return false;

    ctx->done = xSemaphoreCreateBinary();
    if (ctx->done == NULL)
        return false;

    if (xTaskCreate(psa_bf_worker, "psa_bf", PP_BF_TASK_STACK_WORDS, ctx,
                    PP_BF_TASK_PRIORITY, NULL) != pdPASS)
    {
        vSemaphoreDelete(ctx->done);
        ctx->done = NULL;
        return false;
    }
    return true;
}

/** Save the recovered capture next to the source as "<stem>_recovered.sub". */
static bool pp_bf_save_recovered(pp_bf_ctx_t *ctx)
{
    char path[SUBGHZ_SAVED_FULL_PATH_MAX + 16];
    if (!subghz_saved_path_recovered_build(path, sizeof(path), ctx->src_path))
        return false;

    return flipper_subghz_save_psa_recovered(path, ctx->frequency, ctx->preset,
                                             ctx->bit_count, ctx->key1,
                                             ctx->key2, ctx->te, &ctx->result);
}

/*============================================================================*/
/* Scene callbacks                                                             */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    pp_bf_ctx_free();

    /* Pick the capture to attack. */
    S_M1_file_info *f_info = storage_browse("0:/SUBGHZ");
    if (f_info == NULL || !f_info->file_is_selected)
    {
        subghz_scene_pop(app);
        return;
    }

    s_ctx = calloc(1, sizeof(*s_ctx));
    if (s_ctx == NULL)
    {
        subghz_scene_pop(app);
        return;
    }

    if (!subghz_saved_path_build(s_ctx->src_path, sizeof(s_ctx->src_path),
                                 f_info->dir_name, f_info->file_name))
    {
        pp_bf_ctx_free();
        subghz_scene_pop(app);
        return;
    }

    if (!pp_bf_load_capture(s_ctx))
    {
        s_ctx->state = PP_BF_STATE_INVALID;
        app->need_redraw = true;
        return;
    }

    if (!pp_bf_start_worker(s_ctx))
    {
        s_ctx->state = PP_BF_STATE_ERROR;
        app->need_redraw = true;
        return;
    }

    s_ctx->state = PP_BF_STATE_RUNNING;
    subghz_scene_set_tick_period(app, PP_BF_TICK_MS);
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    if (s_ctx == NULL)
        return false;

    switch (event)
    {
        case SubGhzEventTick:
            if (s_ctx->state == PP_BF_STATE_RUNNING &&
                xSemaphoreTake(s_ctx->done, 0) == pdTRUE)
            {
                switch (s_ctx->job.status)
                {
                    case SUBGHZ_BRUTEFORCE_JOB_FOUND:
                        s_ctx->state = PP_BF_STATE_FOUND;
                        break;
                    case SUBGHZ_BRUTEFORCE_JOB_CANCELLED:
                        s_ctx->state = PP_BF_STATE_CANCELLED;
                        break;
                    default:
                        s_ctx->state = PP_BF_STATE_NOT_FOUND;
                        break;
                }
                subghz_scene_set_tick_period(app, 0);
            }
            app->need_redraw = true;
            return true;

        case SubGhzEventBack:
            if (s_ctx->state == PP_BF_STATE_RUNNING)
            {
                pp_bf_stop_worker();
                s_ctx->state = PP_BF_STATE_CANCELLED;
                subghz_scene_set_tick_period(app, 0);
                app->need_redraw = true;
                return true;
            }
            subghz_scene_pop(app);
            return true;

        case SubGhzEventOk:
            if (s_ctx->state == PP_BF_STATE_FOUND)
            {
                s_ctx->state = pp_bf_save_recovered(s_ctx)
                                   ? PP_BF_STATE_SAVED
                                   : PP_BF_STATE_ERROR;
                app->need_redraw = true;
                return true;
            }
            if (s_ctx->state != PP_BF_STATE_RUNNING)
            {
                subghz_scene_pop(app);
                return true;
            }
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    subghz_scene_set_tick_period(app, 0);
    pp_bf_stop_worker();
    pp_bf_ctx_free();
}

/*============================================================================*/
/* Draw                                                                        */
/*============================================================================*/

static void draw(SubGhzApp *app)
{
    (void)app;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "PSA Brute Force");

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    if (s_ctx == NULL)
    {
        m1_u8g2_nextpage();
        return;
    }

    char line[40];

    switch (s_ctx->state)
    {
        case PP_BF_STATE_RUNNING:
        {
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Searching TEA key...");

            /* Progress bar with a real fill. */
            const uint32_t cur = s_ctx->job.progress_current;
            const uint32_t total = s_ctx->job.progress_total;
            const uint16_t fill =
                (total > 0U) ? (uint16_t)(((uint64_t)cur * 112ULL) / total) : 0U;
            u8g2_DrawFrame(&m1_u8g2, 4, 30, 120, 10);
            if (fill > 0U)
                u8g2_DrawBox(&m1_u8g2, 6, 32, fill, 6);

            const uint32_t pct10 =
                (total > 0U) ? (uint32_t)(((uint64_t)cur * 1000ULL) / total) : 0U;
            snprintf(line, sizeof(line), "%lu.%u%%",
                     (unsigned long)(pct10 / 10U), (unsigned int)(pct10 % 10U));
            u8g2_DrawStr(&m1_u8g2, 2, 52, line);
            break;
        }

        case PP_BF_STATE_FOUND:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Key found!");
            snprintf(line, sizeof(line), "Ser: %06lX",
                     (unsigned long)s_ctx->result.serial);
            u8g2_DrawStr(&m1_u8g2, 2, 34, line);
            snprintf(line, sizeof(line), "Btn: %X  Cnt: %lX",
                     (unsigned int)s_ctx->result.button,
                     (unsigned long)s_ctx->result.counter);
            u8g2_DrawStr(&m1_u8g2, 2, 44, line);
            subghz_button_bar_draw(NULL, NULL, ok_circle_8x8, "Save",
                                   NULL, NULL);
            break;

        case PP_BF_STATE_SAVED:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Recovered key saved");
            u8g2_DrawStr(&m1_u8g2, 2, 36, "as _recovered.sub");
            break;

        case PP_BF_STATE_NOT_FOUND:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Key not found");
            u8g2_DrawStr(&m1_u8g2, 2, 36, "Full range searched");
            break;

        case PP_BF_STATE_CANCELLED:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Cancelled");
            break;

        case PP_BF_STATE_INVALID:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Not a PSA capture");
            u8g2_DrawStr(&m1_u8g2, 2, 36, "Need PSA Key_2 capture");
            break;

        case PP_BF_STATE_ERROR:
        default:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Error: no resources");
            break;
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_proto_pirate_bruteforce_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
