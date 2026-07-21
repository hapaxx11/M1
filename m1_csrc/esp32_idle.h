/* See COPYING.txt for license details. */

/*
 *
 * esp32_idle.h
 *
 * Pure-logic idle-timeout state machine for the ESP32-C6 coprocessor.
 *
 * The M1 leaves the ESP32-C6 powered (EN pin high) after a WiFi/BT/802.15.4
 * feature exits — m1_esp32_deinit() tears down the SPI/UART transport but does
 * NOT drop the enable line, so the coprocessor keeps drawing current forever.
 * This module owns the decision of *when* to cut power: once the transport has
 * been idle (not initialised) for a bounded window while the C6 is still
 * powered, the coprocessor is powered off to save battery.  Re-entry into any
 * ESP32 feature re-enables and re-boots the C6 via the normal init path.
 *
 * This file is HARDWARE-INDEPENDENT: it contains no HAL/RTOS/GPIO access and is
 * unit-tested on the host (tests/test_esp32_idle.c).  The caller supplies the
 * current power/busy state and monotonic tick, and acts on the returned action.
 *
 * M1 Project
 *
 */

#ifndef ESP32_IDLE_H_
#define ESP32_IDLE_H_

#include <stdbool.h>
#include <stdint.h>

/* Idle window (ms) after which a powered-but-unused ESP32-C6 is powered off. */
#define ESP32_IDLE_POWER_OFF_MS   60000u

/* Action the caller must perform after a poll. */
typedef enum {
    ESP32_IDLE_ACTION_NONE = 0,   /* do nothing                          */
    ESP32_IDLE_ACTION_POWER_OFF,  /* cut ESP32-C6 power (drop EN pin low) */
} esp32_idle_action_t;

/* Idle-timer context.  Zero-initialise then call esp32_idle_ctx_init(). */
typedef struct {
    uint32_t timeout_ms;    /* idle window before power-off              */
    uint32_t idle_since_ms; /* tick captured when the idle window opened */
    bool     idle_active;   /* an idle window is currently being timed   */
} esp32_idle_ctx_t;

/*
 * Initialise an idle context.  A timeout of 0 means "power off on the second
 * consecutive idle poll".  Use ESP32_IDLE_POWER_OFF_MS for the default window.
 */
void esp32_idle_ctx_init(esp32_idle_ctx_t *ctx, uint32_t timeout_ms);

/*
 * Advance the idle-timeout state machine.
 *
 *   powered  — true when the ESP32-C6 enable line is currently asserted.
 *   busy     — true when the ESP32 transport is initialised / in use.
 *   now_ms   — current monotonic tick in milliseconds.
 *
 * Returns ESP32_IDLE_ACTION_POWER_OFF exactly once when the coprocessor has
 * been powered but idle for at least timeout_ms; otherwise ESP32_IDLE_ACTION_NONE.
 * After returning POWER_OFF the internal window is cleared, so the action will
 * not repeat until the C6 is powered and used again.  Tick wrap-around is
 * handled via unsigned subtraction.
 */
esp32_idle_action_t esp32_idle_poll(esp32_idle_ctx_t *ctx,
                                    bool powered,
                                    bool busy,
                                    uint32_t now_ms);

#endif /* ESP32_IDLE_H_ */
