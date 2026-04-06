/* See COPYING.txt for license details. */

/*
 * m1_fw_download.h
 *
 * WiFi firmware download manager and UI.
 * Allows users to browse and download firmware images from
 * configured Internet sources (e.g., GitHub Releases).
 *
 * M1 Project
 */

#ifndef M1_FW_DOWNLOAD_H_
#define M1_FW_DOWNLOAD_H_

/*
 * Entry point for the "Download from Internet" firmware update sub-menu.
 * Called as a menu sub_func from the firmware update menu.
 *
 * Flow:
 *   1. Check WiFi connected → prompt if not
 *   2. Show source selection list
 *   3. Fetch and show available releases
 *   4. Confirm download
 *   5. Download with progress display
 *   6. Offer to flash immediately
 */
void fw_download_start(void);

#endif /* M1_FW_DOWNLOAD_H_ */
