/* See COPYING.txt for license details. */

#ifndef BLE_SCAN_STATE_H_
#define BLE_SCAN_STATE_H_

#include <stdbool.h>

static inline bool ble_scan_state_should_start(
    bool continuous,
    bool scan_in_progress,
    bool cancel_requested,
    bool interval_elapsed)
{
    return continuous && !scan_in_progress && !cancel_requested &&
           interval_elapsed;
}

#endif
