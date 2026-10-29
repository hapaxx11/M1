/* See COPYING.txt for license details. */

#ifndef M1_SDCARD_DETECT_H_
#define M1_SDCARD_DETECT_H_

#include <stdbool.h>
#include <stdint.h>

static inline bool m1_sdcard_detect_should_post(bool queue_ready,
                                                uint8_t status_changed)
{
    return queue_ready && status_changed == 0u;
}

#endif /* M1_SDCARD_DETECT_H_ */
