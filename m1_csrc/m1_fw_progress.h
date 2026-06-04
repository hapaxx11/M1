/* See COPYING.txt for license details. */

#ifndef M1_FW_PROGRESS_H_
#define M1_FW_PROGRESS_H_

#include <stddef.h>
#include <stdint.h>

/*
 * fw_update_progress_percent() - pure, host-testable progress calc for the
 * STM32 (Hapax) self firmware-update bar.
 *
 *   total     = full image size in bytes
 *   remainder = bytes still left to flash (full at start, 0 when done)
 *
 * Returns 0..100.  Guards total == 0 and clamps remainder > total so the bar
 * can never report a bogus or out-of-range percentage.
 */
static inline uint8_t fw_update_progress_percent(size_t total, size_t remainder)
{
    if (total == 0)
        return 0;
    if (remainder > total)
        remainder = total;
    return (uint8_t)(((uint64_t)(total - remainder) * 100u) / total);
}

#endif /* M1_FW_PROGRESS_H_ */
