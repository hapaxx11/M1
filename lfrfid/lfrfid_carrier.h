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
 * Pet / animal microchips in the US use three carrier frequencies:
 *   - 125   kHz : older AVID / FDX-A chips
 *   - 128   kHz : older US chips
 *   - 134.2 kHz : ISO 11784/11785 FDX-B (current standard)
 * All three are swept so that pets tagged on any of them can be read.
 */

#ifndef LFRFID_CARRIER_H_
#define LFRFID_CARRIER_H_

#include <stdint.h>
#include <stdbool.h>

/* Carrier mode for ASK/PSK auto-switching (follows Flipper Zero approach) */
#define LFRFID_CARRIER_SWITCH_MS    2000   /* switch every 2 seconds */
#define LFRFID_CARRIER_ASK_FREQ     125000
#define LFRFID_CARRIER_ASK_DUTY     0.5f
#define LFRFID_CARRIER_ASK_128_FREQ 128000  /* older US pet/animal chips */
#define LFRFID_CARRIER_ASK_128_DUTY 0.5f
#define LFRFID_CARRIER_ASK_134_FREQ 134200  /* ISO 11784/11785 pet chips */
#define LFRFID_CARRIER_ASK_134_DUTY 0.5f
#define LFRFID_CARRIER_PSK_FREQ     62500
#define LFRFID_CARRIER_PSK_DUTY     0.25f

typedef enum {
    LFRFID_CARRIER_ASK,      /* 125 kHz ASK */
    LFRFID_CARRIER_ASK_128,  /* 128 kHz ASK — older US pet/animal chips */
    LFRFID_CARRIER_ASK_134,  /* 134.2 kHz for FDX-B pet/animal chips */
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
