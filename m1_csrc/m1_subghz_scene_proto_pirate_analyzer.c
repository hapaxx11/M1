/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_proto_pirate_analyzer.c
 * @brief  Proto Pirate — Remote Analyzer scene (fully asynchronous).
 *
 * Sweeps the region hopper frequencies for a signal, then runs a fine
 * ±300 kHz sweep (20 kHz steps) around the coarse peak to report the
 * signal's frequency, RSSI and a modulation verdict (AM vs other).
 * Detection concept derived from ProtoPirate's CC1101 Remote Analyzer
 * (GPLv3), reworked for the SI4463 and M1's radio infrastructure:
 * there is no narrow-BW second stage — the fine sweep runs with the
 * same RX config as the coarse stage (documented M1 adaptation).
 * All detection math lives in Sub_Ghz/subghz_remote_analyzer.c and the
 * per-point radio sweep sequence is shared in m1_sub_ghz.c
 * (subghz_fine_sweep_points_ext); this file only owns the coarse/fine
 * staging and the display.
 *
 * Layout (128×64, compact data display — not a selection list):
 *   Row  0–10: Header "Remote Analyzer" + separator
 *   Row 13–27: Big frequency line "433.920 MHz" / "Scanning..."
 *   Row 30–37: RSSI bar (live) + trigger cursor + dBm text
 *   Row 40–48: Modulation verdict: "Mod: AM" / "Mod: FM?" / "Mod: --"
 *   Row 50–58: Trigger level + Paused/scanning indicator
 *   Row 60–63: Legend: OK pause/resume, LEFT/RIGHT trigger ±3 dB
 *
 * Async model (200 ms tick, same cadence as the Timing Tuner scene):
 *   - COARSE: one hopper frequency per tick; after the 6th, if the
 *     coarse peak is above the trigger enter FINE, else deliver and
 *     restart the coarse cycle.
 *   - FINE: 8 sweep points per tick (31 total) so no event handler
 *     blocks long; when complete, analyze + deliver, back to COARSE.
 *   - OK toggles PAUSED: tuning stops (radio stays in RX at the last
 *     frequency), the live RSSI readout keeps updating.
 *   - LEFT/RIGHT adjust the trigger in 3 dB steps (-110 .. -60 dBm).
 *   - BACK / scene exit: instant cancel — pause + deinit exactly like
 *     the tuner's exit path.
 *
 * Radio lifecycle mirrors the Timing Tuner scene, minus TIM1 input
 * capture: the analyzer is RSSI-only, so sub_ghz_rx_start_ext() is
 * deliberately NOT called — running TIM1 capture without record mode
 * floods the main queue with Q_EVENT_SUBGHZ_RX noise edges (Read Raw
 * passive-listen rule).  sub_ghz_rx_pause_ext() on exit is a no-op for
 * a never-started timer (channel-state guarded), so the exit path is
 * identical to the tuner's.
 *
 * The per-point radio work (band mux → READY → Set_Frequency → Start_Rx →
 * 2 ms settle → RSSI read) is shared with Smart ID refinement via
 * subghz_fine_sweep_points_ext() in m1_sub_ghz.c; each point costs ~3 ms,
 * so a coarse point is ~3 ms and a fine tick (8 points) is ≤ ~25 ms.
 * Bounded and short, no external-event dependency; an async redesign
 * would add ~200 ms of dead time per sweep point with no UX benefit.
 * Same idiom as the Spectrum/Frequency Analyzer sweeps in m1_sub_ghz.c.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "m1_subghz_scene.h"
#include "m1_led_indicator.h"
#include "m1_lp5814.h"
#include "subghz_freq_presets.h"   /* SUBGHZ_HOPPER_FREQ_COUNT */
#include "subghz_remote_analyzer.h"
#include "subghz_rssi_bar.inc"     /* subghz_rssi_fill_w(), SUBGHZ_RSSI_FLOOR/CEIL */

/* Shared radio state (defined in m1_sub_ghz.c) */
extern S_M1_SubGHz_Scan_Config subghz_scan_config;

