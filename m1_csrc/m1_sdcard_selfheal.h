/* See COPYING.txt for license details. */

#ifndef M1_SDCARD_SELFHEAL_H_
#define M1_SDCARD_SELFHEAL_H_

#include <stdbool.h>
#include <stdint.h>
#include "m1_sdcard.h"

/* Pure decision for the SD detection task's self-heal path.
 *
 * The detection task normally advances only on card-insert/remove edge
 * events pushed into its queue from ISR context. A missed/bounced edge can
 * leave a physically-present card stuck at SD_access_NotReady (unmounted,
 * FatFs driver unlinked) with no further edge to ever re-drive recovery —
 * the SD stays dead until reboot. Called each time the detection task's
 * bounded queue-receive times out with no event pending: if the card is
 * still physically present, still stuck NotReady, and no event is already
 * queued (so a recovery attempt can't pile up on top of a real one already
 * in flight), re-drive the normal detect/recovery path. A genuine card
 * removal (sd_present == false) is left alone — the removal edge itself
 * handles that. */
static inline bool m1_sdcard_should_self_heal(
    bool sd_present,
    S_M1_SDCard_Access_Status status,
    uint32_t queue_messages_waiting)
{
    return sd_present &&
           (status == SD_access_NotReady) &&
           (queue_messages_waiting == 0);
}

#endif /* M1_SDCARD_SELFHEAL_H_ */
