/* See COPYING.txt for license details. */

/*
 * badusb_hold.c
 *
 * Pure-logic held-key state for the BadUSB HOLD / RELEASE DuckyScript commands.
 * See badusb_hold.h for the API contract.
 *
 * M1 Project
 */

#include "badusb_hold.h"
#include <string.h>

void busb_hold_init(busb_hold_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

void busb_hold_release_all(busb_hold_state_t *s)
{
    busb_hold_init(s);
}

bool busb_hold_is_empty(const busb_hold_state_t *s)
{
    if (!s) return true;
    return (s->modifiers == 0 && s->count == 0);
}

bool busb_hold_add(busb_hold_state_t *s, uint8_t modifier, uint8_t keycode)
{
    if (!s) return false;

    s->modifiers |= modifier;

    if (keycode == 0)
        return true;   /* modifier-only hold */

    /* Already held? no-op. */
    for (uint8_t i = 0; i < s->count; i++)
    {
        if (s->keys[i] == keycode)
            return true;
    }

    if (s->count >= BUSB_HOLD_MAX_KEYS)
        return false;  /* no room */

    s->keys[s->count++] = keycode;
    return true;
}

void busb_hold_remove(busb_hold_state_t *s, uint8_t modifier, uint8_t keycode)
{
    if (!s) return;

    s->modifiers &= (uint8_t)~modifier;

    if (keycode == 0)
        return;   /* modifier-only release */

    for (uint8_t i = 0; i < s->count; i++)
    {
        if (s->keys[i] == keycode)
        {
            /* Shift the remaining keys down to keep the table compact. */
            for (uint8_t j = i; j + 1 < s->count; j++)
                s->keys[j] = s->keys[j + 1];
            s->count--;
            s->keys[s->count] = 0;
            return;
        }
    }
}

void busb_hold_build_report(const busb_hold_state_t *s,
                            uint8_t modifier, uint8_t keycode,
                            uint8_t report[8])
{
    memset(report, 0, 8);
    if (!s)
    {
        report[0] = modifier;
        if (keycode != 0)
            report[2] = keycode;
        return;
    }

    report[0] = (uint8_t)(s->modifiers | modifier);

    uint8_t slot = 2;   /* report[2..7] hold up to 6 keycodes */
    for (uint8_t i = 0; i < s->count && slot < 8; i++)
        report[slot++] = s->keys[i];

    if (keycode != 0 && slot < 8)
    {
        /* Avoid duplicating a key that is already held. */
        bool dup = false;
        for (uint8_t i = 0; i < s->count; i++)
        {
            if (s->keys[i] == keycode) { dup = true; break; }
        }
        if (!dup)
            report[slot++] = keycode;
    }
}