/* Radio control helpers (defined in m1_sub_ghz.c) */
extern void menu_sub_ghz_init(void);
extern void sub_ghz_set_opmode_ext(uint8_t opmode, uint8_t band,
                                   uint8_t channel, uint8_t tx_power);
extern void sub_ghz_rx_init_ext(void);
extern void sub_ghz_rx_pause_ext(void);
extern void sub_ghz_rx_deinit_ext(void);
extern int16_t subghz_read_rssi_ext(void);
extern void subghz_apply_config_ext(uint8_t freq_idx, uint8_t mod_idx);

/* Hopper: region-aware coarse-stage frequency list (defined in m1_sub_ghz.c) */
extern const uint32_t *subghz_get_hopper_freqs_ext(void);

/*============================================================================*/
/* Scene-local state                                                          */
/*============================================================================*/

typedef enum {
    RA_STAGE_COARSE = 0,   /**< Stepping through the 6 hopper frequencies */
    RA_STAGE_FINE,         /**< ±300 kHz sweep around the coarse peak */
} ra_stage_t;

/** Fine-sweep points collected per tick (keeps the handler ≤ ~25 ms). */
#define RA_FINE_POINTS_PER_TICK  8u

/** RSSI bar geometry */
#define RA_BAR_X    2
#define RA_BAR_Y    31
#define RA_BAR_W    88
#define RA_BAR_H    5

static subghz_ra_state_t s_ra;                          /**< Detection state machine */
static int16_t  s_sweep[SUBGHZ_RA_SWEEP_POINTS];        /**< Fine-stage RSSI samples */

static ra_stage_t s_stage;
static uint8_t  s_coarse_idx;    /**< Next hopper frequency index (0..5) */
static uint8_t  s_fine_idx;      /**< Next fine sweep point (0..30) */
static uint32_t s_fine_base;     /**< Coarse peak the fine sweep centers on */

static bool     s_paused;        /**< OK toggles: sweep paused, RSSI live */
static int16_t  s_trigger;       /**< Detection trigger level (dBm) */

/* Last delivered frame (what the display shows) */
static uint32_t s_disp_freq;
static int16_t  s_disp_rssi;
static bool     s_disp_is_am;
static bool     s_disp_present;

static int16_t  s_rssi_live;     /**< Most recent raw RSSI reading (dBm) */
static uint8_t  s_anim;          /**< Scanning spinner frame */
static bool     s_rx_active;     /**< Radio lifecycle guard (like the tuner) */

/*============================================================================*/
/* Radio helpers                                                              */
/*============================================================================*/

/** Start RX (RSSI-only; no TIM1 capture — see file header). */
static bool start_rx(SubGhzApp *app)
{
    menu_sub_ghz_init();
    subghz_apply_config_ext(app->freq_idx, app->mod_idx);
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_RX, subghz_scan_config.band, 0, 0);
    sub_ghz_rx_init_ext();

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
    return true;
}

/** Stop RX cleanly (identical to the tuner's exit path). */
static void stop_rx(void)
{
    if (!s_rx_active)
        return;
    sub_ghz_rx_pause_ext();
    sub_ghz_rx_deinit_ext();
    sub_ghz_set_opmode_ext(SUB_GHZ_OPMODE_ISOLATED, subghz_scan_config.band, 0, 0);
    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
    s_rx_active = false;
}

/**
 * Tune to an arbitrary frequency and read RSSI via the shared fine-sweep
 * collector: sweep index 15 is exactly the sweep center, so a 1-point
 * "sweep" around (freq) reads at freq itself with the same radio sequence
 * as the fine stage.
 */
static int16_t ra_tune_and_read(uint32_t freq_hz)
{
    int16_t rssi;
    subghz_fine_sweep_points_ext(freq_hz, 15, 1, &rssi);
    return rssi;
}

/*============================================================================*/
/* Scan state machine                                                         */
/*============================================================================*/

static void apply_frame(subghz_ra_frame_t frame)
{
    s_disp_freq    = frame.frequency;
    s_disp_rssi    = frame.rssi;
    s_disp_is_am   = frame.is_am;
    s_disp_present = frame.signal_present;
}

