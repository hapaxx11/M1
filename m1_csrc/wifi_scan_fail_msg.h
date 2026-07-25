/* See COPYING.txt for license details. */

/*
 * wifi_scan_fail_msg.h
 *
 * Wording shown on the WiFi access-point scan screen when a scan returns no
 * results.  The message is split across two display rows.  It is defined here
 * as standalone macros (with no hardware dependencies) so the wording is
 * covered by host-side unit tests.
 *
 * M1 Project
 */

#ifndef WIFI_SCAN_FAIL_MSG_H_
#define WIFI_SCAN_FAIL_MSG_H_

/* Reads "AP scan failed. Please try again." across the two rows. */
#define M1_WIFI_SCAN_FAIL_LINE_1  "AP scan failed."
#define M1_WIFI_SCAN_FAIL_LINE_2  "Please try again."

#endif /* WIFI_SCAN_FAIL_MSG_H_ */
