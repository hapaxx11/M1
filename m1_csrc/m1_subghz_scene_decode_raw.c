/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_decode_raw.c
 * @brief  Sub-GHz Read-Raw offline decode-results scene (Phase 6).
 *
 * Pushed by the MoreRAW scene when the user selects "Decode" on a loaded
 * RAW file.  Loads @ref SubGhzApp.raw_filepath, runs the offline RAW
 * decode engine (subghz_decode_raw_offline) across every registered
 * protocol, and presents the matched protocols as a scrollable list with
 * a detail view.
 *
 *   List view (default):
 *     - Header: "Decode Results"
 *     - Body: "Decoded: N" followed by up to 3 visible rows showing
 *             "<protocol> 0x<key>" with selection highlight.
 *     - UP / DOWN scroll, OK opens the detail view for the selection.
 *     - "No protocols decoded" message when the engine returned 0 matches.
 *
 *   Detail view (OK from list):
 *     - Protocol name, Key + bit length, TE, Frequency, optional SN/RC.
 *     - BACK returns to the list view; second BACK pops the scene.
 *
 * The scene loads the file eagerly on entry into a file-scope
 * @ref flipper_subghz_signal_t (16 KB raw-sample buffer) to avoid blowing
 * the RTOS task stack — matching the convention already used by Read Raw
 * and the SavedMenu decode path.
 *
 * Phase 6 split: this overlay was previously inlined inside
 * `m1_subghz_scene_read_raw.c` behind `rr_in_decode` / `rr_decode_*`
 * file-scope statics and a separate `do_rr_decode()` helper.  Extracting
 * it removes ~140 lines of overlay state from Read Raw and gives the
 * decode view its own dedicated scene lifetime.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_subghz_scene.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_decenc.h"
#include "flipper_subghz.h"
#include "subghz_protocol_registry.h"
#include "subghz_raw_decoder.h"

extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern void subghz_pulse_handler_reset(void);

/*============================================================================*/
/* Scene state                                                                */
/*============================================================================*/

#define DECODE_RAW_MAX     16   /**< max distinct protocols shown */
#define DECODE_RAW_VISIBLE  3   /**< list rows visible at once */
#define DECODE_RAW_ROW_H    8   /**< px per list row */

static SubGhzRawDecodeResult decode_results[DECODE_RAW_MAX];
static uint8_t  decode_count;
static uint8_t  decode_sel;
static uint8_t  decode_scroll;
static bool     decode_detail;

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/** Load the file referenced by @p app->raw_filepath and run offline decode.
 *  Results are stored in @ref decode_results / @ref decode_count.  Failures
 *  leave @ref decode_count == 0 — the draw path will render the "No
 *  protocols decoded" message in that case. */
static void load_and_decode(const SubGhzApp *app)
{
    decode_count  = 0;
    decode_sel    = 0;
    decode_scroll = 0;
    decode_detail = false;

    if (app->raw_filepath[0] == '\0')
        return;

    char full_path[80];
    snprintf(full_path, sizeof(full_path), "0:%s", app->raw_filepath);

    /* Static to avoid a 16 KB stack allocation (raw_data[8192] lives inside
     * the struct).  This function only runs from the main RTOS task while
     * the scene is active; the blocking call chain in the scene prevents
     * re-entry, so non-reentrancy of a static local is safe. */
    static flipper_subghz_signal_t sig;
    memset(&sig, 0, sizeof(sig));
    if (!flipper_subghz_load(full_path, &sig))
        return;

    if (sig.type != FLIPPER_SUBGHZ_TYPE_RAW || sig.raw_count == 0)
        return;

    subghz_pulse_handler_reset();
    subghz_decenc_ctl.ndecodedrssi = 0;

    decode_count = subghz_decode_raw_offline(
        sig.raw_data, sig.raw_count, sig.frequency,
        decode_results, DECODE_RAW_MAX,
        subghz_registry_decode_try_fn, NULL);
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    load_and_decode(app);
    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            if (decode_detail)
            {
                decode_detail = false;
                app->need_redraw = true;
            }
            else
            {
                subghz_scene_pop(app);
            }
            return true;

        case SubGhzEventOk:
            if (!decode_detail && decode_count > 0)
            {
                decode_detail = true;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventUp:
            if (!decode_detail && decode_count > 0)
            {
                if (decode_sel > 0)
                    decode_sel--;
                if (decode_sel < decode_scroll)
                    decode_scroll = decode_sel;
                app->need_redraw = true;
            }
            return true;

        case SubGhzEventDown:
            if (!decode_detail && decode_count > 0)
            {
                if (decode_sel + 1 < decode_count)
                    decode_sel++;
                if (decode_sel >= decode_scroll + DECODE_RAW_VISIBLE)
                    decode_scroll = (uint8_t)(decode_sel - DECODE_RAW_VISIBLE + 1);
                app->need_redraw = true;
            }
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    (void)app;
}

/*============================================================================*/
/* Draw                                                                       */
/*============================================================================*/

static void draw(SubGhzApp *app)
{
    (void)app;
    char line[48];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "Decode Results");
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    if (decode_count == 0)
    {
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 10, 32, "No protocols");
        u8g2_DrawStr(&m1_u8g2, 10, 44, "decoded");
        m1_u8g2_nextpage();
        return;
    }

    if (decode_detail)
    {
        /* Detail view — full info for the selected result */
        const SubGhzRawDecodeResult *d = &decode_results[decode_sel];

        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, 24, protocol_text[d->protocol]);

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

        snprintf(line, sizeof(line), "Key: 0x%lX  %dbit",
                 (uint32_t)d->key, d->bit_len);
        u8g2_DrawStr(&m1_u8g2, 2, 34, line);

        snprintf(line, sizeof(line), "TE: %d us", d->te);
        u8g2_DrawStr(&m1_u8g2, 2, 43, line);

        /* Integer arithmetic — embedded nano.specs has no %f support */
        snprintf(line, sizeof(line), "Freq: %lu.%02lu MHz",
                 (unsigned long)(d->frequency / 1000000UL),
                 (unsigned long)((d->frequency % 1000000UL) / 10000UL));
        u8g2_DrawStr(&m1_u8g2, 2, 52, line);

        if (d->serial_number != 0 || d->rolling_code != 0)
        {
            snprintf(line, sizeof(line), "SN: %lX RC: %lX",
                     (unsigned long)d->serial_number,
                     (unsigned long)d->rolling_code);
            u8g2_DrawStr(&m1_u8g2, 2, 61, line);
        }
    }
    else
    {
        /* List view */
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

        snprintf(line, sizeof(line), "Decoded: %d", decode_count);
        u8g2_DrawStr(&m1_u8g2, 2, 22, line);

        uint8_t vis = (decode_count < DECODE_RAW_VISIBLE)
                      ? decode_count : DECODE_RAW_VISIBLE;
        for (uint8_t i = 0; i < vis; i++)
        {
            uint8_t idx = (uint8_t)(decode_scroll + i);
            if (idx >= decode_count) break;

            const SubGhzRawDecodeResult *d = &decode_results[idx];
            uint8_t y = (uint8_t)(24 + i * DECODE_RAW_ROW_H);

            if (idx == decode_sel)
            {
                u8g2_DrawRBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH,
                              DECODE_RAW_ROW_H, 2);
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            }

            snprintf(line, sizeof(line), "%s 0x%lX",
                     protocol_text[d->protocol], (uint32_t)d->key);
            u8g2_DrawStr(&m1_u8g2, 2, y + 6, line);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_decode_raw_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
