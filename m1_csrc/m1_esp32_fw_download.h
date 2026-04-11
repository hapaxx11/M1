/* See COPYING.txt for license details. */

/*
 * m1_esp32_fw_download.h
 *
 * ESP32-C6 AT firmware download manager.
 * Downloads firmware images (.bin + .md5) from GitHub Releases to SD card
 * for subsequent flashing via the ESP32 Update → Image File → Firmware Update flow.
 *
 * M1 Project
 */

#ifndef M1_ESP32_FW_DOWNLOAD_H_
#define M1_ESP32_FW_DOWNLOAD_H_

/*
 * Entry point for the ESP32 firmware download UI.
 * Called as a blocking delegate from the ESP32 Update sub-menu.
 *
 * Flow:
 *   1. Check WiFi connected → prompt if not
 *   2. Load ESP32 firmware sources (Category: esp32 from fw_sources.txt)
 *   3. Show source selection list
 *   4. Fetch and show available releases
 *   5. Confirm download
 *   6. Download .bin to 0:/ESP32_FW/
 *   7. Download matching .md5 to 0:/ESP32_FW/
 *   8. Show result — user returns to ESP32 Update menu to flash
 */
void esp32_fw_download_start(void);

#endif /* M1_ESP32_FW_DOWNLOAD_H_ */