/** One tick of the sweep.  Never blocks longer than ~25 ms. */
static void scan_tick(void)
{
    if (s_stage == RA_STAGE_COARSE)
    {
        const uint32_t *hopper = subghz_get_hopper_freqs_ext();
        uint32_t freq = hopper[s_coarse_idx];
        int16_t rssi  = ra_tune_and_read(freq);

        s_rssi_live = rssi;
        subghz_ra_coarse_feed(&s_ra, freq, rssi);
        s_coarse_idx++;

        if (s_coarse_idx >= SUBGHZ_HOPPER_FREQ_COUNT)
        {
            s_coarse_idx = 0;
            if (s_ra.coarse.best_rssi > s_trigger)
            {
                /* Signal candidate: refine around the coarse peak */
                s_fine_base = s_ra.coarse.best_freq;
                s_fine_idx  = 0;
                s_stage     = RA_STAGE_FINE;
            }
            else
            {
                /* deliver() consumes the cycle's peaks, so the next
                 * coarse cycle starts clean. */
                apply_frame(subghz_ra_deliver(&s_ra, s_trigger));
            }
        }
    }
    else /* RA_STAGE_FINE */
    {
        uint8_t n = SUBGHZ_RA_SWEEP_POINTS - s_fine_idx;
        if (n > RA_FINE_POINTS_PER_TICK)
            n = RA_FINE_POINTS_PER_TICK;
        subghz_fine_sweep_points_ext(s_fine_base, s_fine_idx, n,
                                     &s_sweep[s_fine_idx]);
        s_rssi_live = s_sweep[s_fine_idx + n - 1];
        s_fine_idx += n;

        if (s_fine_idx >= SUBGHZ_RA_SWEEP_POINTS)
        {
            bool is_am = false;
            subghz_ra_fine_analyze(s_sweep, SUBGHZ_RA_SWEEP_POINTS,
                                   s_fine_base, &s_ra, &is_am);
            apply_frame(subghz_ra_deliver(&s_ra, s_trigger));
            s_stage = RA_STAGE_COARSE;
        }
    }
}

/*============================================================================*/
/* Draw                                                                       */
/*============================================================================*/

