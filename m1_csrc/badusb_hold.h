/* See COPYING.txt for license details. */

/*
 * badusb_hold.h
 *
 * Pure-logic held-key state for the BadUSB HOLD / RELEASE DuckyScript commands.
 *
 * Maintains the set of currently-held modifiers and keycodes and builds the
 * 8-byte USB HID boot-keyboard report that merges the held keys with an
 * optional transient key press. Hardware-independent: no RTOS, no USB.
 *
 * M1 Project
 */

#ifndef BADUSB_HOLD_H_
#define BADUSB_HOLD_H_

#include <stdint.h>
#include <stdbool.h>

/* USB HID boot keyboard supports up to 6 simultaneous non-modifier keys. */
#define BUSB_HOLD_MAX_KEYS  6

typedef struct
{
    uint8_t modifiers;                 /* OR of held modifier bits */
    uint8_t keys[BUSB_HOLD_MAX_KEYS];  /* held keycodes (0 = empty slot) */
    uint8_t count;                     /* number of held keycodes in use */
} busb_hold_state_t;

/** Reset the hold state to empty (nothing held). */
void busb_hold_init(busb_hold_state_t *s);

/**
 * Add held modifier bits and/or a held keycode.
 * A keycode of 0 (BUSB_KEY_NONE) adds only the modifier bits.
 * Adding a keycode that is already held is a no-op (no duplicate).
 * @return true on success; false if the keycode table is already full.
 */
bool busb_hold_add(busb_hold_state_t *s, uint8_t modifier, uint8_t keycode);

/**
 * Remove held modifier bits and/or a specific held keycode.
 * A keycode of 0 removes only the modifier bits.
 */
void busb_hold_remove(busb_hold_state_t *s, uint8_t modifier, uint8_t keycode);

/** Release everything (equivalent to busb_hold_init). */
void busb_hold_release_all(busb_hold_state_t *s);

/** @return true if nothing is currently held. */
bool busb_hold_is_empty(const busb_hold_state_t *s);

/**
 * Build an 8-byte HID boot-keyboard report from the held state merged with an
 * optional transient key press (modifier bits and/or keycode). Pass 0/0 to
 * report only the held state. Extra keys beyond BUSB_HOLD_MAX_KEYS are dropped.
 */
void busb_hold_build_report(const busb_hold_state_t *s,
                            uint8_t modifier, uint8_t keycode,
                            uint8_t report[8]);

#endif /* BADUSB_HOLD_H_ */
