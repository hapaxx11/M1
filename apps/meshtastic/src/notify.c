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

/* Play one tone for `ms`, then wait it out (the M1 buzzer is timer-driven and
 * ignores a new tone while one is still sounding). Never pass 0 Hz / 0 ms — the
 * firmware buzzer latches busy on those. */
static void spk_tone(int freq, int ms) {
    if(freq <= 0) freq = 1;
    if(ms <= 0) ms = 1;
    if(freq > 20000) freq = 20000;
    m1_buzzer_set((uint16_t)freq, (uint16_t)ms);
    m1app_delay((uint32_t)ms + 6);
}
static void spk_gap(int ms) {
    if(ms > 0) m1app_delay((uint32_t)ms);
}

static void play_nokia(void) {
    const int melody[] = {659, 587, 370, 415, 554, 494, 370, 330};
    const int durations[] = {125, 125, 250, 250, 125, 125, 250, 250};
    for(int i = 0; i < 8; i++) {
        spk_tone(melody[i], durations[i]);
        spk_gap(20);
    }
}
static void play_descend(void) {
    for(int f = 1200; f >= 600; f -= 100) spk_tone(f, 80);
}
static void play_bounce(void) {
    const int melody[] = {600, 800, 600, 900, 600, 1000};
    for(int i = 0; i < 6; i++) {
        spk_tone(melody[i], 100);
        spk_gap(30);
    }
}
static void play_alert(void) {
    for(int i = 0; i < 3; i++) {
        spk_tone(900, 150);
        spk_gap(100);
        spk_tone(700, 150);
        if(i < 2) spk_gap(100);
    }
}
static void play_pulse(void) {
    for(int i = 0; i < 4; i++) {
        spk_tone(800, 120);
        spk_gap(80);
    }
}
static void play_siren(void) {
    for(int cycle = 0; cycle < 2; cycle++) {
        for(int f = 600; f <= 900; f += 75) spk_tone(f, 40);
        for(int f = 900; f >= 600; f -= 75) spk_tone(f, 40);
    }
}
static void play_beep3(void) {
    for(int i = 0; i < 3; i++) {
        spk_tone(1000, 100);
        if(i < 2) spk_gap(100);
    }
}
static void play_trill(void) {
    const int melody[] = {800, 1000, 800, 1000, 800, 1000, 1200};
    const int durations[] = {80, 80, 80, 80, 80, 80, 200};
    for(int i = 0; i < 7; i++) {
        spk_tone(melody[i], durations[i]);
        if(i < 6) spk_gap(30);
    }
}
static void play_mario(void) {
    const int melody[] = {659, 659, 659, 523, 659, 784};
    const int durations[] = {150, 150, 150, 100, 150, 300};
    for(int i = 0; i < 6; i++) {
        spk_tone(melody[i], durations[i]);
        if(i < 5) spk_gap(50);
    }
}
static void play_levelup(void) {
    const int melody[] = {523, 659, 784, 1047, 1319, 1568};
    for(int i = 0; i < 6; i++) {
        spk_tone(melody[i], 100);
        if(i < 5) spk_gap(30);
    }
}
static void play_metric(void) {
    for(int i = 0; i < 4; i++) {
        spk_tone(1047, 120);
        if(i < 3) spk_gap(120);
    }
}
static void play_minimalist(void) {
    spk_tone(1047, 150);
    spk_gap(150);
    spk_tone(1047, 150);
}

void play_ringtone(ZeroMeshApp* app) {
    if(app->notify_ringtone == RingtoneNone) return;

    if(app->notify_ringtone >= RINGTONE_COUNT) {
        rtttl_play_custom(app, app->notify_ringtone);
        return;
    }

    switch(app->notify_ringtone) {
    case RingtoneShort:
        spk_tone(800, 100);
        break;
    case RingtoneDouble:
        spk_tone(800, 80);
        spk_gap(60);
        spk_tone(1000, 80);
        break;
    case RingtoneTriple:
        for(int i = 0; i < 3; i++) {
            spk_tone(800 + i * 200, 60);
            if(i < 2) spk_gap(40);
        }
        break;
    case RingtoneLong:
        spk_tone(600, 400);
        break;
    case RingtoneSOS:
        for(int i = 0; i < 3; i++) {
            spk_tone(800, 50);
            spk_gap(50);
        }
        spk_gap(100);
        for(int i = 0; i < 3; i++) {
            spk_tone(800, 150);
            spk_gap(50);
        }
        spk_gap(100);
        for(int i = 0; i < 3; i++) {
            spk_tone(800, 50);
            if(i < 2) spk_gap(50);
        }
        break;
    case RingtoneChirp:
        for(int f = 400; f <= 1200; f += 50) spk_tone(f, 15);
        break;
    case RingtoneNokia:
        play_nokia();
        break;
    case RingtoneDescend:
        play_descend();
        break;
    case RingtoneBounce:
        play_bounce();
        break;
    case RingtoneAlert:
        play_alert();
        break;
    case RingtonePulse:
        play_pulse();
        break;
    case RingtoneSiren:
        play_siren();
        break;
    case RingtoneBeep3:
        play_beep3();
        break;
    case RingtoneTrill:
        play_trill();
        break;
    case RingtoneMario:
        play_mario();
        break;
    case RingtoneLevelUp:
        play_levelup();
        break;
    case RingtoneMetric:
        play_metric();
        break;
    case RingtoneMinimalist:
        play_minimalist();
        break;
    default:
        break;
    }
}

static void led_flash(void) {
    for(int i = 0; i < 2; i++) {
        lp5814_led_on_Blue(255);
        m1app_delay(250);
        lp5814_all_off_RGB();
        m1app_delay(100);
    }
}

void notify_rx_message(ZeroMeshApp* app) {
    if(app->notify_vibro) {
        /* No vibration motor on the M1 header; stand in with a short buzz. */
        spk_tone(200, 120);
    }
    if(app->notify_led) {
        led_flash();
    }
    if(app->notify_ringtone != RingtoneNone) {
        play_ringtone(app);
    }
}
