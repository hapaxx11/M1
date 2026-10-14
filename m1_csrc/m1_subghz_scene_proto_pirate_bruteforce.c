/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_proto_pirate_bruteforce.c
 * @brief  Proto Pirate — brute-force key recovery scene (PSA + Renault V1).
 *
 * Flow:
 *   1. on_enter opens the file browser (0:/SUBGHZ); the user picks a capture
 *      in Flipper Key .sub or M1 native .sgh format carrying `Key:` and
 *      `Key_2:` (written by the Read scene, or exported by ProtoPirate).
 *      Supported protocols:
 *        - PSA (TEA mode / type 0x36): two-phase TEA key search,
 *          2 × 2^24 candidates (Sub_Ghz/subghz_psa_bruteforce.c).
 *        - Renault V1: Hitag2 IV (seed) search, 2^18 candidates
 *          (Sub_Ghz/subghz_renault_v1_bruteforce.c).
 *   2. The search runs on a low-priority worker task; the scene polls
 *      completion on its 100 ms tick and draws a live progress bar.
 *      BACK cancels.
 *   3. On FOUND the recovered material is shown and OK saves a
 *      `<name>_recovered.sub` copy patched with the recovered fields.
 *      For Renault V1 the seed additionally unlocks TX: RIGHT synthesizes a
 *      fresh frame with an incremented counter (subghz_renault_v1_encoder.c),
 *      writes it as a temp .sgh and hands off to the Read Raw scene in Loaded
 *      state, where Send transmits it via the async TX path.
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
#include "subghz_renault_v1_bruteforce.h"
#include "subghz_renault_v1_encoder.h"
#include "ff.h"

/*============================================================================*/
/* Scene state                                                                 */
/*============================================================================*/

typedef enum {
    PP_BF_STATE_RUNNING,
    PP_BF_STATE_FOUND,
    PP_BF_STATE_SAVED,
    PP_BF_STATE_NOT_FOUND,
    PP_BF_STATE_CANCELLED,
    PP_BF_STATE_INVALID,   /* picked file is not a BF-able PSA/Renault capture */
    PP_BF_STATE_ERROR,
} pp_bf_state_t;

typedef enum {
    PP_BF_KIND_PSA,
    PP_BF_KIND_RENAULT_V1,
} pp_bf_kind_t;

/** Temp waveform file handed to the Read Raw scene for Renault V1 TX. */
#define PP_BF_TX_TMP_PATH  "0:/SUBGHZ/_pp_bf_tx_tmp.sgh"

/* Worker + scene shared context — heap-allocated for the scene's lifetime.
 * Budget: ~400 B context + 3 KB task stack/TCB from the 256 KB FreeRTOS pool
 * (see configTOTAL_HEAP_SIZE); the 10.4 KB TX upload buffer is allocated only
 * for the duration of one Emulate press. */
