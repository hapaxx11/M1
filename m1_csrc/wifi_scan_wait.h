/* See COPYING.txt for license details. */

/*
 * wifi_scan_wait.h
 *
 * Outcome decision for the bounded brain-CD3 async WiFi scan wait in
 * wifi_do_scan() (m1_wifi.c).  Pure logic with no hardware dependencies so
 * host-side unit tests cover every combination.
 *
 * Safety rule encoded here: the scan worker task may outlive its caller's
 * patience (its transaction-mutex wait is not interruptible), and it keeps
 * touching the job context, entries buffer and done-semaphore until it
 * finishes.  Resources may only be freed once the worker has signalled
 * completion — otherwise the job must be orphaned and reaped later.
 *
 * M1 Project
 */

#ifndef WIFI_SCAN_WAIT_H_
#define WIFI_SCAN_WAIT_H_

#include <stdbool.h>

typedef enum {
	/* Worker signalled within the scan bound or the post-cancel drain:
	 * safe to clean up and free all job resources immediately. */
	WIFI_SCAN_WAIT_COMPLETED,
	/* Worker never signalled: it may still touch the job, so the job MUST
	 * stay alive (orphaned) until the worker signals, then be reaped. */
	WIFI_SCAN_WAIT_ORPHANED
} wifi_scan_wait_result_t;

static inline wifi_scan_wait_result_t wifi_scan_wait_decide(bool done_in_bound,
                                                            bool done_in_drain)
{
	return (done_in_bound || done_in_drain)
	       ? WIFI_SCAN_WAIT_COMPLETED
	       : WIFI_SCAN_WAIT_ORPHANED;
}

#endif /* WIFI_SCAN_WAIT_H_ */
