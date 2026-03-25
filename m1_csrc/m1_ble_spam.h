/* See COPYING.txt for license details. */

/*
 * m1_ble_spam.h
 *
 * BLE Beacon Spam — cycles advertising payloads that trigger
 * pairing / proximity notifications on nearby devices.
 *
 * Uses standard ESP32-C6 AT advertising commands (AT+BLEADVPARAM,
 * AT+BLEADVDATA, AT+BLEADVSTART, AT+BLEADVSTOP) — no ESP32 firmware
 * changes required.
 *
 * Supported spam categories:
 *   - Apple iOS  (Continuity Proximity Pair — AirPods, Apple TV, etc.)
 *   - Android    (Google Fast Pair / Nearby Notifications)
 *   - Windows    (Microsoft Swift Pair)
 *   - Samsung    (Galaxy Fast Pair)
 */

#ifndef M1_BLE_SPAM_H_
#define M1_BLE_SPAM_H_

#include <stdint.h>
#include <stdbool.h>

/* Maximum length of a raw advertising data hex string (31 bytes × 2 chars + NUL) */
#define BLE_SPAM_ADV_HEX_MAX   63

typedef struct {
    const char *name;                        /* Short description shown on screen  */
    const char  adv_data[BLE_SPAM_ADV_HEX_MAX]; /* Raw adv payload as ASCII hex       */
} ble_spam_payload_t;

/* Menu entry point */
void ble_spam_run(void);

#endif /* M1_BLE_SPAM_H_ */