typedef struct {
    subghz_bruteforce_job_t job;      /* generic lifecycle: progress/cancel */
    SemaphoreHandle_t       done;     /* given once by the worker at exit */
    pp_bf_kind_t  kind;
    uint64_t key1;
    uint32_t key2;
    uint32_t frequency;
    uint32_t te;
    uint32_t bit_count;
    char     preset[FLIPPER_SUBGHZ_PRESET_MAX_LEN];
    char     src_path[SUBGHZ_SAVED_FULL_PATH_MAX];
    pp_bf_state_t state;
    /* PSA result */
    subghz_psa_bf_result_t psa;
    /* Renault V1 result + TX state */
    uint8_t  rv1_iv[4];
    uint32_t rv1_seed;
    uint32_t rv1_serial;
    uint32_t rv1_hop;
    uint32_t rv1_counter;   /* full counter reconstructed from the seed */
    uint32_t rv1_tx_cnt;    /* session TX counter (increments per Emulate) */
    uint8_t  rv1_button;
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

static void pp_bf_worker(void *arg)
{
    pp_bf_ctx_t *ctx = (pp_bf_ctx_t *)arg;

    if (ctx->kind == PP_BF_KIND_PSA)
    {
        (void)subghz_psa_bf_run(&ctx->job, ctx->key1, (uint16_t)ctx->key2,
                                &ctx->psa);
    }
    else
    {
        uint8_t frame[RENAULT_V1_BF_FRAME_BYTES];
        subghz_renault_v1_bf_frame_from_capture(frame, ctx->key1, ctx->key2);
        (void)subghz_renault_v1_bf_run(&ctx->job, frame, ctx->rv1_iv);
    }

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
 *  BF-able PSA or Renault V1 key file carrying Key_2 and the context fields
 *  are populated. */
static bool pp_bf_load_capture(pp_bf_ctx_t *ctx)
{
    bool ok = false;
    flipper_subghz_signal_t *sig = malloc(sizeof(*sig));
    if (sig == NULL)
        return false;

    memset(sig, 0, sizeof(*sig));
    if (flipper_subghz_load(ctx->src_path, sig) &&
        sig->type == FLIPPER_SUBGHZ_TYPE_PARSED)
    {
        if (pp_str_ieq(sig->protocol, "PSA") &&
            flipper_subghz_psa_is_tea_candidate(
                sig->has_key_2, sig->has_protocol_type, sig->protocol_type))
        {
            ctx->kind = PP_BF_KIND_PSA;
            ok = true;
        }
        else if (pp_str_ieq(sig->protocol, "Renault V1") && sig->has_key_2)
        {
            ctx->kind = PP_BF_KIND_RENAULT_V1;
            ok = true;
        }
    }

    if (ok)
    {
        ctx->key1      = sig->key;
        ctx->key2      = sig->key_2;
        ctx->frequency = sig->frequency;
        ctx->te        = sig->te;
        ctx->bit_count = sig->bit_count;
        strncpy(ctx->preset, sig->preset, sizeof(ctx->preset) - 1);
        ctx->preset[sizeof(ctx->preset) - 1] = '\0';
    }

    free(sig);
    return ok;
}

static bool pp_bf_start_worker(pp_bf_ctx_t *ctx)
{
    subghz_bruteforce_job_init(&ctx->job);
    if (!subghz_bruteforce_job_begin(
            &ctx->job, ctx->kind == PP_BF_KIND_PSA ? PSA_BF_TOTAL
                                                   : RENAULT_V1_BF_CANDIDATES))
        return false;

    ctx->done = xSemaphoreCreateBinary();
    if (ctx->done == NULL)
        return false;

    if (xTaskCreate(pp_bf_worker, "pp_bf", PP_BF_TASK_STACK_WORDS, ctx,
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

    if (ctx->kind == PP_BF_KIND_PSA)
    {
        return flipper_subghz_save_psa_recovered(path, ctx->frequency,
                                                 ctx->preset, ctx->bit_count,
                                                 ctx->key1, ctx->key2, ctx->te,
                                                 &ctx->psa);
    }

    return flipper_subghz_save_renault_v1_recovered(path, ctx->frequency,
                                                    ctx->preset,
                                                    ctx->bit_count, ctx->key1,
                                                    ctx->key2, ctx->te,
                                                    ctx->rv1_seed);
}

/** Complete the Renault V1 result fields once the search has found the IV. */
static void pp_bf_rv1_finish(pp_bf_ctx_t *ctx)
{
    uint8_t frame[RENAULT_V1_BF_FRAME_BYTES];
    uint16_t cnt10 = 0;

    subghz_renault_v1_bf_frame_from_capture(frame, ctx->key1, ctx->key2);
    subghz_renault_v1_bf_unpack_frame(frame, &ctx->rv1_serial,
                                      &ctx->rv1_button, &cnt10,
                                      &ctx->rv1_hop);
    ctx->rv1_seed = subghz_renault_v1_bf_seed_from_iv(ctx->rv1_iv);
    ctx->rv1_counter = subghz_renault_v1_bf_counter_from_seed(ctx->rv1_seed);
    /* The first emulated frame must advance past the captured counter. */
    ctx->rv1_tx_cnt = ctx->rv1_counter + 1u;
}

/*============================================================================*/
/* Renault V1 TX after recovery                                                */
/*============================================================================*/

/**
 * Write the upload waveform as an M1 native NOISE .sgh (unsigned alternating
 * durations, first sample is a mark).  Consecutive same-level entries are
 * merged so the alternation invariant holds.
 */
static bool pp_bf_rv1_write_waveform(const char *path,
                                    const renault_v1_upload_entry_t *upload,
                                    uint32_t count, uint32_t frequency)
{
    FIL f;
    if (f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return false;

    bool ok = f_puts("Filetype: M1 SubGHz NOISE\r\n", &f) >= 0 &&
              f_puts("Version: 0.9\r\n", &f) >= 0;

    char line[160];
    if (ok)
    {
        snprintf(line, sizeof(line), "Frequency: %lu\r\nModulation: OOK\r\n",
                 (unsigned long)frequency);
        ok = f_puts(line, &f) >= 0;
    }

    /* Stream merged durations in Data: lines of bounded length. */
    int pos = snprintf(line, sizeof(line), "Data:");
    bool have_pending = false;
    uint32_t pending = 0;
    for (uint32_t i = 0; ok && i < count; i++)
    {
        if (!have_pending)
        {
            pending = upload[i].duration;
            have_pending = true;
            continue;
        }
        if (upload[i].level == upload[i - 1u].level)
        {
            pending += upload[i].duration;
            continue;
        }

        int w = snprintf(line + pos, sizeof(line) - (size_t)pos, " %lu",
                         (unsigned long)pending);
        if (w <= 0)
        {
            ok = false;
            break;
        }
        pos += w;
        have_pending = false;
        pending = upload[i].duration;
        have_pending = true;

        if (pos > 100)
        {
            ok = f_puts(line, &f) >= 0 && f_puts("\r\n", &f) >= 0;
            pos = snprintf(line, sizeof(line), "Data:");
        }
    }
    if (ok && have_pending)
    {
        snprintf(line + pos, sizeof(line) - (size_t)pos, " %lu",
                 (unsigned long)pending);
        pos = (int)strlen(line);
    }
    if (ok && pos > 5)
        ok = f_puts(line, &f) >= 0 && f_puts("\r\n", &f) >= 0;

    f_close(&f);
    return ok;
}

/** Synthesize a fresh Renault V1 frame (counter incremented per press) and
 *  hand it to the Read Raw scene in Loaded state for async TX. */
static bool pp_bf_rv1_emulate(SubGhzApp *app, pp_bf_ctx_t *ctx)
{
    uint8_t out[11];
    subghz_renault_v1_encrypt_frame(ctx->rv1_serial, ctx->rv1_tx_cnt,
                                    ctx->rv1_button, ctx->rv1_seed, out, NULL);

    uint64_t key = 0;
    for (uint8_t i = 0; i < 8u; i++)
        key = (key << 8) | out[i];
    const uint32_t key2 = ((uint32_t)out[8] << 16) |
                          ((uint32_t)out[9] << 8) | out[10];

    renault_v1_upload_entry_t *upload = malloc(
        RENAULT_V1_UPLOAD_CAPACITY * sizeof(renault_v1_upload_entry_t));
    if (upload == NULL)
        return false;

    bool ok = false;
    const uint32_t n = subghz_renault_v1_build_upload(
        upload, RENAULT_V1_UPLOAD_CAPACITY, key, key2);
    if (n > 0)
    {
        ok = pp_bf_rv1_write_waveform(PP_BF_TX_TMP_PATH, upload, n,
                                     ctx->frequency > 0 ? ctx->frequency
                                                        : 433920000UL);
    }
    free(upload);

    if (!ok)
        return false;

    ctx->rv1_tx_cnt++;

    /* Hand the temp waveform to Read Raw (Loaded state → async Send). */
    strncpy(app->raw_load_path, PP_BF_TX_TMP_PATH, SUBGHZ_SAVED_PATH_MAX - 1U);
    app->raw_load_path[SUBGHZ_SAVED_PATH_MAX - 1U] = '\0';
    app->raw_load_is_native = true;
    app->raw_load_freq_hz = ctx->frequency > 0 ? ctx->frequency : 433920000UL;
    app->raw_load_mod = MODULATION_OOK;
    app->resume_from_child = true;
    subghz_scene_push(app, SubGhzSceneReadRaw);
    return true;
}

/*============================================================================*/
/* Scene callbacks                                                             */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    /* Resume from the Read Raw TX handoff: keep the result screen. */
    if (s_ctx != NULL)
    {
        app->resume_from_child = false;
        app->need_redraw = true;
        return;
    }

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
                        if (s_ctx->kind == PP_BF_KIND_RENAULT_V1)
                            pp_bf_rv1_finish(s_ctx);
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

        case SubGhzEventRight:
            /* Renault V1: transmit a fresh frame with the recovered seed. */
            if ((s_ctx->state == PP_BF_STATE_FOUND ||
                 s_ctx->state == PP_BF_STATE_SAVED) &&
                s_ctx->kind == PP_BF_KIND_RENAULT_V1)
            {
                if (!pp_bf_rv1_emulate(app, s_ctx))
                {
                    s_ctx->state = PP_BF_STATE_ERROR;
                    app->need_redraw = true;
                }
                return true;
            }
            return false;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    subghz_scene_set_tick_period(app, 0);
    if (app->resume_from_child)
        return;  /* handing off to Read Raw for TX; keep the context */
    pp_bf_stop_worker();
    pp_bf_ctx_free();
    (void)f_unlink(PP_BF_TX_TMP_PATH);
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
    u8g2_DrawStr(&m1_u8g2, 2, 10,
                 (s_ctx != NULL && s_ctx->kind == PP_BF_KIND_RENAULT_V1)
                     ? "Renault V1 Brute Force"
                     : "PSA Brute Force");

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
            u8g2_DrawStr(&m1_u8g2, 2, 24,
                         s_ctx->kind == PP_BF_KIND_PSA ? "Searching TEA key..."
                                                       : "Recovering seed...");

            /* Progress bar with a real fill. */
            uint32_t cur;
            uint32_t total;
            subghz_bruteforce_job_progress_snapshot(&s_ctx->job, &cur, &total);
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
        case PP_BF_STATE_SAVED:
            if (s_ctx->kind == PP_BF_KIND_PSA)
            {
                u8g2_DrawStr(&m1_u8g2, 2, 24, "Key found!");
                snprintf(line, sizeof(line), "Ser: %06lX",
                         (unsigned long)s_ctx->psa.serial);
                u8g2_DrawStr(&m1_u8g2, 2, 34, line);
                snprintf(line, sizeof(line), "Btn: %X  Cnt: %lX",
                         (unsigned int)s_ctx->psa.button,
                         (unsigned long)s_ctx->psa.counter);
                u8g2_DrawStr(&m1_u8g2, 2, 44, line);
            }
            else
            {
                u8g2_DrawStr(&m1_u8g2, 2, 24, "Seed recovered!");
                snprintf(line, sizeof(line), "Seed: %08lX",
                         (unsigned long)s_ctx->rv1_seed);
                u8g2_DrawStr(&m1_u8g2, 2, 34, line);
                snprintf(line, sizeof(line), "Cnt: %lX  Btn: %X",
                         (unsigned long)s_ctx->rv1_counter,
                         (unsigned int)s_ctx->rv1_button);
                u8g2_DrawStr(&m1_u8g2, 2, 44, line);
            }
            if (s_ctx->state == PP_BF_STATE_SAVED)
                u8g2_DrawStr(&m1_u8g2, 2, 54, "Saved _recovered.sub");
            subghz_button_bar_draw(NULL, NULL, ok_circle_8x8, "Save",
                                   NULL,
                                   s_ctx->kind == PP_BF_KIND_RENAULT_V1
                                       ? "Emulate"
                                       : NULL);
            break;

        case PP_BF_STATE_NOT_FOUND:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Not found");
            u8g2_DrawStr(&m1_u8g2, 2, 36, "Full range searched");
            break;

        case PP_BF_STATE_CANCELLED:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Cancelled");
            break;

        case PP_BF_STATE_INVALID:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Unsupported capture");
            u8g2_DrawStr(&m1_u8g2, 2, 36, "Need PSA/Renault Key_2");
            break;

        case PP_BF_STATE_ERROR:
        default:
            u8g2_DrawStr(&m1_u8g2, 2, 24, "Operation failed");
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
