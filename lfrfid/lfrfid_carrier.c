/* See COPYING.txt for license details. */

/*
 * lfrfid_carrier.c
 *
 * Pure-logic LF-RFID carrier-cycling state machine.  Hardware-independent —
 * see lfrfid_carrier.h for the rationale and the US pet-chip frequency list.
 */

#include "lfrfid_carrier.h"

lfrfid_carrier_t lfrfid_carrier_next(lfrfid_carrier_t current)
{
    switch (current) {
    case LFRFID_CARRIER_ASK:
        return LFRFID_CARRIER_ASK_128;
    case LFRFID_CARRIER_ASK_128:
        return LFRFID_CARRIER_ASK_134;
    case LFRFID_CARRIER_ASK_134:
        return LFRFID_CARRIER_PSK;
    case LFRFID_CARRIER_PSK:
    default:
        return LFRFID_CARRIER_ASK;
    }
}

void lfrfid_carrier_params(lfrfid_carrier_t carrier, uint32_t *freq, float *duty)
{
    uint32_t f;
    float d;

    switch (carrier) {
    case LFRFID_CARRIER_ASK_128:
        f = LFRFID_CARRIER_ASK_128_FREQ;
        d = LFRFID_CARRIER_ASK_128_DUTY;
        break;
    case LFRFID_CARRIER_ASK_134:
        f = LFRFID_CARRIER_ASK_134_FREQ;
        d = LFRFID_CARRIER_ASK_134_DUTY;
        break;
    case LFRFID_CARRIER_PSK:
        f = LFRFID_CARRIER_PSK_FREQ;
        d = LFRFID_CARRIER_PSK_DUTY;
        break;
    case LFRFID_CARRIER_ASK:
    default:
        f = LFRFID_CARRIER_ASK_FREQ;
        d = LFRFID_CARRIER_ASK_DUTY;
        break;
    }

    if (freq)
        *freq = f;
    if (duty)
        *duty = d;
}

bool lfrfid_carrier_is_psk(lfrfid_carrier_t carrier)
{
    return carrier == LFRFID_CARRIER_PSK;
}
