/* See COPYING.txt for license details. */

/*
 *
 * lcd_saver.h
 *
 * Pure-logic decision state machine for the LCD backlight screen-saver.
 *
 * The M1 dims its backlight after an inactivity window (m1_sleep_timeout_idx,
 * default 60 s). A button press reseats m1_device_stat.active_timestamp; when
 * HAL_GetTick() - active_timestamp exceeds the timeout the backlight is turned
 * off, and the next sub-timeout tick turns it back on. A long NFC/RFID scan
 * that finishes without any button press therefore shows its successful-read
 * info screen with the backlight already off, which reads as "nothing happened"
 * -- the bug this module's wake path fixes (see m1_lcd_wake_restart_timer()).
 *
 * This file is HARDWARE-INDEPENDENT: it contains no HAL/RTOS/GPIO access and is
 * unit-tested on the host (tests/test_lcd_saver.c). The caller supplies the
 * current tick, the last-activity timestamp and the timeout, and acts on the
 * returned action by driving the physical backlight.
 *
 * M1 Project
 *
 */

#ifndef LCD_SAVER_H_
#define LCD_SAVER_H_

#include <stdbool.h>
#include <stdint.h>

/* Action the caller must perform after a poll. */
typedef enum {
    LCD_SAVER_ACTION_NONE = 0,  /* leave the backlight as-is       */
    LCD_SAVER_ACTION_ON,        /* turn the backlight on (wake)    */
    LCD_SAVER_ACTION_OFF,       /* turn the backlight off (dim)    */
} lcd_saver_action_t;

/* Saver context. Zero-initialise (backlight assumed on) or call the init. */
typedef struct {
    bool backlight_off;   /* true once the saver has dimmed the backlight */
} lcd_saver_ctx_t;

/*
 * Initialise a saver context to the "backlight on" state.
 */
void lcd_saver_ctx_init(lcd_saver_ctx_t *ctx);

/*
 * Advance the screen-saver state machine.
 *
 *   now_ms           — current monotonic tick in milliseconds.
 *   active_timestamp — tick of the last activity (button press / wake).
 *   timeout_ms       — inactivity window before dimming; 0 means "never dim".
 *
 * Returns LCD_SAVER_ACTION_ON when the backlight should transition from off to
 * on, LCD_SAVER_ACTION_OFF when it should transition from on to off, and
 * LCD_SAVER_ACTION_NONE when no transition is needed. Tick wrap-around is
 * handled via unsigned subtraction.
 */
lcd_saver_action_t lcd_saver_poll(lcd_saver_ctx_t *ctx,
                                    uint32_t now_ms,
                                    uint32_t active_timestamp,
                                    uint32_t timeout_ms);

#endif /* LCD_SAVER_H_ */
