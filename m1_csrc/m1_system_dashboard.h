/* See COPYING.txt for license details. */

/**
 * @file   m1_system_dashboard.h
 * @brief  3-page at-a-glance system status dashboard.
 *
 * Ported from dagnazty/M1_T-1000, adapted for Hapax scene-based architecture.
 */

#ifndef M1_SYSTEM_DASHBOARD_H_
#define M1_SYSTEM_DASHBOARD_H_

/**
 * @brief  Run the system dashboard (blocking delegate).
 *
 * Displays three pages of device status:
 *   Page 1 — Overview: date/time, battery %, voltage, current, uptime
 *   Page 2 — I/O: SD card, USB mode/link, ESP32 status
 *   Page 3 — System: firmware version, bank, orientation, buzzer/LED
 *
 * Navigate with LEFT/RIGHT (or UP/DOWN).  BACK exits.
 */
void system_dashboard_run(void);

#endif /* M1_SYSTEM_DASHBOARD_H_ */
