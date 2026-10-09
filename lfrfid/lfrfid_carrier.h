/* See COPYING.txt for license details. */

/*
 * lfrfid_carrier.h
 *
 * Pure-logic LF-RFID carrier-cycling state machine (hardware-independent).
 *
 * During a read the firmware energises the antenna at several LF carrier
 * frequencies in turn so that transponders tuned to different frequencies all
 * get a chance to respond.  This module owns the carrier definitions and the
 * cycle order; it touches no hardware, RTOS or display state and is unit-tested
 * on the host (tests/test_lfrfid_carrier.c).
 *
 * The sweep energises the antenna at 125, 128 and 134.2 kHz (ASK) and 62.5 kHz
 * (PSK).  This module only schedules excitation frequencies; whether a tag
 * answering at a given frequency can be decoded depends on the registered
 * protocol decoders, which are outside this module.
 */

#ifndef LFRFID_CARRIER_H_
#define LFRFID_CARRIER_H_

#include <stdint.h>
#include <stdbool.h>

/* Carrier mode for ASK/PSK auto-switching (follows Flipper Zero approach) */
#define LFRFID_CARRIER_SWITCH_MS    2000   /* switch every 2 seconds */
#define LFRFID_CARRIER_ASK_FREQ     125000
#define LFRFID_CARRIER_ASK_DUTY     0.5f
#define LFRFID_CARRIER_ASK_128_FREQ 128000  /* 128 kHz ASK excitation */
#define LFRFID_CARRIER_ASK_128_DUTY 0.5f
#define LFRFID_CARRIER_ASK_134_FREQ 134200  /* 134.2 kHz ASK excitation */
#define LFRFID_CARRIER_ASK_134_DUTY 0.5f
#define LFRFID_CARRIER_PSK_FREQ     62500
#define LFRFID_CARRIER_PSK_DUTY     0.25f

/* Number of carriers in the sweep and the time one full cycle takes.  A read
 * timeout must exceed LFRFID_CARRIER_CYCLE_MS so every carrier (including the
 * last, PSK) gets a full dwell before the read is torn down. */
#define LFRFID_CARRIER_COUNT        4
#define LFRFID_CARRIER_CYCLE_MS     (LFRFID_CARRIER_COUNT * LFRFID_CARRIER_SWITCH_MS)

typedef enum {
    LFRFID_CARRIER_ASK,      /* 125 kHz ASK */
    LFRFID_CARRIER_ASK_128,  /* 128 kHz ASK */
    LFRFID_CARRIER_ASK_134,  /* 134.2 kHz ASK */
    LFRFID_CARRIER_PSK,
} lfrfid_carrier_t;

/*
 * Return the next carrier in the sweep cycle:
 *   ASK 125k -> ASK 128k -> ASK 134.2k -> PSK -> ASK 125k ...
 * Any unknown value wraps back to the ASK 125 kHz start of the cycle.
 */
lfrfid_carrier_t lfrfid_carrier_next(lfrfid_carrier_t current);

/*
 * Resolve a carrier mode to its PWM parameters.  Writes the carrier frequency
 * (Hz) to *freq and the duty cycle to *duty.  Either pointer may be NULL.
 */
void lfrfid_carrier_params(lfrfid_carrier_t carrier, uint32_t *freq, float *duty);

/* True when the carrier uses PSK modulation (vs ASK). */
bool lfrfid_carrier_is_psk(lfrfid_carrier_t carrier);

#endif /* LFRFID_CARRIER_H_ */