static void draw(SubGhzApp *app)
{
    (void)app;

    char line[40];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* ---- Header ------------------------------------------------------- */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 9, "Remote Analyzer");
    u8g2_DrawHLine(&m1_u8g2, 0, 11, M1_LCD_DISPLAY_WIDTH);

    /* ---- Big frequency line ------------------------------------------- */
    u8g2_SetFont(&m1_u8g2, M1_DISP_LARGE_FONT_1B);
    if (s_disp_freq != 0)
        snprintf(line, sizeof(line), "%lu.%03lu MHz",
                 (unsigned long)(s_disp_freq / 1000000UL),
                 (unsigned long)((s_disp_freq % 1000000UL) / 1000UL));
    else
        snprintf(line, sizeof(line), "Scanning...");
    u8g2_DrawStr(&m1_u8g2, 2, 26, line);

    /* ---- RSSI bar (live) + trigger cursor + dBm text ------------------- */
    u8g2_DrawFrame(&m1_u8g2, RA_BAR_X, RA_BAR_Y, RA_BAR_W, RA_BAR_H);
    uint8_t fill = subghz_rssi_fill_w(s_rssi_live, (uint8_t)(RA_BAR_W - 2));
    if (fill > 0)
        u8g2_DrawBox(&m1_u8g2, RA_BAR_X + 1, RA_BAR_Y + 1, fill, RA_BAR_H - 2);

    /* Trigger cursor: vertical tick at the trigger level's bar position */
    uint8_t trig_x = RA_BAR_X + 1
                     + subghz_rssi_fill_w(s_trigger, (uint8_t)(RA_BAR_W - 2));
    u8g2_DrawVLine(&m1_u8g2, trig_x, RA_BAR_Y - 2, RA_BAR_H + 4);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    int16_t rssi_txt = s_rssi_live;
    if (rssi_txt < (int16_t)SUBGHZ_RSSI_FLOOR) rssi_txt = (int16_t)SUBGHZ_RSSI_FLOOR;
    if (rssi_txt > (int16_t)SUBGHZ_RSSI_CEIL)  rssi_txt = (int16_t)SUBGHZ_RSSI_CEIL;
    snprintf(line, sizeof(line), "%ddBm", (int)rssi_txt);
    u8g2_DrawStr(&m1_u8g2, RA_BAR_X + RA_BAR_W + 4, RA_BAR_Y + RA_BAR_H, line);

    /* ---- Modulation verdict -------------------------------------------- */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    snprintf(line, sizeof(line), "Mod: %s",
             !s_disp_present ? "--" : (s_disp_is_am ? "AM" : "FM?"));
    u8g2_DrawStr(&m1_u8g2, 2, 47, line);

    /* Signal RSSI of the delivered frame (when a signal is held) */
    if (s_disp_freq != 0)
    {
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        snprintf(line, sizeof(line), "pk:%d", (int)s_disp_rssi);
        u8g2_DrawStr(&m1_u8g2, 80, 47, line);
    }

    /* ---- Trigger level + paused/scanning indicator ---------------------- */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    snprintf(line, sizeof(line), "Trg:%d", (int)s_trigger);
    u8g2_DrawStr(&m1_u8g2, 2, 57, line);

    static const char *anim_chars[] = { "|", "/", "-", "\\" };
    const char *state = s_paused ? "Paused"
                        : (s_stage == RA_STAGE_FINE) ? "Fine"
                        : anim_chars[s_anim & 0x03u];
    u8g2_DrawStr(&m1_u8g2, 96, 57, state);

    /* ---- Legend ---------------------------------------------------------- */
    u8g2_DrawStr(&m1_u8g2, 2, 63, s_paused ? "OK:Resume LR:Trg" : "OK:Pause LR:Trg");

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    subghz_ra_reset(&s_ra);
    memset(s_sweep, 0, sizeof(s_sweep));
    s_stage        = RA_STAGE_COARSE;
    s_coarse_idx   = 0;
    s_fine_idx     = 0;
    s_fine_base    = 0;
    s_paused       = false;
    s_trigger      = SUBGHZ_RA_TRIGGER_DBM;
    s_disp_freq    = 0;
    s_disp_rssi    = 0;
    s_disp_is_am   = false;
    s_disp_present = false;
    s_rssi_live    = -120;
    s_anim         = 0;

    s_rx_active = start_rx(app);

    /* 200 ms ticks drive the sweep (same cadence as the tuner scene) */
    subghz_scene_set_tick_period(app, 200U);

    app->need_redraw = true;
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    switch (event)
    {
        case SubGhzEventBack:
            stop_rx();
            subghz_scene_set_tick_period(app, 0);
            subghz_scene_pop(app);
            return true;

        case SubGhzEventOk:
            /* Pause/resume the sweep.  Radio stays in RX at the last tuned
             * frequency so the live RSSI readout keeps updating. */
            s_paused = !s_paused;
            m1_led_fast_blink(LED_BLINK_ON_RGB,
                              s_paused ? LED_FASTBLINK_PWM_OFF : LED_FASTBLINK_PWM_M,
                              s_paused ? LED_FASTBLINK_ONTIME_OFF : LED_FASTBLINK_ONTIME_M);
            app->need_redraw = true;
            return true;

        case SubGhzEventLeft:
            s_trigger = subghz_ra_adjust_trigger(s_trigger, false);
            app->need_redraw = true;
            return true;

        case SubGhzEventRight:
            s_trigger = subghz_ra_adjust_trigger(s_trigger, true);
            app->need_redraw = true;
            return true;

        case SubGhzEventTick:
            if (s_rx_active)
            {
                if (s_paused)
                    s_rssi_live = subghz_read_rssi_ext();
                else
                    scan_tick();
            }
            s_anim++;
            app->need_redraw = true;
            return true;

        default:
            break;
    }
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    (void)app;
    stop_rx();
    subghz_scene_set_tick_period(app, 0);
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_proto_pirate_analyzer_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
