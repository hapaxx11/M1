/*
 * notify.c — RX alert tones + LED, reimplemented on the M1.
 *
 * ZeroMesh drove the Flipper speaker (furi_hal_speaker_start/stop) and the
 * notification LED/vibro via notification sequences. The M1 exposes a
 * fire-and-forget buzzer (m1_buzzer_set: play freq for N ms) and an LP5814 RGB
 * LED. Every furi_hal_speaker start/delay/stop triplet becomes one spk_tone().
 *
 * The M1 has no vibration motor in the app SDK, so "Vibration" is stood in with
 * a short buzz. The melodies themselves are carried over unchanged.
 */
#include "notify.h"
#include "rtttl.h"

const char* ringtone_names[] = {
    "Off",   "Short",  "Double",  "Triple", "Long",  "SOS",   "Chirp",
    "Nokia", "Descend", "Bounce", "Alert",  "Pulse", "Siren", "Beep3",
    "Trill", "Mario",  "LevelUp", "Metric", "Minimal",
};

typedef struct {
    uint16_t frequency;
    uint16_t duration_ms;
} ToneStep;

#define NOTE(freq, duration) {freq, duration}
#define GAP(duration) {0, duration}
#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

static const ToneStep s_short[] = {NOTE(800, 100)};
static const ToneStep s_double[] = {NOTE(800, 80), GAP(60), NOTE(1000, 80)};
static const ToneStep s_triple[] = {
    NOTE(800, 60), GAP(40), NOTE(1000, 60), GAP(40), NOTE(1200, 60)};
static const ToneStep s_long[] = {NOTE(600, 400)};
static const ToneStep s_sos[] = {
    NOTE(800, 50), GAP(50), NOTE(800, 50), GAP(50), NOTE(800, 50), GAP(100),
    NOTE(800, 150), GAP(50), NOTE(800, 150), GAP(50), NOTE(800, 150), GAP(50),
    GAP(100), NOTE(800, 50), GAP(50), NOTE(800, 50), GAP(50), NOTE(800, 50)};
static const ToneStep s_chirp[] = {
    NOTE(400, 15), NOTE(450, 15), NOTE(500, 15), NOTE(550, 15), NOTE(600, 15),
    NOTE(650, 15), NOTE(700, 15), NOTE(750, 15), NOTE(800, 15), NOTE(850, 15),
    NOTE(900, 15), NOTE(950, 15), NOTE(1000, 15), NOTE(1050, 15), NOTE(1100, 15),
    NOTE(1150, 15), NOTE(1200, 15)};
static const ToneStep s_nokia[] = {
    NOTE(659, 125), GAP(20), NOTE(587, 125), GAP(20), NOTE(370, 250), GAP(20),
    NOTE(415, 250), GAP(20), NOTE(554, 125), GAP(20), NOTE(494, 125), GAP(20),
    NOTE(370, 250), GAP(20), NOTE(330, 250), GAP(20)};
static const ToneStep s_descend[] = {
    NOTE(1200, 80), NOTE(1100, 80), NOTE(1000, 80), NOTE(900, 80),
    NOTE(800, 80), NOTE(700, 80), NOTE(600, 80)};
static const ToneStep s_bounce[] = {
    NOTE(600, 100), GAP(30), NOTE(800, 100), GAP(30), NOTE(600, 100), GAP(30),
    NOTE(900, 100), GAP(30), NOTE(600, 100), GAP(30), NOTE(1000, 100), GAP(30)};
static const ToneStep s_alert[] = {
    NOTE(900, 150), GAP(100), NOTE(700, 150), GAP(100),
    NOTE(900, 150), GAP(100), NOTE(700, 150), GAP(100),
    NOTE(900, 150), GAP(100), NOTE(700, 150)};
static const ToneStep s_pulse[] = {
    NOTE(800, 120), GAP(80), NOTE(800, 120), GAP(80),
    NOTE(800, 120), GAP(80), NOTE(800, 120), GAP(80)};
static const ToneStep s_siren[] = {
    NOTE(600, 40), NOTE(675, 40), NOTE(750, 40), NOTE(825, 40), NOTE(900, 40),
    NOTE(900, 40), NOTE(825, 40), NOTE(750, 40), NOTE(675, 40), NOTE(600, 40),
    NOTE(600, 40), NOTE(675, 40), NOTE(750, 40), NOTE(825, 40), NOTE(900, 40),
    NOTE(900, 40), NOTE(825, 40), NOTE(750, 40), NOTE(675, 40), NOTE(600, 40)};
static const ToneStep s_beep3[] = {
    NOTE(1000, 100), GAP(100), NOTE(1000, 100), GAP(100), NOTE(1000, 100)};
static const ToneStep s_trill[] = {
    NOTE(800, 80), GAP(30), NOTE(1000, 80), GAP(30), NOTE(800, 80), GAP(30),
    NOTE(1000, 80), GAP(30), NOTE(800, 80), GAP(30), NOTE(1000, 80), GAP(30),
    NOTE(1200, 200)};
static const ToneStep s_mario[] = {
    NOTE(659, 150), GAP(50), NOTE(659, 150), GAP(50), NOTE(659, 150), GAP(50),
    NOTE(523, 100), GAP(50), NOTE(659, 150), GAP(50), NOTE(784, 300)};
