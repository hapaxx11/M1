/* See COPYING.txt for license details. */

/**
 * @file  nfc_poller_helpers.c
 * @brief Pure-logic helpers extracted from nfc_poller.c.
 *
 * Hardware-independent — no HAL, RTOS, or RFAL dependencies.
 * Both firmware and host-side unit tests compile this file directly.
 */

#include "nfc_poller_helpers.h"

bool mfc_is_classic_sak(uint8_t sak)
{
    switch (sak) {
    case 0x01: /* Classic 1K (TNP3xxx) */
    case 0x08: /* Classic 1K */
    case 0x09: /* MIFARE Mini */
    case 0x10: /* Plus 2K SL2 */
    case 0x11: /* Plus 4K SL2 */
    case 0x18: /* Classic 4K */
    case 0x19: /* Classic 2K */
    case 0x28: /* Classic EV1 1K */
    case 0x38: /* Classic EV1 4K */
        return true;
    default:
        return false;
    }
}