static const ToneStep s_levelup[] = {
    NOTE(523, 100), GAP(30), NOTE(659, 100), GAP(30), NOTE(784, 100), GAP(30),
    NOTE(1047, 100), GAP(30), NOTE(1319, 100), GAP(30), NOTE(1568, 100)};
static const ToneStep s_metric[] = {
    NOTE(1047, 120), GAP(120), NOTE(1047, 120), GAP(120),
    NOTE(1047, 120), GAP(120), NOTE(1047, 120)};
static const ToneStep s_minimalist[] = {
    NOTE(1047, 150), GAP(150), NOTE(1047, 150)};

static const ToneStep* const s_ringtones[RINGTONE_COUNT] = {
    NULL,          s_short,   s_double,   s_triple,  s_long,   s_sos,
    s_chirp,       s_nokia,   s_descend,  s_bounce,  s_alert,  s_pulse,
    s_siren,       s_beep3,   s_trill,    s_mario,   s_levelup, s_metric,
    s_minimalist,
};
static const size_t s_ringtone_lengths[RINGTONE_COUNT] = {
    0, ARRAY_LEN(s_short), ARRAY_LEN(s_double), ARRAY_LEN(s_triple),
    ARRAY_LEN(s_long), ARRAY_LEN(s_sos), ARRAY_LEN(s_chirp), ARRAY_LEN(s_nokia),
    ARRAY_LEN(s_descend), ARRAY_LEN(s_bounce), ARRAY_LEN(s_alert), ARRAY_LEN(s_pulse),
    ARRAY_LEN(s_siren), ARRAY_LEN(s_beep3), ARRAY_LEN(s_trill), ARRAY_LEN(s_mario),
    ARRAY_LEN(s_levelup), ARRAY_LEN(s_metric), ARRAY_LEN(s_minimalist),
};

typedef enum {
    NotifyIdle,
    NotifyVibro,
    NotifyLedOn,
    NotifyLedOff,
    NotifyRing,
} NotifyPhase;

static NotifyPhase s_phase;
static uint32_t s_deadline;
static bool s_waiting;
static bool s_alert_vibro;
static bool s_alert_led;
static uint16_t s_alert_ringtone;
static uint8_t s_pending_alerts;
static uint8_t s_led_remaining;
static const ToneStep* s_sequence;
static size_t s_sequence_length;
static size_t s_sequence_index;

static void select_ringtone(uint16_t index) {
    if(index >= RINGTONE_COUNT) {
        s_sequence = NULL;
        s_sequence_length = 0;
        return;
    }
    s_sequence = s_ringtones[index];
    s_sequence_length = s_ringtone_lengths[index];
    s_sequence_index = 0;
}

static void start_next_alert(void) {
    if(!s_pending_alerts) return;
    s_pending_alerts--;
    select_ringtone(s_alert_ringtone);
    s_led_remaining = s_alert_led ? 2 : 0;
    s_waiting = false;
    s_phase = s_alert_vibro ? NotifyVibro :
              (s_led_remaining ? NotifyLedOn : NotifyRing);
}

static void finish_notification(void) {
    s_phase = NotifyIdle;
    s_waiting = false;
    if(s_pending_alerts) start_next_alert();
}

static void wait_until(uint32_t now, uint16_t duration_ms) {
    s_deadline = now + duration_ms;
    s_waiting = true;
}

void play_ringtone(ZeroMeshApp* app) {
    if(!app) return;
    if(app->notify_ringtone >= RINGTONE_COUNT) {
        rtttl_play_custom(app, app->notify_ringtone);
        return;
    }
    select_ringtone(app->notify_ringtone);
    s_waiting = false;
    s_phase = NotifyRing;
}

void notify_rx_message(ZeroMeshApp* app) {
    if(!app) return;
    if(s_pending_alerts < UINT8_MAX) s_pending_alerts++;
    s_alert_vibro = app->notify_vibro;
    s_alert_led = app->notify_led;
    s_alert_ringtone = app->notify_ringtone;
    if(s_phase == NotifyIdle) start_next_alert();
}

void notify_poll(void) {
    uint32_t now = m1app_get_tick();
    if(s_phase == NotifyIdle) start_next_alert();
    if(s_phase == NotifyIdle) return;
    if(s_waiting) {
        if((int32_t)(now - s_deadline) < 0) return;
        s_waiting = false;
    }

    switch(s_phase) {
    case NotifyVibro:
        m1_buzzer_set(200, 120);
        s_phase = s_alert_led ? NotifyLedOn : NotifyRing;
        wait_until(now, 126);
        break;
    case NotifyLedOn:
        lp5814_led_on_Blue(255);
        s_phase = NotifyLedOff;
        wait_until(now, 250);
        break;
    case NotifyLedOff:
        lp5814_all_off_RGB();
        if(s_led_remaining) s_led_remaining--;
        s_phase = s_led_remaining ? NotifyLedOn : NotifyRing;
        wait_until(now, 100);
        break;
    case NotifyRing:
        if(s_sequence_index >= s_sequence_length) {
            finish_notification();
            break;
        }
        {
            ToneStep step = s_sequence[s_sequence_index++];
            if(step.frequency) m1_buzzer_set(step.frequency, step.duration_ms);
            wait_until(now, (uint16_t)(step.duration_ms + (step.frequency ? 6 : 0)));
        }
        break;
    case NotifyIdle:
    default:
        break;
    }
}
